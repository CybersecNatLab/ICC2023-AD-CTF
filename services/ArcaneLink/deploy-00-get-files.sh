#!/bin/bash
#
# Get some large service files from an external server.
#

set -e

err() {
	echo -ne "\e[01;31m" >&2
	echo -n "$@" >&2
	echo -e "\e[0m" >&2
}


if [ -f .service_deployed ]; then
	err "I wouldn't run this twice if I were you..."
	exit 1
fi

if [ -z "$REMOTE_URL" ]; then
	REMOTE_URL=http://10.10.0.5:8888
fi

FILES=()

# Guest VM disk images
FILES+=(ic3-bookworm-i386.qcow2.gz)
FILES+=(ic3-bookworm-arm64.qcow2.gz)

# Host QEMU
FILES+=(qemu-system-arcanelink.deb)

# These are already installed in the VMs, provide them in the host for convenience
for arch in i386 arm64; do
	FILES+=(karcane-$arch.deb)
	FILES+=(nsjail-bash-$arch.deb)
	FILES+=(arcane-cli-$arch.deb)
done

mkdir -p files
for f in "${FILES[@]}"; do
	wget -O files/"$f" "$REMOTE_URL"/"$f"
done

# Kernels needed for debugging
wget -O debug/vmlinuz-6.1.0-10-686-pae "$REMOTE_URL"/vmlinuz-6.1.0-10-686-pae
wget -O debug/vmlinuz-6.1.0-10-arm64 "$REMOTE_URL"/vmlinuz-6.1.0-10-arm64
