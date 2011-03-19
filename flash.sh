#!/bin/sh
sudo /usr/bin/avrdude -C /etc/avrdude.conf -c avrispmkII -p atmega644p -P usb -b 115200 -e -U flash:w:images/ade.hex:i -v
