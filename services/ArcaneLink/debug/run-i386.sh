#!/bin/sh

# Remove after reading :)
echo 'DO NOT RUN THIS SCRIPT IN THE VULNBOX, copy it somewhere else!'
exit 1

if ! qemu-system-x86_64 -d help | grep -q arcane; then
	echo 'This QEMU does not implement the ArcaneLink device!'
	exit 1
fi

# You can enable debug logs with -d arcane AND/OR -d arcane_rw
qemu-system-x86_64 \
	-kernel ./vmlinuz-6.1.0-10-686-pae \
	-initrd ./initramfs-i386.cpio.gz \
	-machine q35 \
	-cpu pentium3 \
	-m 2G \
	-smp 6 \
	-net none \
	-nographic \
	-append 'console=ttyS0 memmap=1M$0x13370000'
