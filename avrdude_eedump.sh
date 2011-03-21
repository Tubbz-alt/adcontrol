#!/bin/sh

CMD="-U eeprom:r:eeprom.eep:r"
echo ".:: Dumping EEPROM ($CMD)..."
sudo /usr/bin/avrdude -C /etc/avrdude.conf -c avrispmkII -p atmega644p -P usb -b 115200 $CMD

echo ".:: EEPROM Content:"
hexdump -C eeprom.eep

