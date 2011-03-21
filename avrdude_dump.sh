#!/bin/sh

echo ".:: Dumping Device Configuration..."
sudo /usr/bin/avrdude -v -C /etc/avrdude.conf -c avrispmkII -p atmega644p -P usb -b 115200

