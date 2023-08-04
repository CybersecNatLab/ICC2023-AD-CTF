ArcaneLink - Kernel Module
==========================

Kernel module for QEMU guests to interact with the ArcaneLink device through
MMIO.

Follow the instructions below to build two versions of the kernel module, one
for i386 and one for ARM64, and package them as `.deb` Debian packages:
`karcane-i386.deb` and `karcane-arm64.deb`. These packages are then ready to be
installed through `apt`/`dpkg` inside the VMs. The `workdir` directory created
during the build process can be safely deleted.


Building through Docker
-----------------------

Run the [`./build-deb-in-docker.sh](./build-deb-in-docker.sh) script from this
directory. This is the preferred way of building as it avoids leaking
information about the host environment (e.g. paths to files).


Building without Docker
-----------------------

Run the [`./build-debs.sh`](./build.sh) script from this directory.

You will need an ARM64 cross compilation toolchain. The build script uses
`CROSS_COMPILE=aarch64-linux-gnu-` when compiling the ARM64 kernel module. Also,
good luck with the weird debian build dependencies.
