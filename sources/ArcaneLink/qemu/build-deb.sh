#!/bin/bash
#
# Usage: ./build-deb.sh [SKIPCONFIG]
#
# Patch QEMU 7.2.1 source and build a custom QEMU binary for the challenge, then
# create a Debian package (.deb) for easy installation. The final package will
# be created as "qemu-system-arcanelink.deb".
#
# Pass SKIPCONFIG as argument if you want to skip the ./configure step and only
# build QEMU, e.g. for faster testing of modifications to the driver source.
#
# This script will create a disposable "workdir" directory in the current
# directory that can be safely removed after the .deb package has been created.
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

ORIGDIR="$(pwd)"
if [ -z "$ORIGDIR" ]; then ORIGDIR=; fi

SRCDIR="$ORIGDIR"/src
DRIVER_SRC='qarcane.c'
QEMU_PATCH='patch.diff'

if ! [ -f "$SRCDIR"/"$DRIVER_SRC" ]; then
	err "$SRCDIR"/"$DRIVER_SRC not found, make sure to run this in the correct directory!"
	exit 1
fi

if ! [ -f "$SRCDIR"/"$QEMU_PATCH" ]; then
	err "$SRCDIR"/"$QEMU_PATCH not found, make sure to run this in the correct directory!"
	exit 1
fi

WORKDIR="$ORIGDIR"/workdir
PKGDIR="$WORKDIR"/qemu-system-arcanelink

rm -rf "$PKGDIR"
cd "$WORKDIR"

if ! [ -d qemu-7.2.1 ]; then
	if ! [ -f qemu-7.2.1.tar.xz ]; then
		log "Downloading QEMU 7.2.1 source"
		wget https://download.qemu.org/qemu-7.2.1.tar.xz
	fi

	log "Extracting QEMU 7.2.1 source"
	tar xf qemu-7.2.1.tar.xz
fi

log "Applying changes"
cd qemu-7.2.1

if ! [ -f hw/misc/"$DRIVER_SRC" ] || [ "$SRCDIR"/"$DRIVER_SRC" -nt hw/misc/"$DRIVER_SRC" ]; then
	cp -f "$SRCDIR"/"$DRIVER_SRC" hw/misc/"$DRIVER_SRC"
fi

if ! grep -q "$DRIVER_SRC" hw/misc/meson.build; then
	patch -p1 < "$SRCDIR"/"$QEMU_PATCH"
fi

SKIPCONFIG=0
if [ "$1" = SKIPCONFIG ]; then
	SKIPCONFIG=1

	if ! [ -f build/build.ninja ]; then
		log "Cannot skip ./configure, build files weren't generated yet!"
		SKIPCONFIG=0
	fi
fi

if [ $SKIPCONFIG -eq 0 ]; then
	log "Configuring QEMU"
	./configure --target-list=x86_64-softmmu,aarch64-softmmu --disable-tools \
		--disable-kvm --disable-debug-info --disable-slirp --disable-lto \
		--disable-werror --enable-multiprocess
fi

log "Building QEMU"
ninja -C build

log "Installing QEMU into $PKGDIR"
DESTDIR="$PKGDIR" ninja -C build install

# Don't need these
rm -rf "$PKGDIR"/usr/local/share/{icons,applications} "$PKDGIR"/usr/local/include

cd ..

log "Building Debian package"

mkdir -p "$PKGDIR"/DEBIAN
cat > "$PKGDIR"/DEBIAN/control <<EOF
Package: qemu-system-arcanelink
Version: 7.2.1
Maintainer: D33z N00tz G0tt3m
Architecture: amd64
Description: QEMU system emulator with built-in ArcaneLink MMIO driver
EOF

# Dirty as hell, but don't want to overcomplicate things with dh_fixperms
cat > "$PKGDIR"/DEBIAN/postinst <<EOF
#!/bin/bash
chown -R root:root /usr/local/share/qemu
chown root:root /usr/local/bin
chmod 755 /usr/local/bin
chown root:root /usr/local/bin/qemu-{ga,system-aarch64,system-x86_64}
chmod 755 /usr/local/bin/qemu-{ga,system-aarch64,system-x86_64}
EOF
chmod 755 "$PKGDIR"/DEBIAN/postinst

dpkg-deb --build qemu-system-arcanelink
cp qemu-system-arcanelink.deb ..

log "Done"
