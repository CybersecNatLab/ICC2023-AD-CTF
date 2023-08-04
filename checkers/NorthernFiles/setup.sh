#!/bin/bash

# This mkdir locking is terrible but required by the infra, because every 
# worker share the same mounted volume and they cannot start installing 
# node_modules on top of the others. Also, flock was not an option because of
# the file system

mkdir lock && npm ci