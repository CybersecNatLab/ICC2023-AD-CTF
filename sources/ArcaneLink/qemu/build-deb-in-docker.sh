#!/bin/bash
#
# Like ./build-deb.sh, but use Docker to build the package in a clean Ubuntu 22
# environment. You'll need docker and docker-compose-plugin installed.
#

set -e

if id -nG | grep -qw docker; then
	MAYBE_SUDO=
else
	MAYBE_SUDO=sudo
fi

$MAYBE_SUDO docker compose run --rm --build build "$@"
$MAYBE_SUDO chown -R $USER:$USER workdir
mv -f workdir/qemu-system-arcanelink.deb .
