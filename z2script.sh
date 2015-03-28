#!/bin/sh
# IZ2S v2.05beta-enhanced (02/04/10)
# z2script.sh (c) 2009-2010 Russell K. Davis.
# portions (c) 2006-2008 Zipitwireless Inc.
# portions (c) 2009 Ray Haque/oddree.com

# Check if lid is closed? If Yes power off
/mnt/sd0/bin/lid
lidopen=$?
if [ $lidopen -eq 0 ] ; then
        poweroff
        sleep 5
fi

# Monitor the green powerbutton
/mnt/sd0/bin/powerbuttond &
#/mnt/sd0/bin/pwrd &
#/mnt/sd0/bin/litepwrd &

# Setup some environment variables
export PATH=/mnt/sd0/bin:$PATH
export TERM=xterm-color
export TERMINFO=/etc/terminfo
export HOME=/mnt/sd0/home
export SDL_NOMOUSE=1
export SDL_VIDEO_FBCON_ROTATION="CCW"

# Create some symbolic links to our upgraded busybox
/mnt/sd0/bin/busybox.sh

# Load kernel modules
insmod /mnt/sd0/modules/zipit2_kbd_helper.ko
insmod /mnt/sd0/modules/pxa27x_keyboard.ko
insmod /mnt/sd0/modules/softcursor.ko
insmod /mnt/sd0/modules/fbcon_ud.ko
insmod /mnt/sd0/modules/fbcon_cw.ko
insmod /mnt/sd0/modules/fbcon_ccw.ko
insmod /mnt/sd0/modules/fbcon_rotate.ko
insmod /mnt/sd0/modules/bitblit.ko
insmod /mnt/sd0/modules/font.ko
insmod /mnt/sd0/modules/fbcon.ko
insmod /mnt/sd0/modules/i2c-core.ko
insmod /mnt/sd0/modules/i2c-algo-bit.ko
insmod /mnt/sd0/modules/i2c-dev.ko
insmod /mnt/sd0/modules/i2c-pxa.ko
insmod /lib/gpio_driver.ko

# Create and mount the sysfs
mkdir /sys
mount -t sysfs sysfs /sys

# Rotate the fbcon
echo 3 > /sys/class/graphics/fbcon/rotate

# Graphic logo
#/mnt/sd0/bin/busybox fbsplash -s /mnt/sd0/etc/logo.ppm -c
#sleep 5

# Make device nodes in /dev
mknod /dev/tty0 c 4 0
rm /dev/tty
mknod /dev/tty c 5 0 
mknod /dev/ptmx c 5 2
chmod 666 /dev/ptmx
mkdir /dev/pts
mount -t devpts devpts /dev/pts
mknod /dev/ttyp0 c 3 0
mknod /dev/ptyp0 c 2 0
mknod /dev/ttyp1 c 3 1
mknod /dev/ptyp1 c 2 1
mknod /dev/ttyp2 c 3 2
mknod /dev/ptyp2 c 2 2
mknod /dev/ttyp3 c 3 3
mknod /dev/ptyp3 c 2 3
mknod /dev/ttyp4 c 3 4
mknod /dev/ptyp4 c 2 4
mknod /dev/pty c 2 176
mknod /dev/ptya0 c 2 177
mknod /dev/tty1 c 4 1
mknod /dev/tty2 c 4 2
mknod /dev/tty3 c 4 3
mknod /dev/tty4 c 4 4
mknod /dev/tty5 c 4 5
mknod /dev/tty6 c 4 6
mknod /dev/tty7 c 4 7
mknod /dev/tty8 c 4 8
mknod /dev/i2c-0 c 89 0 -m 666
mknod /dev/i2c-1 c 89 1 -m 666

# Tidy up all the various directory links
ln -s /mnt/sd0/etc/* /etc/
ln -s /mnt/sd0/var /var
ln -s /mnt/sd0/lib/* /lib
if test ! -d /usr
then
        mkdir /usr
fi
ln -s /mnt/sd0/lib /usr/lib
if test ! -d /usr/local
then 
	mkdir -p /usr/local
fi 
ln -s /mnt/sd0/lib /usr/local/lib
ln -s /mnt/sd0/bin /usr/local/bin
ln -s /mnt/sd0/bin /usr/local/sbin
ln -s /mnt/sd0/etc /use/local/etc
ln -s /mnt/sd0/share /share
ln -s /mnt/sd0/share/* /usr/share/
ln -s /mnt/sd0/share /usr/local/share
ln -s /mnt/sd0/home/.bashrc /etc/profile

# Start the powerdaemon
/mnt/sd0/bin/powerd &

# Remap the keyboard
#loadkeys < /mnt/sd0/etc/keymap.map 
loadkmap < /mnt/sd0/etc/keymap.kmap 

# Load a readable small font -- gives us 53 colums give or take
/mnt/sd0/bin/setfont /mnt/sd0/share/fonts/iz2slat.psf

cd ~
# Turn off the broken console screen blanker
echo "\\033[9;0]" >/dev/tty0
cls > /dev/tty0
cp /mnt/sd0/bin/bash /bin/bash


##########################
# Boot right into rockbox for music in the car.
ln -s /mnt/sd0/music /audio
kbbrightness 511 63 50
#/mnt/sd0/bin/gmenu
/mnt/sd0/bin/rockthecar
#
# Done with rockbox. 
##########################


# Load the wireless driver.
ifconfig lo up
insmod /lib/gspi.ko
insmod /lib/gspi8686.ko helper_name=/lib/firmware/helper_gspi.bin fw_name=/lib/firmware/gspi8686.bin

if test ! -f /mnt/sd0/etc/.macaddr
then
	if test -f /mnt/ffs/properties.txt
	then
		z2macaddr=`grep MAC /mnt/ffs/properties.txt | sed 's/SETUP : MAC=//;s/://g'`
		echo $z2macaddr > /mnt/sd0/etc/.macaddr
	else
	# We will have to create a random mac address.
		lower=100000
		upper=999999
		MAXvalue=$(($upper-$lower))
		i=0
		while [ $i -lt 1 ] ; do
			z2macaddr="001D04"$(($lower+($RANDOM%$MAXvalue))) 
			echo $z2macaddr > /mnt/sd0/etc/.macaddr
			let "i += 1"
		done
	fi
fi
macaddr=`cat /mnt/sd0/etc/.macaddr`
ifconfig eth0 hw ether $macaddr

# Set lcd backlight and turn up key lights.
lcdbrightness 511 63 500
kbbrightness 511 63 50
rightledoff

if test ! -f /mnt/sd0/etc/dropbear_dss_host_key
then
	cls > /dev/tty0
	echo Making ssh-dss host key.... > /dev/tty0
	echo "(*this will take a while*)" > /dev/tty0
	dropbearkey -t dss -f /mnt/sd0/etc/dropbear_dss_host_key > /dev/tty0
fi
if test ! -f /mnt/sd0/etc/dropbear_rsa_host_key
then
	cls > /dev/tty0
	echo Making ssh-rsa host key... > /dev/tty0
	echo "(*this also takes a while*)" > /dev/tty0
	dropbearkey -t rsa -f /mnt/sd0/etc/dropbear_rsa_host_key > /dev/tty0
fi

dropbear -E -d /mnt/sd0/etc/dropbear_dss_host_key -r /mnt/sd0/etc/dropbear_rsa_host_key

/mnt/sd0/bin/setup-keys.sh > /dev/tty0 < /dev/tty0 
#/mnt/sd0/bin/setup-mouse.sh > /dev/tty0 < /dev/tty0 

# dbclient wants to be here
ln -s /mnt/sd0/bin/dbclient /usr/bin/dbclient
# Fix /etc/tz for FAT filesystem on SD card.
ln -s /mnt/sd0/etc/tz /etc/TZ

clear > /dev/tty0
/mnt/sd0/bin/setfont /mnt/sd0/share/fonts/iz2slat.psf


##########################
# Run a shell on 1 and gmenu2x on 2
#own-tty /dev/tty1 /bin/bash -bash &
own-tty /dev/tty1 /bin/sh -sh &

# Rotate fbcons 4,3,2 and un-hide cursor (except on 3)
chvt 4
echo 3 > /sys/class/graphics/fbcon/rotate
echo "\033[?25h" >/dev/tty0
chvt 3
echo 3 > /sys/class/graphics/fbcon/rotate
#echo "\033[?25h" >/dev/tty0
chvt 2
echo 3 > /sys/class/graphics/fbcon/rotate
echo "\033[?25h" >/dev/tty0

# Turn on the option key toggled "mouse" (and LCD,KBD backlight controls).
/mnt/sd0/bin/setup-mouse.sh
# Then run gmenu2x.
own-tty /dev/tty2 /bin/sh sh --login -c "/mnt/sd0/bin/gmenu"

##########################
# We ALREADY launched a shell on tty1 above, so...
# Start a shell on tty2 just in case of gmenu2x crash.
chvt 2

##########################
#  Now boot into IZ2S.  Start with graphic logo.
/mnt/sd0/bin/busybox fbsplash -s /mnt/sd0/etc/logo.ppm -c
sleep 5
clear > /dev/tty0
. /mnt/sd0/etc/brand.cfg
Load a larger font so that the dialog screens are readable
/mnt/sd0/bin/setfont /mnt/sd0/share/fonts/LatArCyrHeb-16.psf


##########################
# Ask about WIFI
dialog --backtitle "$backtitle" --title "Setup Wifi"  --yesno "Setup Wifi? If you say no here you may set it up later." 0 0 >/dev/tty0 </dev/tty0
if [ $? -eq 0 ]  ; then
#	/mnt/sd0/bin/setup-wifi.sh > /dev/tty0 < /dev/tty0 
	/mnt/sd0/bin/setup-wifi.iz2se.sh > /dev/tty0 < /dev/tty0 
fi

##########################
#Last resort = bash on tty0.
own-tty /dev/tty0 /bin/bash -bash 
echo "Shutting down..." > /dev/tty0
poweroff
