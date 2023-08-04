from typing import List, Tuple

from .db import db_get, db_set
from ..virsh import qga_exec, qga_read_file


def get_password(uid: int) -> str:
	assert isinstance(uid, int)
	return db_get(f'password:{uid}')


def set_password(uid: int, password: str):
	assert isinstance(uid, int)
	assert isinstance(password, str)
	db_set(f'password:{uid}', password)


async def user_exists(domain: str, uid: int) -> bool:
	return (await qga_exec(domain, '/usr/bin/id', '-u', str(uid)))[0] == 0


async def useradd(domain: str, uid: int):
	ret, *_, err = await qga_exec(domain, '/usr/sbin/useradd', '-d', '/tmp',
		'-s', '/bin/sh', '-U', '-u', str(uid), f'user{uid}', capture_out=True)
	return ret, err


async def kill_users(domain: str, *uids: int, notify: bool=True):
	if notify:
		script = '\n'.join(f'echo Session timed out! | /usr/bin/write user{uid}' for uid in uids)
		await qga_exec(domain, '/bin/sh', stdin=script)

	await qga_exec(domain, '/usr/bin/pkill', '-KILL', '--uid', ','.join(map(str, uids)))


async def create_user(domain: str, uid: int):
	if await user_exists(domain, uid):
		return

	ret, err = await useradd(domain, uid)
	assert ret == 0, f'Failed to create user {uid} in {domain}: {ret=}, {err=}'


async def list_users(domain: str) -> List[Tuple[int, str]]:
	passwd = await qga_read_file(domain, '/etc/passwd')
	res = []

	for line in passwd.decode().splitlines():
		username, _, uid, *_ = line.split(':')
		res.append((int(uid), username))

	return res
