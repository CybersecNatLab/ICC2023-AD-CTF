from typing import Iterator

from redis import Redis

from ..config import REDIS_HOST, REDIS_PORT


REDIS_DB = Redis(host=REDIS_HOST, port=REDIS_PORT, db=0, decode_responses=True)


def db_get(key: str) -> str:
	assert isinstance(key, str)
	return REDIS_DB.get(key)


def db_set(key: str, value: str):
	assert isinstance(key, str)
	assert isinstance(value, str)
	REDIS_DB.set(key, value)


def db_iter_keys(match: str) -> Iterator[str]:
	assert isinstance(match, str)
	for key in REDIS_DB.scan_iter(match):
		yield key


def db_unlink(key: str):
	assert isinstance(key, str)
	REDIS_DB.unlink(key)


def db_flush():
	REDIS_DB.flushdb()
