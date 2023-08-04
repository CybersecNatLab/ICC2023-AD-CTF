#!/bin/sh

# Remove after reading :)
echo 'DO NOT RUN THIS SCRIPT IN THE VULNBOX, copy it somewhere else!'
exit 1

if ! qemu-system-aarch64 -d help | grep -q arcane; then
	echo 'This QEMU does not implement the ArcaneLink device!'
	exit 1
fi

# You can enable debug logs with -d arcane AND/OR -d arcane_rw
qemu-system-aarch64 \
	-kernel ./vmlinuz-6.1.0-10-arm64 \
	-initrd ./initramfs-arm64.cpio.gz \
	-machine virt \
	-cpu cortex-a57 \
	-m 2G \
	-smp 6 \
	-net none \
	-nographic \
	-append 'console=ttyAMA0'
