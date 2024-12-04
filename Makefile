# #############################################################################
# logue-sdk Oscillator Makefile
# #############################################################################

SUBDIRS = minilogue-xd prologue nutekt-digital
#SUBDIRS = minilogue-xd

default: all

$(SUBDIRS)::
	$(MAKE) -C $@ $(MAKECMDGOALS)

all clean install : $(SUBDIRS)
