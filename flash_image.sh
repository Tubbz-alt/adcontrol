#!/bin/sh

#avr-size --mcu=atmega644p -C images/ade.elf

if [ $# -lt 1 ]; then
	echo "Usage: $0 image.hex"
	exit 1
fi

echo -n "Flash device image ($1)? [N,y] "
read resp

[ "x"$resp != "xy" ] && exit 0

CMD="-e -U flash:w:$1:i"
echo "Flashing device ($CMD)..."

sudo /usr/bin/avrdude -C /etc/avrdude.conf -c avrispmkII -p atmega644p -P usb -b 115200 $CMD

