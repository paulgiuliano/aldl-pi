#!/bin/sh

echo "This script is for modern debian systems, and does everything that should"
echo "be necessary to use ftdi usb devices as a regular user with libftdi,"
echo "instead of the built-in dumb kernel driver."

if [ $(id -u) != 0 ]; then
  echo "This script must be run as root."
  exit 1
else
  read -p "Press enter to continue, or ctrl-c to bail..." x
fi

echo "** Adding usb group"
groupadd usb

echo "** Setting udev permissions on usb devices"
echo 'SUBSYSTEMS=="usb", ACTION=="add", MODE="0664", GROUP="usb"' >> /etc/udev/rules.d/30-usb.rules

echo "** Reloading udev rules"
/etc/init.d/udev reload

echo "** Blacklisting ftdi_sio driver"
echo 'blacklist ftdi_sio' > /etc/modprobe.d/ftdi.conf

echo "** Removing old ftdi_sio driver (it's ok if it fails)"
rmmod ftdi_sio

echo "!! Run the following command as root, to add your user to the usb group"
echo "useradd -G usb yourusernamehere"
