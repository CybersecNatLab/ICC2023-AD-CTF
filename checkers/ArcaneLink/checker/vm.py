import re
import asyncio
from time import monotonic
from traceback import format_exc
from itertools import count
from contextlib import suppress
from typing import Tuple, Iterable, Optional, NoReturn
from base64 import b64encode
from gzip import compress

from pwn import remote, context

from .checklib import quit, Status
from .timer import timer_start, timer_stop
from .utils import eprint_once, die
from .aes_wrap import dh_wrap, unwrap
from .qemu_req import ReqCommand, ReqError
from .config import SERVICE_PORT, PLAYGROUND_DIR
from .utils import eprint, chunked


BAD_ASCII_CHARS = '\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f'


CmdResult = Tuple[ReqError,Optional[bytes]]
CmdIntResult = Tuple[ReqError,Optional[int]]
CmdBoolResult = Tuple[ReqError,Optional[bool]]

class VM:
	team_ip      : str
	token        : str
	arch         : str
	uid          : Optional[int]
	remote       : Optional[remote]
	in_playground: bool
	retry_count  : int
	_vm_name     : str
	_prompt      : bytes


	def __init__(self, team_ip: str, token: str, arch: str):
		self.team_ip       = team_ip
		self.token         = token
		self.arch          = arch
		self.uid           = None
		self.remote        = None
		self.in_playground = False
		self.retry_count   = 3
		self._vm_name      = f'ic3-bookworm-{self.arch}'
		self._prompt       = f'@{self._vm_name}'.encode()


	@property
	def id(self) -> str:
		return f'{self.arch} {hex(id(self))}'


	async def __bulletproof_recvuntil(self, what: bytes, drop: bool=False, timeout: float=30) -> bytes:
		'''
		Pwntools' recvuntil() timeout handling is broken, DO NOT use it. Use
		recvn() in a loop manually instead, which seems to be safe.

		Also, never use timeout=X with recvline(), recvuntil() and similar
		funcs when the connection is AES-wrapped. Pwntools can decide to call
		remote.unrecv(stuff) while waiting for the data and that will desync
		the AES stream. To avoid this, manually receive one byte at a time
		using recvn(1, timeout=X) instead, so pwntools should not unrecv().
		'''
		data = b''
		remaining = timeout
		deadline  = monotonic() + timeout

		for i in count(1):
			data += self.remote.recvn(1, remaining)
			remaining = deadline - monotonic()
			if data.endswith(what):
				break

			if remaining <= 0:
				raise TimeoutError(f'timed out waiting for {what!r} ({self.id})')

			if i % 10:
				await asyncio.sleep(0)

		return data[:-len(what)] if drop else data


	async def __bulletproof_recvline(self, keepends: bool=True, timeout: float=30) -> bytes:
		return await self.__bulletproof_recvuntil(b'\n', not keepends, timeout)


	async def __wait_prompt(self, timeout: float=30):
		return await self.__bulletproof_recvuntil(self._prompt, timeout=timeout)


	async def command(self, cmd: str, shell_wait: Optional[float]=None):
		if shell_wait is not None:
			await self.__bulletproof_recvuntil(b'$ ', timeout=shell_wait)
		else:
			await self.__bulletproof_recvuntil(b'$ ')

		self.remote.sendline(cmd.encode())


	async def command_output(self, cmd: str) -> bytes:
		await self.command(cmd)
		output = await self.__wait_prompt()

		if b'\n' not in output:
			# This will happen if we expect output but the program was somehow
			# killed (e.g. SIGKILL for exceeding cgroup memory limit), which is
			# probably not good and means something in the VM changed.
			# Nonetheless, let the caller handle it.
			return b''

		return output[:output.rfind(b'\n') + 1]


	async def command_retval(self, cmd: str) -> int:
		await self.command(cmd)
		return int(await self.command_output('echo $?'))


	async def command_retval_and_output(self, cmd: str) -> Tuple[int,bytes]:
		out = await self.command_output(cmd)
		return int(await self.command_output('echo $?')), out


	async def upload_text_file(self, local_path: str, remote_path: str):
		assert "'" not in remote_path

		try:
			with open(local_path, 'r', encoding='ascii') as f:
				text = f.read()

				if '\nEOF' in text:
					die(f'Trying to upload a file containing a bash EOF marker as text: {local_path}')

				if '\t' in text:
					eprint_once(local_path, 'WARNING:', local_path, 'contains tabs, replacing them before uploading')
					text = text.replace('\t', '    ')
		except UnicodeDecodeError:
			die(f'Trying to upload a non-ASCII file as text: {local_path}')

		assert not any(c in text for c in BAD_ASCII_CHARS)
		await self.command(f"cat >'{remote_path}' <<'EOF'\n{text}\nEOF")


	async def upload_binary_file(self, local_path: str, remote_path: str, line_length: int=256, chmod_x: bool=True):
		assert "'" not in remote_path

		with open(local_path, 'rb') as f:
			data = b64encode(compress(f.read()))

		lines = b'\n'.join(chunked(data, line_length)).decode()
		await self.command(f"base64 -d <<'EOF' | zcat >'{remote_path}'\n{lines}\nEOF")

		if chmod_x:
			await self.command(f"chmod +x '{remote_path}'")


	def exit_and_close(self):
		with suppress(EOFError):
			if self.in_playground:
				self.remote.send(ReqCommand.WRAPPER_EXIT.to_bytes())
				unwrap(self.remote)

			with context.local(log_level='ERROR'):
				self.remote.sendline(b'exit')
				self.remote.close()


	def quit(self, exit_status: Status, player_msg: str, debug_msg: str) -> NoReturn:
		self.exit_and_close()
		quit(exit_status, player_msg, debug_msg)


	async def start(self):
		if self.remote:
			return

		with context.local(log_level='ERROR'):
			try:
				self.remote = remote(self.team_ip, SERVICE_PORT)
			except Exception:
				quit(Status.DOWN, 'Error connecting (backend down?)',
					'Error connecting to the service: ' + format_exc())

		prompt = await self.__bulletproof_recvuntil(b'Token: ')
		if not prompt.startswith(b'Token: '):
			quit(Status.DOWN, 'Error or timeout connecting (backend down?)',
				f'Backend down? {prompt=}')

		timer_start(f'VM start ({self.id})')
		self.remote.sendline(self.token.encode())

		prompt = await self.__bulletproof_recvline(keepends=False)
		if not prompt.startswith(b'Session expires in'):
			# Match some known errors returned by the server to help players
			# understand what went wrong, but avoid leaking anything else
			if re.match(rb'Backend (error:.+|is unreachable|returned an invalid response)$', prompt):
				player_msg = prompt.decode()
			else:
				player_msg = 'Error authenticating with the service, absent/unexpected prompt'

			quit(Status.DOWN, player_msg, f'Token refused? {prompt=} {self.token=}')

		await self.__bulletproof_recvuntil(b'Select VM: ')
		self.remote.sendline(self._vm_name.encode())
		await self.__wait_prompt(60)
		timer_stop(f'VM start ({self.id})')

		self.uid = int(await self.command_output('id -u'))
		eprint(f'VM {self.id} assigned uid: {self.uid}')


	async def start_playground(self, precompiled: bool=True):
		await self.start()
		if self.in_playground:
			return

		pre = ' pre-compiled' if precompiled else ''
		timer_start(f'Playground start ({self.id}{pre})')

		playgruond_binary = PLAYGROUND_DIR / f'playground-{self.arch}.bin'
		if not playgruond_binary.is_file():
			die(f'Playground binary not found: {playgruond_binary}')

		with playgruond_binary.open('rb') as f:
			code = f.read()

		if precompiled:
			eprint('Uploading pre-compiled AES wrapper')
			await self.upload_binary_file(f'aes_wrapper/aes_wrap_remote-{self.arch}', 'wrapper')
		else:
			eprint('Compiling AES wrapper inside guest')
			await self.upload_text_file('aes_wrapper/aes_wrap_remote.c', 'wrapper.c')

			res, out = await self.command_retval_and_output('gcc -o wrapper wrapper.c -lcrypto')
			if res != 0:
				player_out = ''
				with suppress(UnicodeDecodeError):
					player_out = ': ' + out.decode()

				self.quit(Status.DOWN, f'GCC does not work ({self.arch} VM){player_out}',
					f'GCC KO ({self.id}): {res=} {out=}')

		await self.command('./wrapper', shell_wait=60)

		peer_key_pem = (await self.__bulletproof_recvuntil(b'-----BEGIN PUBLIC KEY-----'))[-26:]
		peer_key_pem += await self.__bulletproof_recvuntil(b'-----END PUBLIC KEY-----')
		self.remote.clean()
		dh_wrap(self.remote, peer_key_pem)

		self.remote.send(len(code).to_bytes(4, 'big'))
		self.remote.send(code)

		timer_stop(f'Playground start ({self.id}{pre})')
		self.in_playground = True


	async def exit_playground(self):
		assert self.in_playground
		self.remote.send(ReqCommand.WRAPPER_EXIT.to_bytes())
		unwrap(self.remote)
		await self.__wait_prompt()
		self.in_playground = False


	async def swap_playground(self):
		if self.in_playground:
			await self.exit_playground()
		else:
			await self.start_playground()


	async def __retry_playground_wait_ready(self, timeout):
		assert self.in_playground

		if self.retry_count == 0:
			die(f'Exceeded max attempts for playground READY ({self.id}), giving up :(')

		eprint(f'Timed out waiting for playground READY ({self.id}), retrying...')

		with context.local(log_level='ERROR'):
			self.remote.close()

		self.remote = None
		self.in_playground = False
		self.retry_count -= 1

		await self.start_playground()
		return await self.__playground_wait_ready(timeout)


	async def __playground_wait_ready(self, timeout: float=20):
		assert self.in_playground
		timed_out = False

		try:
			ready = await self.__bulletproof_recvline(False, timeout)
		except TimeoutError:
			timed_out = True

		if timed_out:
			# Did not receive a newline before timeout, restart playground and
			# retry as long as we can
			return await self.__retry_playground_wait_ready(timeout)

		if ready != b'READY':
			# Differentiate this case for debugging purposes: playground printed
			# some garbage instead of the expected "READY\n"
			if self.retry_count == 0:
				die(f'AES wrapper not ready: {ready!r} ({self.id})')

			return await self.__retry_playground_wait_ready(timeout)


	async def __playground_check_command(self, sent_cmd: ReqCommand) -> ReqError:
		res = await self.__bulletproof_recvline(False)
		if res != b'OK' and not res.startswith(b'ERROR:'):
			if res.startswith(b'UNEXPECTED STATUS:'):
				safe_res = ': ' + res.decode()
			else:
				safe_res = ''

			self.quit(Status.DOWN, f'ArcaneLink command failed ({self.arch} VM){safe_res}',
				f'PLAYGROUND cmd ripperoni ({self.id}): {sent_cmd} -> {res=}')

		err = ReqError.from_playground(res)
		if err is ReqError.BAD_VALUE:
			# Mask this to avoid infoleak. Do not die() for internal error (even
			# though it *could* be), otherwise players would just be able to
			# make the playground return a bad value and keep 100% SLA).
			eprint(f'PLAYGROUND returned bad error value ({self.id}): {res=}')
			return ReqError.UNKNOWN

		return err


	async def _playground_peek_msg(self, idx: int, uid: int, key: int, allow_error: bool) -> CmdResult:
		await self.__playground_wait_ready()

		self.remote.send(ReqCommand.PEEK_MSG.to_bytes())
		self.remote.send(idx.to_bytes(4, 'big'))
		self.remote.send(uid.to_bytes(4, 'big'))
		self.remote.send(key.to_bytes(8, 'big'))

		err = await self.__playground_check_command(ReqCommand.PEEK_MSG)
		if not allow_error and err is not ReqError.OK:
			self.quit(Status.DOWN, f'Msg peek failed ({self.arch} VM): {err:s}',
				f'PLAYGROUND cmd failed ({self.id}): CMD_PEEK_MSG -> {err=}')

		if err is not ReqError.OK:
			return err, None
		return err, await self.__bulletproof_recvline(False)


	async def _playground_push_msg(self, mid: int, msg: bytes, allow_error: bool) -> ReqError:
		await self.__playground_wait_ready()

		self.remote.send(ReqCommand.PUSH_MSG.to_bytes())
		self.remote.send(mid.to_bytes(4, 'big'))
		self.remote.sendline(msg)

		err = await self.__playground_check_command(ReqCommand.PUSH_MSG)
		if not allow_error and err is not ReqError.OK:
			self.quit(Status.DOWN, f'Msg push failed ({self.arch} VM): {err:s}',
				f'PLAYGROUND cmd failed ({self.id}): CMD_PUSH_MSG -> {err=}')

		return err


	async def _playground_pop_msg(self, mid: int, allow_error: bool) -> CmdResult:
		await self.__playground_wait_ready()

		self.remote.send(ReqCommand.POP_MSG.to_bytes())
		self.remote.send(mid.to_bytes(4, 'big'))

		err = await self.__playground_check_command(ReqCommand.POP_MSG)
		if not allow_error and err is not ReqError.OK:
			self.quit(Status.DOWN, f'Msg pop failed ({self.arch} VM): {err:s}',
				f'PLAYGROUND cmd failed ({self.id}): CMD_POP_MSG -> {err=}')

		if err is not ReqError.OK:
			return err, None
		return err, await self.__bulletproof_recvline(False)


	async def _playground_gen_key(self, allow_error: bool) -> CmdIntResult:
		await self.__playground_wait_ready()

		self.remote.send(ReqCommand.GEN_KEY.to_bytes())

		err = await self.__playground_check_command(ReqCommand.GEN_KEY)
		if not allow_error and err is not ReqError.OK:
			self.quit(Status.DOWN, f'Genkey failed ({self.arch} VM): {err:s}',
				f'PLAYGROUND cmd failed ({self.id}): CMD_GEN_KEY -> {err=}')

		if err is not ReqError.OK:
			return err, None
		return err, int.from_bytes(self.remote.recvn(8), 'big')


	async def _playground_chk_key(self, uid: int, key: int, allow_error: bool) -> CmdBoolResult:
		await self.__playground_wait_ready()

		self.remote.send(ReqCommand.CHK_KEY.to_bytes())
		self.remote.send(uid.to_bytes(4, 'big'))
		self.remote.send(key.to_bytes(8, 'big'))

		err = await self.__playground_check_command(ReqCommand.CHK_KEY)
		if not allow_error and err is not ReqError.OK:
			self.quit(Status.DOWN, f'Chkkey failed ({self.arch} VM): {err:s}',
				f'PLAYGROUND cmd failed ({self.id}): CMD_CHK_KEY -> {err=}')

		if err is not ReqError.OK:
			return err, None
		return err, int.from_bytes(self.remote.recvn(1), 'big') == 1


	async def __cli_run_command(self, cmd: str) -> ReqError:
		await self.command(cmd)

		res = await self.__bulletproof_recvline(False)
		if res != b'OK' and not res.startswith(b'ERROR:'):
			self.quit(Status.DOWN, f'arcane-cli failed ({self.arch} VM): {res.decode()}',
				f'CLI cmd ripperoni ({self.id}): {cmd} -> {res=}')

		return ReqError.from_cli(res)


	async def _cli_peek_msg(self, idx: int, uid: int, key: int, allow_error: bool) -> CmdResult:
		cmd = f'arcane-cli peek {uid} {idx} {key:#x}'
		err = await self.__cli_run_command(cmd)

		if not allow_error and err is not ReqError.OK:
			self.quit(Status.DOWN, f'Msg peek failed ({self.arch} VM): {err:s}',
				f'CLI cmd failed ({self.id}): {cmd} -> {err=}')

		if err is not ReqError.OK:
			return err, None
		return err, await self.__bulletproof_recvline(False)


	async def _cli_push_msg(self, mid: int, msg: bytes, allow_error: bool) -> ReqError:
		cmd = f'arcane-cli push {mid} {msg.decode()}'
		err = await self.__cli_run_command(cmd)

		if not allow_error and err is not ReqError.OK:
			self.quit(Status.DOWN, f'Msg push failed ({self.arch} VM): {err:s}',
				f'CLI cmd failed ({self.id}): {cmd} -> {err=}')

		return err


	async def _cli_pop_msg(self, mid: int, allow_error: bool) -> CmdResult:
		cmd = f'arcane-cli pop {mid}'
		err = await self.__cli_run_command(cmd)

		if not allow_error and err is not ReqError.OK:
			self.quit(Status.DOWN, f'Msg pop failed ({self.arch} VM): {err:s}',
				f'CLI cmd failed ({self.id}): {cmd} -> {err=}')

		if err is not ReqError.OK:
			return err, None
		return err, await self.__bulletproof_recvline(False)


	async def _cli_gen_key(self, allow_error: bool) -> CmdIntResult:
		cmd = 'arcane-cli genkey'
		err = await self.__cli_run_command(cmd)

		if not allow_error and err is not ReqError.OK:
			self.quit(Status.DOWN, f'Genkey failed ({self.arch} VM): {err:s}',
				f'CLI cmd failed ({self.id}): {cmd} -> {err=}')

		if err is not ReqError.OK:
			return err, None

		key = await self.__bulletproof_recvline(keepends=False)
		if not key.startswith(b'Key: '):
			self.quit(Status.DOWN, f'Genkey failed: invalid CLI output ({self.arch} VM)',
				f'CLI cmd invalid output ({self.id}): {cmd} -> {key=}')

		try:
			key_value = int(key[5:], 0)
		except ValueError:
			self.quit(Status.DOWN, f'Genkey failed: invalid integer returned by CLI ({self.arch} VM)',
				f'CLI cmd invalid integer ({self.id}): {cmd} -> {key=}')

		return err, key_value


	async def _cli_chk_key(self, uid: int, key: int, allow_error: bool) -> CmdBoolResult:
		cmd = f'arcane-cli chkkey {uid} 0x{key:x}'
		err = await self.__cli_run_command(cmd)

		if not allow_error and err is not ReqError.OK:
			self.quit(Status.DOWN, f'Chkkey failed ({self.arch} VM): {err:s}',
				f'CLI cmd failed ({self.id}): {cmd} -> {err=}')

		if err is not ReqError.OK:
			return err, None

		res = await self.__bulletproof_recvline(keepends=False)
		if res not in (b'Invalid key', b'Key is valid'):
			self.quit(Status.DOWN, f'Chkkey failed: invalid CLI output ({self.arch} VM)',
				f'CLI cmd invalid output ({self.id}): {cmd} -> {res=}')

		return err, res == b'Key is valid'


	async def peek_msg(self, idx: int, uid: int, key: int, allow_error: bool=False) -> CmdResult:
		if self.in_playground:
			return await self._playground_peek_msg(idx, uid, key, allow_error)
		return await self._cli_peek_msg(idx, uid, key, allow_error)


	async def push_msg(self, mid: int, msg: bytes, allow_error: bool=False) -> ReqError:
		if self.in_playground:
			return await self._playground_push_msg(mid, msg, allow_error)
		return await self._cli_push_msg(mid, msg, allow_error)


	async def push_all_msgs(self, msgs: Iterable[Tuple[int,bytes]]):
		for mid, msg in msgs:
			await self.push_msg(mid, msg)


	async def pop_msg(self, mid: int, allow_error: bool=False) -> CmdResult:
		if self.in_playground:
			return await self._playground_pop_msg(mid, allow_error)
		return await self._cli_pop_msg(mid, allow_error)


	async def gen_key(self, allow_error: bool=False) -> CmdIntResult:
		if self.in_playground:
			return await self._playground_gen_key(allow_error)
		return await self._cli_gen_key(allow_error)


	async def chk_key(self, uid: int, key: int, allow_error: bool=False) -> CmdBoolResult:
		if self.in_playground:
			return await self._playground_chk_key(uid, key, allow_error)
		return await self._cli_chk_key(uid, key, allow_error)


	async def flush_all_msgs(self):
		while 1:
			err, _ = await self.pop_msg(0, allow_error=True)
			if err is ReqError.NOMSG:
				break

			if err is not ReqError.OK:
				self.quit(Status.DOWN, f'Msg pop failed ({self.arch} VM): {err:s}',
					f'Msg pop failed while flushing all msgs ({self.id}): {err=}')
