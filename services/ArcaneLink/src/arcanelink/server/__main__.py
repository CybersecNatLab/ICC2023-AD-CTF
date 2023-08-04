import os
import asyncio
from ctypes import CDLL
from signal import SIGHUP
from typing import Tuple, List

import requests
from requests.exceptions import ConnectionError, ConnectTimeout, JSONDecodeError

from ..virsh import qga_get_interfaces
from ..utils import die, input_or_die, format_time
from ..config import BACKEND_HOST, BACKEND_PORT, LIBVIRT_DOMAINS, SESSION_CLEAR_INTERVAL


PR_SET_PDEATHSIG = 1


def get_session() -> Tuple[List[str],str,str,float]:
	raw_token = input_or_die('Token: ').strip()

	try:
		resp = requests.get(f'http://{BACKEND_HOST}:{BACKEND_PORT}/{raw_token}', timeout=30)
		data = resp.json()
	except (ConnectionError, ConnectTimeout):
		die('Backend is unreachable')
	except JSONDecodeError:
		die('Backend returned an invalid response')

	if resp.status_code != 200:
		die('Backend error: ' + str(data.get('detail', resp.status_code)))

	return data['vms'], data['username'], data['password'], data['time_left']


def fork_into_ssh(username: str, ip: str, password: str):
	print('-' * 70, flush=True)

	if os.fork() == 0:
		os.dup2(1, 2)
		CDLL('libc.so.6').prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0)

		os.execlpe('sshpass', 'sshpass', '-e', 'ssh', '-o',
			'StrictHostKeyChecking=no', '-o', 'UserKnownHostsFile=/dev/null',
			'-o', 'LogLevel=ERROR', '-tt', f'{username}@{ip}',
			'stty raw -echo -echonl -icanon -onlcr -ocrnl -opost; '
			'cat /etc/motd; nsjail-bash -i', {'SSHPASS': password})

	os.wait()


def main():
	vms, username, password, time_left = get_session()
	for vm in vms:
		assert vm in LIBVIRT_DOMAINS, f'VM {vm!r} not found'

	print('Session expires', f'in {format_time(time_left)}' if time_left > SESSION_CLEAR_INTERVAL else 'soon')
	print('Available VMs:', ', '.join(vms))

	vm = input_or_die('Select VM: ').strip()
	while vm not in vms:
		print('No such VM!\nAvailable VMs:', ', '.join(vms))
		vm = input_or_die('Select VM: ').strip()

	for iface in asyncio.run(qga_get_interfaces(vm)):
		if iface.name == 'enp1s0':
			break
	else:
		die('Network interface enp1s0 not found')

	ips = iface.get_ip_addrs('ipv4')
	if not ips:
		die('Network interface enp1s0 has no IPv4 assigned')

	fork_into_ssh(username, ips[0], password)


if __name__ == '__main__':
	main()
