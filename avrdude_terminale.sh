#!/bin/sh

CMD="-t"
echo "Entering Device Terminal ($CMD)..."
sudo /usr/bin/avrdude -C /etc/avrdude.conf -c avrispmkII -p atmega644p -P usb -b 115200 -t

