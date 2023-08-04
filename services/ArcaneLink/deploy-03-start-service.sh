#!/bin/bash
#
# Bring up the service after first deploy.
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

if [ -z "$1" ]; then
	echo "Missing TEAM_ID" 2>&1
	exit 1
fi

if ! [[ "$1" =~ ^[0-9]+$ ]] || [ "$1" -gt 255 ]; then
	err "Invalid TEAM_ID, must be an integer between 0 and 255 (inclusive)."
	exit 1
fi

cd src
sed -i "s/TEAM_ID=255/TEAM_ID=$1/" docker-compose.yml
grep -q "TEAM_ID=$1" docker-compose.yml
docker compose up -d
