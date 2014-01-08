#!/bin/bash
# Giacomo Bergami (c) 2012

echo Insert the idvendor from lsusb
read idd
echo Insert the username of the system
read user
echo Insert the folder where to mount the drive
read path

apt-get install mtpfs
apt-get install mtp-tools
#apt-get install python-pymtp
echo SUBSYSTEM==\"usb\", ATTR{idVendor}==\"$idd\", MODE=\"0666\" >> /etc/udev/rules.d/51-android.rules
mkdir /media/$path
chown $user:$user /media/$path
echo mtpfs     /media/$path     fuse     user,noauto,allow_other      0      0 >> /etc/fstab
echo user_allow_other >> /etc/fuse.conf
adduser $user fuse
echo The System will now reboot. Press ENTER to continue.
read
reboot
