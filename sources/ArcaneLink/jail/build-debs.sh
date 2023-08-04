#!/bin/bash
#
# Usage: ./build-deb.sh
#
# Build DEB packages for a jailed bash shell to install inside challenge VMs.
#
# Produces two files: "nsjail-bash-i386.deb" and "nsjail-bash-arm64.deb".
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

NSJAIL_I386="$ORIGDIR"/nsjail_dynamic_i386
NSJAIL_ARM64="$ORIGDIR"/nsjail_dynamic_arm64
NSJAIL_CFG="$ORIGDIR"/nsjail-bash.cfg

WORKDIR="$ORIGDIR"/workdir
PKGDIR="$WORKDIR"/nsjail-bash
INSTALLDIR="$PKGDIR"/usr/local

rm -rf "$PKGDIR"
mkdir -p "$INSTALLDIR"
mkdir -p "$INSTALLDIR"/bin "$INSTALLDIR"/etc
mkdir -p "$PKGDIR"/DEBIAN

cp "$NSJAIL_CFG" "$INSTALLDIR"/etc
cat > "$PKGDIR"/DEBIAN/control <<EOF
Package: nsjail-bash
Version: 0.0.1
Depends: libc6, libstdc++6, libgcc-s1, libprotobuf32, libnl-route-3-200, libnl-3-200
Maintainer: D33z N00tz G0tt3m
Architecture: i386
Description: nsjail-wrapped GNU Bourne Again SHell
EOF

# Dirty as hell, but don't want to overcomplicate things with dh_fixperms
cat > "$PKGDIR"/DEBIAN/postinst <<EOF
#!/bin/bash
chown root:root /usr/local/bin /usr/local/etc
chmod 755 /usr/local/bin /usr/local/etc
chown root:root /usr/local/bin/{nsjail,nsjail-bash} /usr/local/etc/nsjail-bash.cfg
chmod 755 /usr/local/bin/{nsjail,nsjail-bash}
chmod u+s /usr/local/bin/nsjail-bash
chmod 644 /usr/local/etc/nsjail-bash.cfg
EOF
chmod 755 "$PKGDIR"/DEBIAN/postinst

cd "$WORKDIR"

log 'Compiling i386 binary'
make -C "$ORIGDIR" CFLAGS=-m32
mv -f "$ORIGDIR"/nsjail-bash "$INSTALLDIR"/bin

log 'Building i386 Debian package'
cp -f "$NSJAIL_I386" "$INSTALLDIR"/bin/nsjail
chmod a+rx "$INSTALLDIR"/bin/nsjail
dpkg-deb --build nsjail-bash
mv -f nsjail-bash.deb "$ORIGDIR"/nsjail-bash-i386.deb


log 'Compiling amd64 binary'
make -C "$ORIGDIR" CROSS_COMPILE=aarch64-linux-gnu-
mv -f "$ORIGDIR"/nsjail-bash "$INSTALLDIR"/bin

log 'Building arm64 Debian package'
sed -i 's/Architecture: i386/Architecture: arm64/' "$PKGDIR"/DEBIAN/control
cp -f "$NSJAIL_ARM64" "$INSTALLDIR"/bin/nsjail
chmod a+rx "$INSTALLDIR"/bin/nsjail
dpkg-deb --build nsjail-bash
mv -f nsjail-bash.deb "$ORIGDIR"/nsjail-bash-arm64.deb

log "Done"
