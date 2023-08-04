ArcaneLink - Guest VM Shell Jail
================================

This is a simple wrapper to be run as setuid root that invokes `bash` through
[`nsjail`](https://github.com/google/nsjail) to limit the capabilities and
resource usage of users that connect to the challenge VMs. This wrapper should
be instlled inside the VMs and spawned instead of the normal login shell by the
backend server.


Building
--------

Run the [`./build-deb.sh`](./build-deb.sh) script from this directory to build
two `.deb` packages: `nsjail-bash-i386.deb` and `nsjail-bash-arm64.deb`, that
are then ready to be installed through `apt`/`dpkg` inside the VMs.
