#!/bin/bash
#
# Build DEB packages for the arcane-cli to install inside challenge VMs.
#
# Produces two files: "arcane-cli-i386.deb" and "arcane-cli-arm64.deb".
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


ORIGDIR="$(pwd)"
WORKDIR="$ORIGDIR"/workdir
PKGDIR="$WORKDIR"/arcane-cli
INSTALLDIR="$PKGDIR"/usr/local

rm -rf "$PKGDIR"
mkdir -p "$INSTALLDIR"
mkdir -p "$INSTALLDIR"/bin
mkdir -p "$PKGDIR"/DEBIAN

cat > "$PKGDIR"/DEBIAN/control <<EOF
Package: arcane-cli
Version: 0.0.1
Maintainer: D33z N00tz G0tt3m
Architecture: i386
Description: Command Line Interface utility for the ArcaneLink device
EOF

# Dirty as hell, but don't want to overcomplicate things with dh_fixperms
cat > "$PKGDIR"/DEBIAN/postinst <<EOF
#!/bin/bash
chown root:root /usr/local/bin
chmod 755 /usr/local/bin
chown root:root /usr/local/bin/arcane-cli
chmod 755 /usr/local/bin/arcane-cli
EOF
chmod 755 "$PKGDIR"/DEBIAN/postinst

cd "$WORKDIR"

log 'Compiling i386 binary'
make -C "$ORIGDIR" CFLAGS=-m32
mv -f "$ORIGDIR"/arcane-cli "$INSTALLDIR"/bin/arcane-cli

log 'Building i386 Debian package'
dpkg-deb --build arcane-cli
mv -f arcane-cli.deb "$ORIGDIR"/arcane-cli-i386.deb


log 'Compiling amd64 binary'
make -C "$ORIGDIR" CROSS_COMPILE=aarch64-linux-gnu-
mv -f "$ORIGDIR"/arcane-cli "$INSTALLDIR"/bin/arcane-cli

log 'Building arm64 Debian package'
sed -i 's/Architecture: i386/Architecture: arm64/' "$PKGDIR"/DEBIAN/control
dpkg-deb --build arcane-cli
mv -f arcane-cli.deb "$ORIGDIR"/arcane-cli-arm64.deb

log "Done"
