import sys
import asyncio
from time import monotonic
from traceback import format_exc
from subprocess import Popen, PIPE, TimeoutExpired
from typing import Dict, Any, AnyStr, Optional, Tuple, Callable, Awaitable, Sequence, Union

from jwt import decode
from jwt.exceptions import PyJWTError

from .config import JWT_PUBKEY


def chunked(seq: Sequence, chunk_size: int):
	for i in range(0, len(seq), chunk_size):
		yield seq[i:i + chunk_size]


def format_time(seconds: Union[float,int]) -> str:
	return '{:.0f}m {:.0f}s'.format(*divmod(seconds, 60))


def die(msg: Any=None):
	if msg:
		print(msg)

	sys.stderr.flush()
	sys.stdout.flush()
	sys.stderr.close()
	sys.stdout.close()
	sys.exit(0)


def input_or_die(prompt: str) -> str:
	try:
		data = input(prompt)
	except EOFError:
		die()

	return data


def eprint(*a, **kwa):
	print(*a, **kwa, file=sys.stderr, flush=True)


def run_command(*cmd: AnyStr, stdin: Optional[AnyStr]=None, timeout: Optional[int]=3) -> Tuple[Optional[int], Optional[str], Optional[str]]:
	p = Popen(cmd, stdin=None if stdin is None else PIPE, stdout=PIPE,
		stderr=PIPE, text=True)

	try:
		out, err = p.communicate(stdin, timeout=timeout)
		ret = p.returncode
	except TimeoutExpired:
		eprint('Command timed out:', cmd)
		p.kill()
		raise TimeoutError() from None

	return ret, out, err


async def asyncio_set_interval(task: Callable[[], Awaitable[None]], interval_seconds: int):
	while 1:
		start = monotonic()

		try:
			await task()
		except Exception:
			eprint(f'Exception in scheduled interval task {task.__name__}:\n' + format_exc())

		task_duration = monotonic() - start
		wait = interval_seconds - task_duration
		if wait > 0:
			await asyncio.sleep(wait)


def decode_jwt(token: str, audience: int) -> Optional[Dict]:
	try:
		return decode(token, JWT_PUBKEY, audience=str(audience), algorithms=['ES256'])
	except PyJWTError:
		return None
