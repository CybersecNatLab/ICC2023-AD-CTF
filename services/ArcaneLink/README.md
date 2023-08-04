ArcaneLink
==========

In a bygone era, nestled between two grand castles, lay a flourishing kingdom
ruled by a wise and visionary king. Eager to ensure seamless communication and
enhance the realm's security, the king yearned for a groundbreaking solution.
Then, astonishingly, a mysterious figure, claiming to be a wizard from a distant
future, made a grand entrance into the kingdom's midst.

"You see," - whispered the wizard into the king's ears - "you are surrounded by
walls and gates, but you still send messages through carrier pigeons"... "you
could use an additional level of magic protection: I call it 'virtualization'".
This enigmatic visitor was about to bestow upon the kingdom an extraordinary
technology that will only later be known as "QEMU".

The wizard had conjured an enchanted device enabling a king to simulate a
Virtual Manor (VM) safer than its own fortified castle. Gone were the days when
one had to wait days to deliver a message! Each kingdom could now use its own VM
to exchange information instantly without the need of carrier pigeons, through
an ethereal, invisible communication link.

However, this Arcane Link hid fatal flaws that only revealed themselves through
the time. Dark wizards from neighboring enemy reigns had long studied its magic,
and crafted spells capable of breaking it. In an effort to safeguard the kingdom
as fast as possible, the king himself entrusted a scroll containing invaluable
information to one of his most reliable conuselors, that just so happened to
also be a wizard.

As the sun set over the castle, with determination, the loyal conuselor prepared
to embark on an extraordinary and fascinating quest, hoping that the knowledge
within the manual would unveil the secrets necessary to save the realm from the
impending magical war on the horizon.

--------------------------------------------------------------------------------

About
-----

This service manages two VMs through libvirt, which run using the QEMU binaries
installed from `files/qemu-system-arcanelink.deb` and use copies of the two
QCOW2 images (uncompressed to `/arcanelink_vms`) provided in the same directory
as disk. The other `.deb` packages provided in the `files` directory for
convenience are installed *inside* the VMs and are functional to the service.


Managing the service
--------------------

Start/stop the service using `docker compose`:

```bash
cd src
docker compose up -d
docker compose stop
```

**NOTE THAT** starting the service automatically starts the associated VMs.
However, stopping it does *NOT* automatically shut them down. If wish to do so,
you will have to do this manually through `virsh shutdown`.


Connecting to the service
-------------------------

A token server running at `http://10.10.0.5` generates temporary tokens to use
to authenticate to the service.

Request a token from the token server with a GET request to
`/token?target=TEAM_ID`:

```bash
curl 'http://10.10.0.5/token?target=TEAM_ID'
```

You can now use the token to authenticate with the service running on
`TEAM_ID`'s vulnbox.

**NOTE THAT** the token returned by the token server is a JSON string and
therefore enclosed in double quotes (`"`), which must be stripped.


Debugging
---------

A simplified debugging setup with small initramfs images is provied in the
`debug` directory. You can use it on a different machine than the vulnbox for
testing/debugging.

If you want a shell inside the VMs managed by the service, each of them has an
`administrator` user with `sudo` privileges that you can use (at your own risk).
You can create a SSH key for it and add it through `virsh`:

```bash
ssh-keygen -f mykey -N ''

virsh list --all
virsh set-user-sshkeys --file mykey.pub VM_NAME administrator

virsh domifaddr VM_NAME --source agent
ssh -i mykey administrator@VM_ADDR
```
