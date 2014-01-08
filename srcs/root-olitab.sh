#! /bin/sh
BCTFILE=flash.bct
CONFIGFILE=flash.cfg
BOOTLOADER=bootloader.bin
BOOTLOADER_ID=4
SYSID=9
KERNEL=boot.img
KERNEL_ID=7
#ODMDATA=0xC0075
ODMDATA=0x300C001
BACKUPSYS=system-org.img

mkdir -p ~/.android
echo 0x0408 >~/.android/adb_usb.ini

set -e
if [ $(id -u) != 0 ]
then
        echo "Must be root"
        exit 1
fi
if ! lsusb -d 0955:7820
then
        echo "Device must be in APX-Mode"
        exit 1
fi
./nvflash --bct $BCTFILE --bl $BOOTLOADER --download $BOOTLOADER_ID $BOOTLOADER
./nvflash -r --read $KERNEL_ID origboot.img
./bootunpack origboot.img
rm -rf initrd
mkdir initrd
cp mkbootfs initrd
cd initrd
zcat ../origboot.img-ramdisk.cpio.gz | cpio -i
sed -i "s/ro.secure=1/ro.secure=0/" default.prop
sed -i "s/persist.service.adb.enable=0/persist.service.adb.enable=1/" default.prop
./mkbootfs  . | gzip -9 >../new_ramdisk.gz
cd ..
./mkbootimg --kernel origboot.img-kernel.gz --ramdisk new_ramdisk.gz -o newboot.img
cat newboot.img /dev/zero | dd bs=2048 count=4096 >newboot_pad.img
./nvflash -r --download $KERNEL_ID newboot.img
./nvflash -r --sync
./nvflash -r --read 6 test.img --go
echo
echo "#########################################"
echo "#           !! Please Note  !!          #"
echo "#                                       #"
echo "# Wait until LifeTab Starts completely  #"
echo "# Only then press the Enter key         # "
echo "#                                       #"
echo "#########################################"
echo
read dummy
./adb kill-server
echo "waiting for adb connection to LifeTab device.."
./adb wait-for-device
echo "ok, installing su tools.."
./adb remount
./adb push su /system/xbin/su
./adb shell chmod 6755 /system/xbin/su
./adb push Superuser.apk /system/app/Superuser.apk
./adb pull /system/build.prop build.prop
sed -i "s/ro.config.play.bootsound=1/ro.config.play.bootsound=0/" build.prop
./adb push build.prop /system/build.prop
echo
echo "#########################################"
echo "#          Lifetab is restarted         #"
echo "#########################################"
echo
./adb reboot
#rm -rf initrd *.img *.gz *config build.prop

