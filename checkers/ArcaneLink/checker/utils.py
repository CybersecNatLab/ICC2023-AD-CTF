import sys
from traceback import format_exc
from typing import Set, Any, Hashable, Sequence, Iterator, TypeVar

import requests
from requests.exceptions import ConnectionError, ConnectTimeout, JSONDecodeError

from .config import TOKEN_SERVER_PASSWORD, in_dev_mode, get_token_server
from .checklib import quit, Status


_T = TypeVar('_T')


def eprint(*a, **kwa):
	print(*a, **kwa, file=sys.stderr, flush=True)


_EPRINT_ONCE_CACHE: Set[Hashable] = set()

def eprint_once(key: Hashable, *a, **kwa):
	if key in _EPRINT_ONCE_CACHE:
		return

	eprint(*a, **kwa)
	_EPRINT_ONCE_CACHE.add(key)


def die(debug_msg: Any):
	quit(Status.ERROR, 'Checker internal error', debug_msg)


def chunked(seq: Sequence[_T], chunk_size: int) -> Iterator[Sequence[_T]]:
	for i in range(0, len(seq), chunk_size):
		yield seq[i:i + chunk_size]


def get_new_token(target_team_id: int) -> str:
	target_team_id = 255 if in_dev_mode() else target_team_id

	try:
		resp = requests.get(get_token_server() + '/checker_token', params={
			'password': TOKEN_SERVER_PASSWORD,
			'target': target_team_id
		}, timeout=30)
		raw_data = resp.content
		data = resp.json()
	except (ConnectionError, ConnectTimeout):
		quit(Status.ERROR, 'Token server is unreachable', format_exc())
	except JSONDecodeError:
		quit(Status.ERROR, 'Token server returned an invalid response', repr(raw_data))

	if resp.status_code != 200:
		quit(Status.ERROR, f'Token server returned HTTP {resp.status_code}', data)

	return data
