#!/bin/bash
#
# Copy files to deploy the vulnbox into the appropriate directory.
#

set -e

line() {
	echo -e "\e[01;33m-----" "$@" "--------------------------------------------------------\e[0m"
}


if ! [ -f ./copy-files-to-deploy.sh ]; then
	echo "This script must be run from the directory where it is located" >&2
	exit 1
fi

DEPLOY_DIR="../../services/ArcaneLink"

# Copy backend source code excluding the README
rm -rf "$DEPLOY_DIR"/src
cp -r backend "$DEPLOY_DIR"/src
rm "$DEPLOY_DIR"/src/README.md

# Also build and copy DEBs (dev only, should be hosted separately for deployment)
if [ "$1" = "DEBS" ]; then
	mkdir -p "$DEPLOY_DIR"/files
	line cli  && cd cli  && ./build-debs.sh                     && cd ..
	line jail && cd jail && ./build-debs.sh                     && cd ..
	line kmod && cd kmod && ./build-debs-in-docker.sh           && cd ..
	line qemu && cd qemu && ./build-deb-in-docker.sh SKIPCONFIG && cd ..
	cp -f {cli,jail,kmod,qemu}/*.deb "$DEPLOY_DIR"/files
fi
