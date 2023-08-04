#!/bin/bash
#
# Create and start two VMs named "ic3-bookworm-i386" and "ic3-bookworm-arm64".
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

VMS_DIR=/arcanelink_vms
mkdir -p "$VMS_DIR"

I386_DISK_FNAME=ic3-bookworm-i386.qcow2
I386_DISK_GZ_PATH=files/"$I386_DISK_FNAME".gz
I386_DISK_PATH="$VMS_DIR"/ic3-bookworm-i386.qcow2

ARM64_DISK_FNAME=ic3-bookworm-arm64.qcow2
ARM64_DISK_GZ_PATH=files/"$ARM64_DISK_FNAME".gz
ARM64_DISK_PATH="$VMS_DIR"/ic3-bookworm-arm64.qcow2

if ! [ -f "$I386_DISK_PATH" ]; then
	log "Extracting $I386_DISK_GZ_PATH to $I386_DISK_PATH"
	gunzip -ck "$I386_DISK_GZ_PATH" > "$I386_DISK_PATH"
fi

if ! [ -f "$ARM64_DISK_PATH" ]; then
	log "Extracting $ARM64_DISK_GZ_PATH to $ARM64_DISK_PATH"
	gunzip -ck "$ARM64_DISK_GZ_PATH" > "$ARM64_DISK_PATH"
fi

chown -R libvirt-qemu:kvm "$VMS_DIR"
chmod -R 770 "$VMS_DIR"


log "Creating VM ic3-bookworm-i386"
virt-install \
	--arch x86_64 \
	--name ic3-bookworm-i386 \
	--cpu pentium3 \
	--memory 2048 \
	--vcpus 6 \
	--disk "$I386_DISK_PATH",device=disk,bus=virtio \
	--import \
	--os-variant debiantesting \
	--network bridge=virbr0 \
	--noautoconsole


log "Creating VM ic3-bookworm-arm64"
virt-install \
	--arch aarch64 \
	--name ic3-bookworm-arm64 \
	--cpu cortex-a57 \
	--memory 2048 \
	--vcpus 6 \
	--disk "$ARM64_DISK_PATH",device=disk,bus=virtio \
	--import \
	--os-variant debiantesting \
	--network bridge=virbr0 \
	--tpm none \
	--boot loader=/usr/local/share/qemu/edk2-aarch64-code.fd,loader.type=pflash,loader.readonly=yes,loader.secure=no,nvram.template=/usr/local/share/qemu/edk2-arm-vars.fd \
	--noautoconsole


virsh list --all
