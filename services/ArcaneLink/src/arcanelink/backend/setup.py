import asyncio
from functools import partial

from passlib.context import CryptContext
from passlib.pwd import genword

from .user import create_user, get_password, set_password
from ..utils import chunked, eprint
from ..virsh import qga_write_file, qga_exec
from ..config import LIBVIRT_DOMAINS, UID_RANGE, MAX_LOGINS_PER_USER


async def install_user_limits(domain: str):
	await qga_write_file(domain, '/etc/security/limits.d/user-limits.conf',
		f'{UID_RANGE.start}: hard maxlogins {MAX_LOGINS_PER_USER}\n'.encode())


async def create_users(domain: str):
	for chunk in chunked(list(UID_RANGE), 8):
		await asyncio.gather(*map(partial(create_user, domain), chunk))


async def run_chpasswd(domain: str, stdin: str):
	ret, *_, err = await qga_exec(domain, '/usr/sbin/chpasswd', '--encrypted',
		stdin=stdin, capture_out=True)
	if ret != 0:
		raise RuntimeError(f'Failed to set passwords in {domain}: {ret=}, {err=}')


async def init_user_passwords():
	ctx = CryptContext('sha256_crypt', sha256_crypt__rounds=1000)
	script = ''

	for uid in UID_RANGE:
		pwd = get_password(uid)

		if pwd is None:
			pwd = genword(128, charset='hex')
			set_password(uid, pwd)

		script += f'user{uid}:{ctx.hash(pwd)}\n'

	await asyncio.gather(*(run_chpasswd(d, script) for d in LIBVIRT_DOMAINS))


async def init_vms():
	eprint('Initializing VMs...')
	await asyncio.gather(*map(install_user_limits, LIBVIRT_DOMAINS))
	await asyncio.gather(*map(create_users, LIBVIRT_DOMAINS))
	await init_user_passwords()
