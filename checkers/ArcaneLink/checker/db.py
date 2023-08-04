import os
from typing import Optional
from collections import namedtuple
from json import dumps, loads

from redis.asyncio import Redis

from .config import in_dev_mode


_FlagInfo = namedtuple('_FlagInfo', ['flag', 'token', 'uid', 'key', 'flag_idx'])

# Just to have good type hints
class FlagInfo(_FlagInfo):
	flag: str
	token: str
	uid: int
	key: int
	flag_idx: int


async def db_get_flag_info(flag: str) -> Optional[FlagInfo]:
	'''Get info about necessary to retrieve a previously inserted flag
	(PUT_FLAG), so that	its existence can be checked (GET_FLAG) command for
	verification.
	'''
	assert isinstance(flag, str)
	data = await REDIS_DB.get('arcanelink:flag:' + flag)
	if data is None:
		return None

	return FlagInfo(**loads(data))


async def db_set_flag_info(flag: str, token: str, uid: int, key: int, flag_idx: int):
	'''Save info about a flag in the DB, so that it can be used to retrieve it
	later (GET_FLAG). NOTE that this assumes that flags will be unique, so they
	are not saved in the DB in a per-team-id basis
	'''
	assert isinstance(flag, str)
	await REDIS_DB.set('arcanelink:flag:' + flag, dumps({
		'flag': flag,
		'token': token,
		'uid': uid,
		'key': key,
		'flag_idx': flag_idx
	}, separators=(',', ':')))


if in_dev_mode():
	REDIS_DB = Redis(host='127.0.0.1', port=6379, db=0, decode_responses=True)
else:
	REDIS_DB = Redis(
		host=os.environ['REDIS_HOST'],
		port=os.environ['REDIS_PORT'],
		password=os.environ['REDIS_PASSWORD'],
		db=os.environ['REDIS_DB'],
		decode_responses=True
	)
