#
# User makefile.
# Edit this file to change compiler options and related stuff.
#

# Programmer interface configuration, see http://dev.bertos.org/wiki/ProgrammerInterface for help
ade_PROGRAMMER_TYPE = avrispmkII
ade_PROGRAMMER_PORT = usb

# Files included by the user.
ade_USER_CSRC = \
	$(ade_SRC_PATH)/command.c \
	$(ade_SRC_PATH)/console.c \
	$(ade_SRC_PATH)/eeprom.c \
	$(ade_SRC_PATH)/gsm.c \
	$(ade_SRC_PATH)/control.c \
	$(ade_SRC_PATH)/main.c \
	$(ade_SRC_PATH)/signals.c \
	#

# Files included by the user.
ade_USER_PCSRC = \
	#

# Files included by the user.
ade_USER_CPPASRC = \
	#

# Files included by the user.
ade_USER_CXXSRC = \
	#

# Files included by the user.
ade_USER_ASRC = \
	#

# Flags included by the user.
ade_USER_LDFLAGS = \
	#

# Flags included by the user.
ade_USER_CPPAFLAGS = \
	#

# Flags included by the user.
ade_USER_CPPFLAGS = \
	-fno-strict-aliasing \
	-fwrapv \
	#
