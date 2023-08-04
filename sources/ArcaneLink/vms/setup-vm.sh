#!/bin/bash

if [ "$EUID" -ne 0 ]; then
	echo 'Restarting script as root'
	exec sudo -- "$0" "$@"
fi

ARCH=$(uname -m)
if [ "$ARCH" = "i686" ]; then
	ARCH="i386"
fi

if [ "$ARCH" = "aarch64" ]; then
	ARCH="arm64"
fi

if [ "$ARCH" != "i386" ] && [ "$ARCH" != "arm64" ]; then
	echo "Unsupported architecture: $ARCH"
	exit 1
fi

for f in ./karcane-$ARCH.deb ./nsjail-bash-$ARCH.deb ./arcane-cli-$ARCH.deb; do
	if [ ! -f "$f" ]; then
		echo "Missing deb: $f"
		exit 1
	fi
done

if [ "$ARCH" = "i386" ]; then
	if ! grep -q memmap= /etc/default/grub; then
		# Reserve physmem for ArcaneLink device
		sed -i 's|GRUB_CMDLINE_LINUX_DEFAULT="[^"]*"|GRUB_CMDLINE_LINUX_DEFAULT="memmap=1M\\\\\\$0x13370000"|' /etc/default/grub
		update-grub

		echo 'Rebooting after editing grub linux command line in 5s...'
		echo 'Restart this script to continue after reboot'
		sleep 5 && reboot
	fi
fi

# Needed packages
apt-get update
apt-get install -y binutils bsdutils procps qemu-guest-agent gcc libssl-dev \
	adduser perl-modules-5.36

apt-get install -y ./karcane-$ARCH.deb ./nsjail-bash-$ARCH.deb ./arcane-cli-$ARCH.deb

# Disable bracketed paste for cleaner raw TCP output
echo 'set enable-bracketed-paste off' >> /etc/inputrc

# Disable systemd user
sed -i 's/[^#].\+pam_systemd.so/#\0/' /etc/pam.d/common-session

# Enable SSH password authentication
sed -i 's/PasswordAuthentication.\+no//g' /etc/ssh/sshd_config
echo 'PasswordAuthentication yes' >> /etc/ssh/sshd_config

# Create user with UID 1000 if not present
USERNAME="$(id -nu 1000 2>/dev/null)"
if [ $? -ne 0 ]; then
	USERNAME=administrator
	adduser --disabled-password --gecos '' --uid 1000 $USERNAME
fi

# Remove its password and SSH keys and enable sudo without password
passwd -dl $USERNAME
rm -f /home/$USERNAME/.ssh/authorized_keys
echo "$USERNAME ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

# Clear logs
truncate -s 0 /var/log/btmp /var/log/wtmp /var/log/lastlog /var/log/dpkg.log /var/log/alternatives.log /var/log/journal/**/*.journal
if [ -d /var/log/unattended-upgrades ]; then truncate -s 0 "/var/log/unattended-upgrades/*.log"; fi
rm -rf /var/log/installer
rm -rf ~/.bash_history

# Replace motd
cat > /etc/motd <<'EOF'
                                  |>>>
                                  |
                    |>>>      _  _|_  _         |>>>
                    |        |;| |;| |;|        |
                _  _|_  _    \\.    .  /    _  _|_  _
               |;|_|;|_|;|    \\:. ,  /    |;|_|;|_|;|
               \\..      /    ||;   . |    \\.    .  /
                \\.  ,  /     ||:  .  |     \\:  .  /
                 ||:   |_   _ ||_ . _ | _   _||:   |
                 ||:  .|||_|;|_|;|_|;|_|;|_|;||:.  |
                 ||:   ||.    .     .      . ||:  .|
                 ||: . || .  Arcane Link  ,  ||:   |       \,/
                 ||:   ||:  ,  _______   .   ||: , |            /`\
                 ||:   || .   /+++++++\    . ||:   |
                 ||:   ||.    |+++++++| .    ||: . |
              __ ||: . ||: ,  |+++++++|.  . _||_   |
     ____--`~    '--~~__|.    |+++++__|----~    ~`---,              ___
-~--~                   ~---__|,--~'                  ~~----_____-~'   `~----~~
EOF
