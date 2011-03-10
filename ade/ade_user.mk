#
# User makefile.
# Edit this file to change compiler options and related stuff.
#

# Programmer interface configuration, see http://dev.bertos.org/wiki/ProgrammerInterface for help
ade_PROGRAMMER_TYPE = none
ade_PROGRAMMER_PORT = none

# Files included by the user.
ade_USER_CSRC = \
	$(ade_SRC_PATH)/main.c \
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
