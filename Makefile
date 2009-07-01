######################################################################
# Makefile for qiv
# User Options
######################################################################

# Directory where qiv will be installed under.
PREFIX = /usr/local

# Font to use for statusbar in fullscreen mode
STATUSBAR_FONT = "fixed"

# Cursor to use on qiv windows - see
# /usr/X11R6/include/X11/cursorfont.h for more choices.
CURSOR = 84

# Should image be centered on screen by default? 1=yes, 0=no.
CENTER = 1

# Should images be filtered by extension? 1=yes, 0=no.
FILTER = 1

# This sets the file extentions to filter on (other file types will be
# skipped.) It should reflect whatever is compiled into imlib.
EXTNS = GIF TIFF XPM XBM PNG PPM PNM PGM PCX BMP EIM JPEG TGA

# Comment this line out if your system doesn't have getopt_long().
GETOPT_LONG = -DHAVE_GETOPT_LONG

# This program will be run on the manual page after it is installed.
# If you don't want to compress the manpage, change it to 'true'.
COMPRESS_PROG = gzip -9f 

######################################################################

# The following only apply to 'make install-xscreensaver':
# Delay in minutes to start xscreensaver
SS_TIMEOUT = 5

# Delay in minutes to cycle between xscreensaver programs
SS_CYCLE = 5

# Delay in seconds to wait between images
SS_DELAY = 5

# Image files to display
SS_IMAGES = ~/pictures/*.jpg

# Comment out this line to have pictures be dislayed sequentially
SS_RANDOMIZE = -r

######################################################################
# Variables and Rules
# Do not edit below here!
######################################################################

CC        = gcc
CFLAGS    = -O2 -Wall -fomit-frame-pointer -finline-functions \
	    -fcaller-saves -ffast-math -fno-strength-reduce \
	    -fthread-jumps #-march=pentium #-DSTAT_MACROS_BROKEN

INCLUDES  = `imlib-config --cflags-gdk`
LIBS      = `imlib-config --libs-gdk`

PROGRAM   = qiv
OBJS      = main.o image.o event.o options.o utils.o
HEADERS   = qiv.h
DEFINES   = $(patsubst %,-DEXTN_%, $(EXTNS)) \
            $(GETOPT_LONG) \
            -DSTATUSBAR_FONT='$(STATUSBAR_FONT)' \
            -DCENTER=$(CENTER) \
            -DFILTER=$(FILTER) \
            -DCURSOR=$(CURSOR)

ifndef GETOPT_LONG
OBJS     += lib/getopt.o lib/getopt1.o
OBJS_G   += lib/getopt.g lib/getopt1.g
endif

PROGRAM_G = qiv-g
OBJS_G    = $(OBJS:.o=.g)
DEFINES_G = $(DEFINES) -DDEBUG

SS_PROG   = $(PREFIX)/ss-qiv

######################################################################

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(LIBS) $(OBJS) -o $(PROGRAM)

$(OBJS): %.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -o $@

main.o: main.h

######################################################################

debug: $(PROGRAM_G)

$(PROGRAM_G): $(OBJS_G)
	$(CC) -g $(CFLAGS) $(DEFINES_G) $(LIBS) $(OBJS_G) -o $(PROGRAM_G)

$(OBJS_G): %.g: %.c $(HEADERS)
	$(CC) -c -g $(CFLAGS) $(DEFINES_G) $(INCLUDES) $< -o $@

######################################################################

clean :  
	@echo "Cleaning up..."
	rm -f $(PROGRAM) $(PROGRAM_G) $(OBJS) $(OBJS_G)

install: $(PROGRAM)
	@echo "Installing..."
	install -s -m 0755 $(PROGRAM) $(PREFIX)/bin
	install -m 0644 $(PROGRAM).1 $(PREFIX)/man/man1
	$(COMPRESS_PROG) $(PREFIX)/man/man1/$(PROGRAM).1
	@if ./qiv -o white -f ./intro.jpg ; \
	then echo "-- Test Passed --" ; \
	else echo "-- Test Failed --" ; \
	fi

install-xscreensaver: install
	@echo "#!/bin/sh" > $(SS_PROG)
	@echo "xhost +`hostname` 1> /dev/null" >> $(SS_PROG)
	@echo "xrdb < ~/.qivrc" >> $(SS_PROG)
	@echo "xscreensaver 1> /dev/null &" >>  $(SS_PROG)
	@echo "xscreensaver.timeout: $(SS_TIMEOUT)" > ~/.qivrc
	@echo "xscreensaver.cycle: $(SS_CYCLE)" >> ~/.qivrc
	@echo "xscreensaver.programs: qiv -iftsd $(SS_DELAY) $(SS_RANDOMIZE) $(SS_IMAGES)" >> ~/.qivrc
	echo "Start screensaver with ss-qiv"

# the end... ;-)