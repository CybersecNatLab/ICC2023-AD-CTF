#!/bin/bash
#
# Set up the system to allow VMs to run using custom QEMU binaries installed
# under /usr/local and using bridge networking on the virbr0 interface.
#

set -e

log() {
	echo -ne "\e[01;34m"
	echo -n "$@"
	echo -e "\e[0m"
}

err() {
	echo -ne "\e[01;31m" >&2
	echo -n "$@" >&2
	echo -e "\e[0m" >&2
}


if [ -f .service_deployed ]; then
	err "I wouldn't run this twice if I were you..."
	exit 1
fi

if [ "$EUID" != "0" ]; then
	err 'This script needs to run as root'
	exit 1
fi

export LIBVIRT_DEFAULT_URI=qemu:///system
echo LIBVIRT_DEFAULT_URI=qemu:///system >> "$HOME"/.bashrc

#
# Install needed packages, QEMU and Docker engine
#
log 'Installing dependencies'
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y qemu-system virtinst virt-manager libvirt-clients libvirt-daemon-system dnsmasq ca-certificates curl gnupg apparmor-utils

log 'Installing qemu-system-arcanelink'
apt-get install -y ./files/qemu-system-arcanelink.deb

log 'Installing Docker from get.docker.com'
which /usr/bin/docker >/dev/null || curl 'https://get.docker.com' | bash

#
# Allow custom QEMU binaries and firmwares under AppArmor
#
log 'Relaxing AppArmor'
mkdir -p /etc/apparmor.d/local/abstractions
cat > /etc/apparmor.d/local/abstractions/libvirt-qemu <<'EOF'
/usr/local/bin/qemu-system-* rmix,
/usr/local/share/qemu/** rk,
/tmp/*.log rw,
EOF

systemctl reload apparmor
systemctl restart libvirtd
aa-complain libvirtd

#
# Enable vm.overcommit_memory=1 for redis
#
log 'Enabling vm.overcommit_memory=1'
echo vm.overcommit_memory=1 >> /etc/sysctl.conf
sysctl -w vm.overcommit_memory=1

#
# Setup/fix default libvirt network and bring virbr0 up
#
log 'Setting up "default" network'
if virsh net-list --all | grep -q default; then
	log 'Network "default" already exists, destroying it'
	virsh net-destroy default 2>/dev/null || true
	virsh net-undefine default
fi

cat > /tmp/net-default.xml <<EOF
<network>
  <name>default</name>
  <forward mode='nat'/>
  <mac address='52:54:00:1c:23:ff'/>
  <ip address='10.88.99.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.88.99.2' end='10.88.99.254'/>
    </dhcp>
  </ip>
</network>
EOF

virsh net-define /tmp/net-default.xml
virsh net-autostart default
virsh net-start default
rm /tmp/net-default.xml

if ! ip a show virbr0 >/dev/null; then
	err 'Interface virbr0 does not exist after network setup!'
	exit 1
fi

#
# Whitelist virbr0 for use by QEMU
#
log 'Enabling virbr0 for use by libvirt user sessions'
mkdir -p /etc/qemu
echo 'allow virbr0' >> /etc/qemu/bridge.conf

log 'Done!'
