#!/usr/bin/env python3
#
# Checker main, runs in the parent directory of this one.
#

import os
import re
import sys
import asyncio
from typing import List
from pathlib import Path
from functools import partial
from traceback import format_exc
from operator import itemgetter
from contextlib import suppress

os.environ['PWNLIB_NOTERM'] = '1'
os.environ['PWNLIB_STDERR'] = '1'
from pwn import args

from .vm import VM
from .rng import RNG
from .utils import die
from .qemu_req import ReqError
from .db import db_get_flag_info, db_set_flag_info
from .utils import eprint, get_new_token
from .config import SERVICE_NAME, in_dev_mode
from .timer import timer_start, timer_stop
from .checklib import Status, Action, get_data, quit, post_flag_id


async def connect(team_ip: str, token: str, arch: str,
		start_playground: bool=False, precompiled: bool=True) -> VM:
	vm = VM(team_ip, token, arch)
	await vm.start()

	if start_playground:
		await vm.start_playground(precompiled)

	return vm


async def put_flag_internal(rng: RNG, vm: VM, flag: str, token: str, team_id: int):
	assert '\n' not in flag
	assert len(flag) < 0x500

	# Pop everything from the queue, needed in case we are retrying after a
	# timeout/EOF and there are still messages in the queue from a previous
	# failed attempt *insert skull emoji and gun emoji*.
	await vm.flush_all_msgs()

	# Some random messages, then the flag, then some more random messages.
	# Remember the index of the flag for later use (MSG_PEEK in GET_FLAG).
	n_before  = flag_idx = rng.randrange(1, 5)
	n_after   = rng.randrange(1, 5)
	messages  = [(rng.random_msgid(), rng.random_msg()) for _ in range(n_before)]
	messages += [(rng.random_msgid(), flag.encode())]
	messages += [(rng.random_msgid(), rng.random_msg()) for _ in range(n_after)]

	# Generate a key for later use (PEEK_MSG) and push all messages
	_, key = await vm.gen_key()
	await vm.push_all_msgs(messages)
	await db_set_flag_info(flag, token, vm.uid, key, flag_idx)

	if not in_dev_mode():
		post_flag_id(SERVICE_NAME, team_id, {'uid': vm.uid})

	quit(Status.OK, 'OK')


async def put_flag(team_id: int, team_ip: str, flag: str):
	seed = os.environ.get('SEED', os.urandom(16).hex())
	eprint('PUT_FLAG RNG seed:', seed)

	rng = RNG(seed)
	token = get_new_token(team_id)
	vm = None

	try:
		# Randomly pick the VM to use. Compile the playground inside the guest
		# 1/4 of the times, send pre-compiled binary 3/4 of the times.
		vm = await connect(team_ip, token, rng.choice(['i386', 'arm64']), True, rng.chance(3, 4))
		await put_flag_internal(rng, vm, flag, token, team_id)
		return
	except EOFError as e:
		eprint('EOFError with playground' + (f': {e}' if str(e) else ''))
	except TimeoutError as e:
		eprint('TimeoutError with playground' + (f': {e}' if str(e) else ''))

	eprint('Retrying forcing pre-compiled wrapper...')
	await asyncio.sleep(1)

	if vm is not None:
		vm.exit_and_close()

	vm = await connect(team_ip, token, rng.choice(['i386', 'arm64']), False)
	await put_flag_internal(rng, vm, flag, token, team_id)


async def get_flag_internal(vm: VM, flag: str, flag_idx: int, uid: int, key: int):
	# Check that the flag is retrievable from this VM
	expected = flag.encode()
	_, actual = await vm.peek_msg(flag_idx, uid, key)

	if actual != expected:
		quit(Status.DOWN, f'Flag mismatch, read wrong flag from {vm.arch} VM',
			f'Flag mismatch ({vm.id}): {expected=} VS {actual=}')

	quit(Status.OK, 'OK')


async def get_flag(team_ip: str, flag: str):
	info = await db_get_flag_info(flag)
	if info is None:
		die(f'Flag not found in DB: {flag}')
		return

	seed = os.environ.get('SEED', os.urandom(16).hex())
	eprint('GET_FLAG RNG seed:', seed)

	rng = RNG(seed)
	vm = None

	try:
		# Randomly pick the VM to use. Compile the playground inside the guest
		# 1/4 of the times, send pre-compiled binary 3/4 of the times.
		vm = await connect(team_ip, info.token, rng.choice(['i386', 'arm64']), True, rng.chance(3, 4))
		await get_flag_internal(vm, flag, info.flag_idx, info.uid, info.key)
		return
	except EOFError as e:
		eprint('EOFError with playground' + (f': {e}' if str(e) else ''))
	except TimeoutError as e:
		eprint('TimeoutError with playground' + (f': {e}' if str(e) else ''))

	eprint('Retrying forcing pre-compiled wrapper...')
	await asyncio.sleep(1)

	if vm is not None:
		vm.exit_and_close()

	vm = await connect(team_ip, info.token, rng.choice(['i386', 'arm64']), True)
	await get_flag_internal(vm, flag, info.flag_idx, info.uid, info.key)


async def check_sla_push_pop(rng: RNG, *vms: VM):
	'''PUSH + POP (same vm, same uid)'''
	vm     = rng.choice(vms)
	pushed = rng.random_msg()
	mid    = rng.random_msgid()

	# Push and pop one
	await vm.push_msg(mid, pushed)
	_, popped = await vm.pop_msg(mid)

	if pushed != popped:
		quit(Status.DOWN, f'Msg pop mismatch ({vm.arch} VM)',
			f'check_sla_push_pop({vm.id}): mismatch: {pushed=} VS {popped=}')

	# Pop again, should get NOMSG
	err, _ = await vm.pop_msg(mid, allow_error=True)
	if err is not ReqError.NOMSG:
		quit(Status.DOWN, f'Unexpected error when popping non-existent message ({vm.arch} VM): {err:s}',
			f'check_sla_push_pop(): message should not exist: {err=} {popped=} ({vm.id})')

	eprint('check_sla_push_pop(): OK')


async def check_sla_push_peek_pop_single_vm(rng: RNG, *vms: VM):
	'''PUSH + GENKEY + PEEK + POP (same vm, same uid)'''
	vm = rng.choice(vms)
	_, key = await vm.gen_key()

	# Generate between 2 and 5 random messages
	messages = [(idx, rng.random_msgid(), rng.random_msg()) for idx in range(rng.randrange(2, 6))]
	await vm.push_all_msgs(map(itemgetter(1, 2), messages))

	# Peek all messages in random order, check that they match what we pushed
	rng.shuffle(messages)

	for idx, mid, pushed in messages:
		_, peeked = await vm.peek_msg(idx, vm.uid, key)

		if pushed != peeked:
			quit(Status.DOWN, f'Msg peek mismatch ({vm.arch} VM)',
				f'check_sla_push_peek_pop_single_vm(): msg {idx=} mismatch: {pushed=} VS {peeked=} ({vm.id})')

	# Pop all messages in random order, check that they match what we pushed
	rng.shuffle(messages)

	for idx, mid, pushed in messages:
		_, popped = await vm.pop_msg(mid)

		if pushed != popped:
			quit(Status.DOWN, f'Msg pop mismatch ({vm.arch} VM)',
				f'check_sla_push_peek_pop_single_vm(): msg {idx=} mismatch: {pushed=} VS {popped=} ({vm.id})')

	eprint('check_sla_push_peek_pop_single_vm(): OK')


async def check_sla_push_peek_pop_multi_vm(rng: RNG, *vms: VM):
	'''PUSH + GENKEY + PEEK (different vms, same uid)'''
	vm1, vm2 = rng.sample(vms, 2)

	_, key = await vm1.gen_key()
	pushed = rng.random_msg()
	mid    = rng.random_msgid()

	await vm1.push_msg(mid, pushed)
	_, peeked = await vm2.peek_msg(0, vm1.uid, key)

	if pushed != peeked:
		quit(Status.DOWN, f'Msg peek mismatch ({vm1.arch} push, {vm2.arch} peek)',
			f'check_sla_push_peek_pop_multi_vm(): mismatch: {pushed=} ({vm1.id}) VS {peeked=} ({vm2.id})')

	_, popped = await vm2.pop_msg(mid)
	if pushed != popped:
		quit(Status.DOWN, f'Msg peek mismatch ({vm1.arch} push, {vm2.arch} pop)',
			f'check_sla_push_peek_pop_multi_vm(): mismatch: {pushed=} ({vm1.id}) VS {popped=} ({vm2.id})')

	eprint('check_sla_push_peek_pop_multi_vm(): OK')


async def check_sla_peek_multi_uid(rng: RNG, *vms: VM):
	'''PUSH + PEEK + POP (different vms, different uids). Special case that
	requires the caller to get a new token for a different UID.
	'''
	assert len(vms) == 2
	vm1, vm2 = rng.sample(vms, 2)
	assert vm1.uid != vm2.uid

	# Push a message from vm1, peek it from vm2
	_, key = await vm1.gen_key()
	pushed = rng.random_msg()
	mid    = rng.random_msgid()

	await vm1.push_msg(mid, pushed)
	_, peeked = await vm2.peek_msg(0, vm1.uid, key)

	if pushed != peeked:
		quit(Status.DOWN, f'Msg peek mismatch (different users)',
			f'check_sla_peek_multi_uid(): mismatch: {pushed=} ({vm1.id} {vm1.uid}) VS {peeked=} ({vm2.id} {vm2.uid})')

	_, popped = await vm1.pop_msg(mid)
	if pushed != popped:
		quit(Status.DOWN, f'Msg pop mismatch ({vm1.arch} VM)',
			f'check_sla_peek_multi_uid(): mismatch: {pushed=} VS {popped=} ({vm1.id})')

	eprint('check_sla_peek_multi_uid(): OK')


async def check_sla_chkkey(rng: RNG, *vms: VM):
	'''GENKEY + CHKKEY (same vm and different vm, same uid and different uid)'''
	vm1, vm2, vm3 = vms
	assert vm1.uid == vm2.uid
	assert vm2.uid != vm3.uid

	# Shuffle only the first two
	shuffled_vms: List[VM] = rng.sample((vm1, vm2), 2)
	shuffled_vms.append(vm3)

	# Generate a key from vm1, check it from all vms
	_, key = await vm1.gen_key()
	uid = vm1.uid

	for vm in shuffled_vms:
		_, good = await vm.chk_key(uid, key)
		if not good:
			quit(Status.DOWN, f'Check key failed (good key reported as invalid)',
				f'check_sla_chkkey(): {key=:#x} should have matched ({vm.id})')

	# Also check a bad  key with a single bit flipped
	badkey = key ^ (1 << rng.randrange(64))

	for vm in shuffled_vms:
		_, good = await vm.chk_key(uid, badkey)
		if good:
			quit(Status.DOWN, f'Check key failed (bad key reported as valid)',
				f'check_sla_chkkey(): {badkey=:#x} should NOT have matched ({vm.id})')

	eprint('check_sla_chkkey(): OK')


async def check_sla_bad_commands(rng: RNG, *vms: VM):
	'''Bad commands that should fail with different errors (same vm, same uid)'''
	vm = rng.choice(vms)

	# Pop when no messages are present (mid=0 pops the first in the queue)
	err, popped = await vm.pop_msg(0, allow_error=True)
	if err is not ReqError.NOMSG:
		quit(Status.DOWN, f'Unexpected result when popping with no messages: {err:s}',
			f'check_sla_bad_commands(): bad POP: got {err} instead of {ReqError.NOMSG}, {popped=} ({vm.id})')

	# Create a key to test peek/chkkey
	_, key = await vm.gen_key()

	# Peek with no messages
	err, peeked = await vm.peek_msg(0, vm.uid, key, allow_error=True)
	if err is not ReqError.NOMSG:
		quit(Status.DOWN, f'Unexpected result when peeking with no messages: {err:s}',
			f'check_sla_bad_commands(): bad PEEK: got {err} instead of {ReqError.NOMSG}, {peeked=} ({vm.id})')

	# Peek with invalid UID
	err, peeked = await vm.peek_msg(0, 9999, key, allow_error=True)
	if err is not ReqError.INVALID_UID:
		quit(Status.DOWN, f'Unexpected result when peeking with invalid UID (9999): {err:s}',
			f'check_sla_bad_commands(): bad PEEK: got {err} instead of {ReqError.INVALID_UID}, {peeked=} ({vm.id})')

	# Peek with invalid key
	err, peeked = await vm.peek_msg(0, vm.uid, key ^ 0xf0f0f0f0f0f0f0f0, allow_error=True)
	if err is not ReqError.INVALID_KEY:
		quit(Status.DOWN, f'Unexpected result when peeking with invalid key: {err:s}',
			f'check_sla_bad_commands(): bad PEEK: got {err} instead of {ReqError.INVALID_KEY}, {peeked=} ({vm.id})')

	eprint('check_sla_bad_commands(): OK')


async def check_sla_system_resources(rng: RNG, *vms: VM):
	'''Miscellaneous checks to ensure shells spawned in the VMs have enough
	resources to do normal stuff and also run potential exploits. Players
	shouldn't be able to deny exploitation attempts just by limiting system
	resources.

	No need to check if GCC works as this is already done in {GET,PUT}_FLAG.
	'''
	ops = list((vm, i) for vm in vms for i in range(4))
	rng.shuffle(ops)

	for vm, op in ops:
		if op == 0:
			# Check that we have at least 2 CPUs available in the jailed shell
			out = await vm.command_output('taskset -pc $$')
			if re.match(rb'^pid \d+\'s current affinity list: \d+,\d+$', out) is None:
				quit(Status.DOWN, f'Shell available system resources have changed ({vm.arch} VM)',
					f'check_sla_system_resources(): taskset reported bad CPU affinity mask: {out=} ({vm.id})')
		elif op == 1:
			# Check that we can write files of 16MiB in the current directory
			fname = rng.random_filename()
			out = await vm.command_output(f'dd if=/dev/zero of={fname} bs=16M count=1 2>&1 && rm {fname}')
			if b'1+0 records out' not in out:
				quit(Status.DOWN, f'Shell available system resources have changed ({vm.arch} VM)',
					f'check_sla_system_resources(): dd failed to write 16MiB in home: {out=} ({vm.id})')
		elif op == 2:
			# Check that we can write files of 16MiB in /tmp
			fname = '/tmp/' + rng.random_filename()
			out = await vm.command_output(f'dd if=/dev/zero of={fname} bs=16M count=1 && rm {fname}')
			if b'1+0 records out' not in out:
				quit(Status.DOWN, f'Shell available system resources have changed ({vm.arch} VM)',
					f'check_sla_system_resources(): dd failed to write 16MiB in /tmp: {out=} ({vm.id})')
		elif op == 3:
			# Check that we can allocate a 32MiB memory buffer
			out = await vm.command_output(f'dd if=/dev/zero of=/dev/null bs=32M count=1')
			if b'1+0 records out' not in out:
				quit(Status.DOWN, f'Shell available system resources have changed ({vm.arch} VM)',
					f'check_sla_system_resources(): dd faield allocating 32MiB buffer: {out=} ({vm.id})')

	eprint('check_sla_system_resources(): OK')


async def check_sla(team_id: int, team_ip: str):
	seed = os.environ.get('SEED', os.urandom(16).hex())
	eprint('CHECK_SLA RNG seed:', seed)

	rng = RNG(seed)
	token1 = get_new_token(team_id)
	token2 = get_new_token(team_id)

	# 3 VMs, 2 for the same UID, 1 with a different UID, no AES for reliability
	vms = await asyncio.gather(*(
		connect(team_ip, token1, 'i386'),
		connect(team_ip, token1, 'arm64'),
		connect(team_ip, token2, rng.choice(['i386', 'arm64']))
	))

	if vms[0].uid != vms[1].uid:
		quit(Status.DOWN, 'UID mismatch for the same token',
			f'UID mismatch (check_sla): {vms[0].uid=} VS {vms[1].uid=}')

	if vms[1].uid == vms[2].uid:
		quit(Status.DOWN, 'Same UID assigned for different tokens',
			f'UID reuse: {vms[1].uid=} VS {vms[2].uid=}')

	ops = [
		partial(check_sla_push_pop, rng, *vms[:2]),
		partial(check_sla_push_peek_pop_single_vm, rng, *vms[:2]),
		partial(check_sla_push_peek_pop_multi_vm, rng, *vms[:2]),
		partial(check_sla_peek_multi_uid, rng, *vms[1:]),
		partial(check_sla_chkkey, rng, *vms),
		partial(check_sla_bad_commands, rng, *vms),
		partial(check_sla_system_resources, rng, *vms[:2])
	]
	rng.shuffle(ops)

	timer_start('CHECK_SLA ops')

	for op in ops:
		await op()

	timer_stop('CHECK_SLA ops')
	quit(Status.OK, 'OK')


async def main():
	# Always run in the right directory so we have all the files we need
	os.chdir(Path(os.path.abspath(__file__)).parent.parent)

	timer_start('main')

	if args.DEV:
		# Dev mode: python3 -m checker DEV [TOKENSERVER=url] ACTION=action [FLAG=flag]
		if not args.ACTION:
			sys.exit('Missing ACTION=')

		team_id = 255
		team_ip = '127.0.0.1'
		action  = args.ACTION
		flag    = args.FLAG or 'FLAG{this_is_a_test_omegalul}'
	else:
		# Normal mode: python3 -m checker, vars passed throug env / checklib
		data    = get_data()
		team_id = int(data['teamId'])
		vbox_id = int(data['vulnboxId'])
		team_ip = f'10.{60 + vbox_id}.{team_id}.1'
		action  = data['action']
		flag    = data['flag']

	try:
		if action == Action.CHECK_SLA.name:
			await check_sla(team_id, team_ip)
		elif action == Action.PUT_FLAG.name:
			await put_flag(team_id, team_ip, flag)
		elif action == Action.GET_FLAG.name:
			await get_flag(team_ip, flag)
	except EOFError:
		quit(Status.DOWN, 'Unexpected EOF', 'Unexpected EOF:\n\n' + format_exc())
	except TimeoutError:
		quit(Status.DOWN, 'Timed out waiting for output', 'Timeout:\n\n' + format_exc())
	except Exception:
		die('Unexpected exception:\n\n' + format_exc())

	die('UNREACHABLE')


if __name__ == '__main__':
	try:
		asyncio.run(main())
	except SystemExit as e:
		# Do nothing, this is normal, just catch it and re-raise it to avoid a
		# silly asyncio stack trace complaining that a "Task exception was never
		# retrieved"... MEME.
		raise e from None
