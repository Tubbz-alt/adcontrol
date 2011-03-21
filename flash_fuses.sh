#!/bin/sh

if [ $# -lt 3 ]; then
	echo "Usage: $0 Low High Ext"
	exit 1
fi

echo "Write fuses (L:0x$1, H:0x$2, E:0x$3)? [N,y] "
read resp

[ "x"$resp != "xy" ] && exit 0

#echo "-U lfuse:w:0x$1:m -U hfuse:w:0x$2:m -U efuse:w:0x$3:m"
sudo /usr/bin/avrdude -C /etc/avrdude.conf -c avrispmkII -p atmega644p -P usb -b 115200 -U lfuse:w:0x$1:m -U hfuse:w:0x$2:m -U efuse:w:0x$3:m


