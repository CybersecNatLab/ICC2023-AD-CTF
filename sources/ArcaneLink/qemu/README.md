ArcaneLink - QEMU Device
========================

Source code for the ArcaneLink device for QEMU 7.2.1 as well as a patch to apply
to QEMU sources for compilation. See [`src`](./src) directory.

Follow the instructions below to build custom QEMU system binaries starting from
QEMU 7.2.1 source code and automatically create a Debian package for its
installation. The package will be created at `qemu-system-arcanelink.deb`. The
`workdir` directory created during the build process can be safely deleted.


Building through Docker
-----------------------

Run the [`./build-deb-in-docker.sh](./build-deb-in-docker.sh) script from this
directory. This is the preferred way of building as it avoids leaking
information about the host environment (e.g. paths to files).

```bash
./build-deb-in-docker.sh            # Configure and build
./build-deb-in-docker.sh SKIPCONFIG # Build only, no ./configure
```


Building without Docker
-----------------------

Install build dependencies:

```bash
sudo apt-get install -y binutils make ninja-build gcc pkg-config \
	libglib2.0-dev libpixman-1-dev libkeyutils1 libkeyutils-dev patch
```

Run the [`./build-deb.sh`](./build-deb.sh) script from this directory:

```bash
./build-deb.sh            # Configure and build
./build-deb.sh SKIPCONFIG # Build only, no ./configure
```


Debugging
---------

Pass `-d arcane` or to the QEMU command line to enable debug logs of the
ArcaneLink device. Pass `-d arcane_rw` for even more logs (every single mmio
r/w). Use `-D path/to/file.log` to also redirect the logs to a file, which can
then be easily followed while QEMU is running using `tail -f path/to/file.log`.

For libvirt, this can be accomplished sdding the following to the `<domain>`
element:

```xml
<domain xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0' ...>
```

And then adding the following inside `<domain>`:

```xml
<qemu:commandline>
	<qemu:arg value='-d'/>
	<qemu:arg value='arcane'/>
	<qemu:arg value='-d'/>
	<qemu:arg value='arcane_rw'/>
	<qemu:arg value='-D'/>
	<qemu:arg value='/path/to/file.log'/>
</qemu:commandline>
```
