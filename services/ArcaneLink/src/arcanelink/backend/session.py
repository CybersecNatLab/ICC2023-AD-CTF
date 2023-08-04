import asyncio
from typing import Optional
from secrets import choice
from datetime import datetime
from json import loads, dumps
from collections import namedtuple
from operator import itemgetter
from traceback import format_exc
from typing import Optional, Iterable, Iterator

from .user import kill_users
from .sysv_msg import drain_msgqueue
from .db import db_get, db_set, db_iter_keys, db_unlink
from ..utils import chunked, eprint, die
from ..config import UID_RANGE, LIBVIRT_DOMAINS


SESSION_LOCK = asyncio.Lock()


_Session = namedtuple('_Session', ('token', 'uid', 'expiry'))

class Session(_Session):
	token: str
	uid: int
	expiry: datetime


def get_session(token: str) -> Optional[Session]:
	session = db_get('session:' + token)
	if session is None:
		return None

	data = loads(session)
	data['expiry'] = datetime.fromtimestamp(data['expiry'])
	return Session(**data)


def save_session(session: Session):
	assert isinstance(session, Session)
	db_set('session:' + session.token, dumps({
		'token' : session.token,
		'uid'   : session.uid,
		'expiry': session.expiry.timestamp(),
	}))


def get_session_count() -> int:
	return sum(1 for _ in db_iter_keys('session:*'))


def iter_sessions() -> Iterator[Session]:
	return filter(None, (get_session(k[8:]) for k in db_iter_keys('session:*')))


async def destroy_sessions_locked(sessions: Iterable[Session], notify):
	uids = set(map(itemgetter(1), sessions))
	await asyncio.gather(*(kill_users(d, *uids, notify=notify) for d in LIBVIRT_DOMAINS))

	for s in sessions:
		db_unlink('session:' + s.token)
		drain_msgqueue(s.uid)
		UID_POOL.add(s.uid)


async def destroy_sessions(sessions: Iterable[Session], notify: bool=True):
	async with SESSION_LOCK:
		try:
			await destroy_sessions_locked(sessions, notify)
		except Exception:
			die(f'FATAL: failed to destroy sessions!\nSessions: {sessions}\n{format_exc()}')


def get_or_create_session_locked(token: str, expiry: datetime) -> Optional[Session]:
	session = get_session(token)

	if session is None:
		uid = get_free_uid()
		if uid is None:
			return None

		session = Session(token, uid, expiry)
		save_session(session)

	return session


async def get_or_create_session(token: str, expiry: datetime) -> Optional[Session]:
	async with SESSION_LOCK:
		return get_or_create_session_locked(token, expiry)


async def destroy_expired_sessions(log=False):
	now = datetime.utcnow()
	expired = list(filter(lambda s: s.expiry <= now, iter_sessions()))

	if log and expired:
		eprint('Destroying', len(expired), 'expired sessions...')

	for chunk in chunked(expired, 8):
		await destroy_sessions(chunk)


async def destroy_all_sessions():
	for chunk in chunked(list(iter_sessions()), 8):
		await destroy_sessions(chunk, notify=False)


UID_POOL = set(UID_RANGE) - set(map(itemgetter(1), iter_sessions()))

def get_free_uid() -> Optional[int]:
	if not UID_POOL:
		return None

	uid = choice(list(UID_POOL))
	UID_POOL.discard(uid)
	return uid
