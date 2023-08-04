#!/bin/bash
#
# Deploy the service on a fresh machine.
#

set -e

err() {
	echo -ne "\e[01;31m" >&2
	echo -n "$@" >&2
	echo -e "\e[0m" >&2
}

run() {
	echo -e "\e[01;33m======================================================================"
	echo "$@"
	echo -e "======================================================================\e[0m"
	"$@"
}


if [ -f .service_deployed ]; then
	err "I wouldn't run this twice if I were you..."
	exit 1
fi

if [ "$EUID" != "0" ]; then
	err 'You need to be root'
	exit 1
fi

if [ -z "$1" ]; then
	err "Usage: $0 TEAM_ID"
	exit 1
fi

if ! [[ "$1" =~ ^[0-9]+$ ]] || [ "$1" -gt 255 ]; then
	err "Invalid TEAM_ID, must be an integer between 0 and 255 (inclusive)."
	exit 1
fi

run ./deploy-00-get-files.sh
run ./deploy-01-host-setup.sh
run ./deploy-02-create-vms.sh
run ./deploy-03-start-service.sh "$1"
touch .service_deployed
