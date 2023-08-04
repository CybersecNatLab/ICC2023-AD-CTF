ArcaneLink - Guest VMs Setup
============================

The two VMs used as guests for the challenge are Debian 12 i386 and arm64
respectively. Their initial setup consists of a simple 10GiB QCOW2 disk where
Debian 12 has been installed from the "netinst" ISO:
[i386](https://cdimage.debian.org/debian-cd/current/i386/iso-cd/)
[arm64](https://cdimage.debian.org/debian-cd/current/arm64/iso-cd/).


Initial VM setup
----------------

To create the two VMs, create two QCOW2 disks and complete the installation
through the netinst ISO using `virt-install`. Choose "no desktop environment"
and install the openssh server. Then create a user named `administrator`.

```bash
# Create disk images
qemu-img create -f qcow2 ic3-bookworm-i386.qcow2 10G
qemu-img create -f qcow2 ic3-bookworm-arm64.qcow2 10G

virt-install \
	--arch x86_64 \
	--name ic3-bookworm-i386 \
	--memory 2048 \
	--vcpus 4 \
	--disk ic3-bookworm-i386.qcow2,device=disk,bus=virtio \
	--cdrom path/to/debian-12.0.0-i386-netinst.iso \
	--os-variant debiantesting \
	--network bridge=virbr0

# Connect to the VM through VNC and complete the setup (only needed for i386)
virt-viewer ic3-bookworm-i386

virt-install \
	--arch aarch64 \
	--name ic3-bookworm-arm64 \
	--memory 2048 \
	--vcpus 4 \
	--disk ic3-bookworm-arm64.qcow2,device=disk,bus=virtio \
	--cdrom path/to/debian-12.0.0-arm64-netinst.iso \
	--os-variant debiantesting \
	--network bridge=virbr0 \
	--tpm none \
	--boot loader=/usr/share/AAVMF/AAVMF_CODE.fd,loader.type=pflash,loader.readonly=yes,loader.secure=no,nvram.template=/usr/share/AAVMF/AAVMF_VARS.fd

# Complete the setup through text console
```


VM configuration
----------------

After creating the VMs as described above, first build the `.deb` packages for
the other challenge components (see also the README file in each component's
directory):

```bash
pushd ../kmod
./build-debs-in-docker.sh

cd ../jail
./build-debs.sh

cd ../cli
./build-debs.sh
popd
```

Now you should have the following files:

- `../kmod/karcane-{i386,arm64}.deb`
- `../jail/nsjail-bash-{i386,arm64}.deb`
- `../cli/arcane-cli-{i386,arm64}.deb`

For each VM now:

1. Copy the appropriate `.deb` files in the VM (according to the arch).
2. Copy the [`setup-vm.sh`](./setup-vm.sh) inside the same directory.
3. Run the setup script inside the VM.
4. Cross your fingers and reboot the VM.
