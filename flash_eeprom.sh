#!/bin/sh

if [ $# -lt 1 ]; then
	echo "Usage: $0 image.eep"
	exit 1
fi

echo -n "Write EEPROM ($1)? [N,y] "
read resp

[ "x"$resp != "xy" ] && exit 0

CMD="-U eeprom:w:$1"
echo ".:: Flashing EEPROM ($CMD)..."
sudo /usr/bin/avrdude -C /etc/avrdude.conf -c avrispmkII -p atmega644p -P usb -b 115200 $CMD

CMD="-U eeprom:v:$1"
echo ".:: Checking EEPROM ($CMD)..."
sudo /usr/bin/avrdude -C /etc/avrdude.conf -c avrispmkII -p atmega644p -P usb -b 115200 $CMD

