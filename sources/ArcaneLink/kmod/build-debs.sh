#!/bin/bash
#
# Download kernel headers from Debian 12 .deb packages, patch them to work
# locally and use them to build the kernel module for the target VMs (i386 and
# arm64). Then, package each module into a Debian package.
#
# NOTE: this script compiles for Debian kernel 6.1.0-10. If you want to compile
# for another kernel, you'll need to patch this script and change the deb names.
#
# Produces two files: "karcane-i386.deb" and "karcane-arm64.deb".
#
# This script will create a disposable "workdir" directory in the current
# directory that can be safely removed after the modules are created.
#
# Lord have mercy for this insanity D:
#

set -e

log() {
	echo -ne "\e[01;34m"
	echo -n "$@"
	echo -e "\e[0m"
}

# get_debs DEBS...
get_debs() {
	for deb in "$@"; do
		if [ ! -f "$deb" ]; then
			wget 'https://deb.debian.org/debian/pool/main/l/linux/'"$deb"
		fi

		dpkg-deb -R "$deb" .
		rm -rf DEBIAN
	done
}

# patch_headers UNAME
patch_headers() {
	unlink lib/modules/$1/build
	unlink lib/modules/$1/source
	unlink usr/src/linux-headers-$1/tools
	unlink usr/src/linux-headers-$1/scripts

	ln -sr usr/src/linux-headers-$1              lib/modules/$1/build
	ln -sr usr/src/linux-headers-6.1.0-10-common lib/modules/$1/source
	ln -sr usr/lib/linux-kbuild-6.1/tools        usr/src/linux-headers-$1/tools
	ln -sr usr/lib/linux-kbuild-6.1/scripts      usr/src/linux-headers-$1/scripts

	sed -i "s|/usr/src|$(realpath usr/src)|" usr/src/linux-headers-$1/Makefile
}

build_i386() {
	local debs=(
		linux-headers-6.1.0-10-common_6.1.38-1_all.deb
		linux-headers-6.1.0-10-686-pae_6.1.38-1_i386.deb
		linux-kbuild-6.1_6.1.38-1_i386.deb
	)

	log 'Setting up i386 build'
	cd "$WORKDIR"
	rm -rf usr lib
	get_debs "${debs[@]}"
	patch_headers 6.1.0-10-686-pae
	local kdir=$(realpath lib/modules/6.1.0-10-686-pae/build)
	cd -

	log 'Building i386 module'
	make -C "$SRCDIR" KDIR="$kdir"

	log 'Building i386 Debian package'
	rm -rf "$INSTALLDIR"/lib/modules
	mkdir -p "$INSTALLDIR"/lib/modules/6.1.0-10-686-pae/kernel
	cp "$SRCDIR"/karcane.ko "$INSTALLDIR"/lib/modules/6.1.0-10-686-pae/kernel

	sed -i 's/Architecture: .\+/Architecture: i386/' "$PKGDIR"/DEBIAN/control
	cat > "$PKGDIR"/DEBIAN/preinst <<EOF
#!/bin/bash
if [ "\$(uname -r)" != "6.1.0-10-686-pae" ]; then
	echo "karcane: kernel version mismatch, refusing install"
	exit 1
fi
EOF
	chmod 755 "$PKGDIR"/DEBIAN/preinst
	dpkg-deb --build "$PKGDIR"
	mv -f "$WORKDIR"/karcane{,-i386}.deb
	cp -f "$WORKDIR"/karcane-i386.deb "$ORIGDIR"/karcane-i386.deb

	make -C "$SRCDIR" KDIR="$kdir" clean
}

build_arm64() {
	local debs=(
		linux-headers-6.1.0-10-common_6.1.38-1_all.deb
		linux-headers-6.1.0-10-arm64_6.1.38-1_arm64.deb
		linux-kbuild-6.1_6.1.38-1_amd64.deb
	)

	log 'Setting up arm64 build'
	cd "$WORKDIR"
	rm -rf usr lib
	get_debs "${debs[@]}"
	patch_headers 6.1.0-10-arm64
	local kdir=$(realpath lib/modules/6.1.0-10-arm64/build)
	cd -

	log 'Building arm64 module'
	make -C "$SRCDIR" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR="$kdir"

	log 'Building arm64 Debian package'
	rm -rf "$INSTALLDIR"/lib/modules
	mkdir -p "$INSTALLDIR"/lib/modules/6.1.0-10-arm64/kernel
	cp "$SRCDIR"/karcane.ko "$INSTALLDIR"/lib/modules/6.1.0-10-arm64/kernel

	sed -i 's/Architecture: .\+/Architecture: arm64/' "$PKGDIR"/DEBIAN/control

	# Make sure kernel version matches!
	cat > "$PKGDIR"/DEBIAN/preinst <<EOF
#!/bin/bash
if [ "\$(uname -r)" != "6.1.0-10-arm64" ]; then
	echo "karcane: kernel version mismatch, refusing install"
	exit 1
fi
EOF
	chmod 755 "$PKGDIR"/DEBIAN/preinst
	dpkg-deb --build "$PKGDIR"
	mv -f "$WORKDIR"/karcane{,-arm64}.deb
	cp -f "$WORKDIR"/karcane-arm64.deb "$ORIGDIR"/karcane-arm64.deb

	make -C "$SRCDIR" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR="$kdir" clean
}

ORIGDIR="$(pwd)"
if [ -z "$ORIGDIR" ]; then ORIGDIR=; fi

WORKDIR="$ORIGDIR"/workdir
SRCDIR="$ORIGDIR"/src
PKGDIR="$WORKDIR"/karcane
INSTALLDIR="$PKGDIR"
mkdir -p "$INSTALLDIR"

mkdir -p "$PKGDIR"/DEBIAN
cat > "$PKGDIR"/DEBIAN/control <<EOF
Package: karcane
Version: 0.0.1
Maintainer: D33z N00tz G0tt3m
Architecture: i386
Description: ArcaneLink Linux device driver
EOF

cat > "$PKGDIR"/DEBIAN/postinst <<EOF
#!/bin/bash
chown root:root /lib/modules/\$(uname -r)/kernel/karcane.ko
chmod 644 /lib/modules/\$(uname -r)/kernel/karcane.ko
chown root:root /etc/modules-load.d/karcane.conf
chmod 644 /etc/modules-load.d/karcane.conf

echo 'karcane: Running depmod...'
depmod

echo 'karcane: Loading module into running kernel...'
modprobe karcane
EOF
chmod 755 "$PKGDIR"/DEBIAN/postinst

cat > "$PKGDIR"/DEBIAN/prerm <<EOF
if lsmod | grep -q karcane; then
	echo 'karcane: Unloading module from running kernel...'
	if ! modprobe -r karcane; then
		echo 'karcane: Failed to unload module from running kernel, a reboot may be required'
	fi
fi
EOF
chmod 755 "$PKGDIR"/DEBIAN/prerm

cat > "$PKGDIR"/DEBIAN/postrm <<EOF
#!/bin/bash
echo 'karcane: Running depmod...'
depmod
EOF
chmod 755 "$PKGDIR"/DEBIAN/postrm

mkdir -p "$INSTALLDIR"/etc/modules-load.d
cat > "$INSTALLDIR"/etc/modules-load.d/karcane.conf <<EOF
karcane
EOF

build_i386
build_arm64
