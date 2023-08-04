import asyncio
from time import monotonic
from json import dumps, loads, JSONDecodeError
from base64 import b64encode, b64decode
from contextlib import suppress
from collections import namedtuple
from typing import Tuple, Optional, Dict, Any, List, Iterable

from .utils import run_command, eprint, chunked


class VirshRuntimeError(RuntimeError):
	pass


class VirshDomainNotFound(VirshRuntimeError):
	pass


class VirshBadDomainStatus(VirshRuntimeError):
	pass


class VirshTimeoutError(VirshRuntimeError):
	pass


class VirshGuestInterface(namedtuple('_VirshGuestInterface', ['name', 'hwaddr', 'ipaddrs'])):
	@staticmethod
	def from_dict(d: Dict) -> 'VirshGuestInterface':
		return VirshGuestInterface(d.get('name'), d.get('hardware-address'), d.get('ip-addresses'))

	def get_ip_addrs(self, of_type: str):
		res = []
		for ip in self.ipaddrs:
			if ip['ip-address-type'] == of_type:
				res.append(ip['ip-address'])

		return res


def domain_start(domain: str):
	res, _, err = run_command('virsh', 'start', domain)

	if res != 0:
		if 'failed to get domain' in err:
			raise VirshDomainNotFound(err)

		if 'already active' in err:
			raise VirshBadDomainStatus(err)

		raise VirshRuntimeError(err)


def domain_status(domain: str) -> str:
	res, out, err = run_command('virsh', 'domstate', domain)

	if res != 0:
		if 'failed to get domain' in err:
			raise VirshDomainNotFound(err)

		raise VirshRuntimeError(err)

	return out.strip()


async def domain_shutdown(domain: str, timeout: float=30):
	res, _, err = run_command('virsh', 'shutdown', domain)

	if res != 0:
		if 'domain is not running' in err:
			pass
		elif 'failed to get domain' in err:
			raise VirshDomainNotFound(err)
		else:
			raise VirshRuntimeError(err)

	deadline = monotonic() + timeout

	while monotonic() < deadline:
		if domain_status(domain) == 'shut off':
			return

		await asyncio.sleep(1)

	raise VirshTimeoutError(f'timed out waiting for domain to shut down: {domain!r}')


async def domain_destroy(domain: str):
	res, _, err = run_command('virsh', 'destroy', domain)

	if res != 0:
		if 'domain is not running' in err:
			pass
		elif 'failed to get domain' in err:
			raise VirshDomainNotFound(err)
		else:
			raise VirshRuntimeError(err)

	while domain_status(domain) != 'shut off':
		await asyncio.sleep(1)


async def qga(domain: str, cmd: str, args: Dict[str, Any]={}) -> str:
	qga_cmd = dumps({'execute': cmd, 'arguments': args}, separators=(',', ':'))

	res, out, err = run_command('virsh', 'qemu-agent-command', domain, qga_cmd)
	if res != 0:
		raise VirshRuntimeError(err)

	return out


async def qga_read_file(domain: str, guest_path: str, timeout: float=30) -> bytes:
	out        = await qga(domain, 'guest-file-open', {'path': guest_path, 'mode': 'rb'})
	handle     = loads(out)['return']
	read_args  = {'handle': handle, 'count': 0x2000}
	close_args = {'handle': handle}
	data       = b''
	deadline   = monotonic() + timeout

	while monotonic() < deadline:
		try:
			out = await qga(domain, 'guest-file-read', read_args)
		except VirshRuntimeError as e:
			await qga(domain, 'guest-file-close', close_args)
			raise e

		try:
			ret: Dict = loads(out).get('return', {})
		except JSONDecodeError as e:
			await qga(domain, 'guest-file-close', close_args)
			raise VirshRuntimeError(e)

		if ret.get('count', 0):
			data += b64decode(ret.get('buf-b64', b''))

		if ret.get('eof', False):
			break

		await asyncio.sleep(0.1)

	await qga(domain, 'guest-file-close', close_args)
	return data


async def qga_write_file(domain: str, guest_path: str, data: bytes, chunk_size: int=0x1000):
	out        = await qga(domain, 'guest-file-open', {'path': guest_path, 'mode': 'wb'})
	handle     = loads(out)['return']
	write_args = {'handle': handle}
	close_args = {'handle': handle}

	for chunk in chunked(data, chunk_size):
		write_args['buf-b64'] = b64encode(chunk).decode()

		try:
			out = await qga(domain, 'guest-file-write', write_args)
		except VirshRuntimeError as e:
			await qga(domain, 'guest-file-close', close_args)
			raise e

		try:
			ret: Dict = loads(out).get('return', {})
		except JSONDecodeError as e:
			await qga(domain, 'guest-file-close', close_args)
			raise VirshRuntimeError(e)

		if ret.get('count', 0) != len(chunk):
			await qga(domain, 'guest-file-close', close_args)
			raise VirshRuntimeError('guest-file-write short write')

	await qga(domain, 'guest-file-flush', close_args)
	await qga(domain, 'guest-file-close', close_args)


async def qga_exec(domain: str, cmd_path: str, *args: str,
		stdin: Optional[str]=None, capture_out: bool=False,
		timeout: float=30) -> Tuple[int, int, Optional[bytes], Optional[bytes]]:
	if not cmd_path.startswith('/'):
		raise ValueError('cmd_path must be an absolute path')

	exec_args: Dict[str,Any] = {'path': cmd_path, 'arg': args}
	if capture_out:
		exec_args['capture-output'] = True
	if stdin:
		exec_args['input-data'] = b64encode(stdin.encode()).decode()

	out      = await qga(domain, 'guest-exec', exec_args)
	pid      = loads(out)['return']['pid']
	qga_args =  {'pid': pid}
	deadline = monotonic() + timeout

	while monotonic() < deadline:
		out = await qga(domain, 'guest-exec-status', qga_args)

		try:
			ret: Dict = loads(out).get('return', {})
		except JSONDecodeError as e:
			raise VirshRuntimeError(e)

		if ret.get('exited', False):
			code   = ret.get('exitcode', None)
			signal = ret.get('signal', None)

			if capture_out:
				stdout = b64decode(ret.get('out-data', b''))
				stderr = b64decode(ret.get('err-data', b''))
				return code, signal, stdout, stderr

			return code, signal, None, None

		await asyncio.sleep(0.1)

	await qga_exec(domain, '/usr/bin/kill', '-9', str(pid))
	raise VirshTimeoutError(f'timed out waiting for command: {cmd_path!r}')


async def qga_get_interfaces(domain: str) -> List[VirshGuestInterface]:
	out = await qga(domain, 'guest-network-get-interfaces')

	try:
		ret: Dict = loads(out).get('return', {})
	except JSONDecodeError as e:
		raise VirshRuntimeError(e)

	return list(map(VirshGuestInterface.from_dict, ret))


async def domain_online(domain: str) -> bool:
	if domain_status(domain) != 'running':
		return False

	res = 1

	try:
		res, *_ = await qga_exec(domain, '/usr/bin/true')
	except VirshRuntimeError as e:
		if 'guest agent is not connected' not in e.args[0]:
			raise e

	return res == 0


async def domain_start_and_wait(domain: str, timeout: int=180, _fail_on_timeout: bool=False):
	if await domain_online(domain):
		return

	with suppress(VirshBadDomainStatus):
		domain_start(domain)

	eprint('Waiting for domain', domain, 'to start')
	deadline = monotonic() + timeout

	while monotonic() < deadline:
		if await domain_online(domain):
			eprint('Domain', domain, 'started')
			return

		await asyncio.sleep(1)

	if _fail_on_timeout:
		raise VirshRuntimeError(f'timed out waiting for domain to start: {domain!r}')

	eprint('Timed out waiting for domain', domain, 'to start, attempting reboot...')

	try:
		await domain_shutdown(domain)
	except VirshTimeoutError:
		await domain_destroy(domain)

	await domain_start_and_wait(domain, timeout, _fail_on_timeout=True)


async def domain_start_and_wait_all(domains: Iterable[str]):
	await asyncio.gather(*map(domain_start_and_wait, domains))
