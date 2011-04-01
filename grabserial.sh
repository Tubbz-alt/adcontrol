#!/bin/sh

xterm -bg black -fg gray -geometry 106x71+3+46 -title "GrabSerial" -e grabserial -d /dev/ttyUSB0 -b 115200 -w 8 -p N -s 1 -t -m "\*\*\* BeRTOS" &
