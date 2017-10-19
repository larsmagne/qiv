#######################################################################
# Makefile for qiv - Quick Image Viewer - http://qiv.spiegl.de/
# User Options
#######################################################################

# Directory where qiv will be installed under.
PREFIX = /usr/local

# Fonts to use for statusbar and comments
STATUSBAR_FONT = "Monospace 9"
COMMENT_FONT = "Monospace 20"

# Cursor to use on qiv windows - see
# /usr/X11R6/include/X11/cursorfont.h for more choices.
CURSOR = 84

# Should image be centered on screen by default? 1=yes, 0=no.
CENTER = 1

# Should images be filtered by extension? 1=yes, 0=no.
FILTER = 1

# This sets the file extentions to filter on (other file types will be
# skipped.) It should reflect whatever is compiled into imlib.
# The latest version of imlib has removed imagemagick fallback support,
# so some extensions (XBM TGA) have been removed.
EXTNS = GIF TIFF XPM PNG PPM PNM PGM PCX BMP EIM JPEG SVG WMF ICO

# Comment this line out if your system doesn't have getopt_long().
GETOPT_LONG = -DHAVE_GETOPT_LONG

# This program will be run on the manual page after it is installed.
# If you don't want to compress the manpage, change it to 'true'.
COMPRESS_PROG = gzip -9f

# Comment this line out if your system doesn't have lcms2 installed
# (for minimal Color Management support)
LCMS = -DSUPPORT_LCMS

# Comment this line out if you do not want to use libmagic to
# identify if a file is an image
MAGIC = -DHAVE_MAGIC

# Comment this line out if you do not want to use libexif to
# display the exif contents of a jpg 
EXIF = -DHAVE_EXIF

######################################################################
# Variables and Rules
# Do not edit below here!
######################################################################

CC        = gcc
PKG_CONFIG ?= pkg-config
#CFLAGS    = -O0 -g -Wall
CFLAGS    = -O2 -Wall \
	    -fcaller-saves -ffast-math -fno-strength-reduce \
	    -fthread-jumps #-march=pentium #-DSTAT_MACROS_BROKEN
#CFLAGS    = -O2 -Wall -fomit-frame-pointer -finline-functions \
#	    -fcaller-saves -ffast-math -fno-strength-reduce \
#	    -fthread-jumps #-march=pentium #-DSTAT_MACROS_BROKEN

INCLUDES  := $(shell $(PKG_CONFIG) --cflags gdk-2.0 imlib2)
LIBS      := $(shell $(PKG_CONFIG) --libs gdk-2.0 imlib2) -lX11 -lXext -lgio-2.0

# [as] thinks that this is not portable enough:
# [lc] I use a virtual screen of 1600x1200, and the resolution is 1024x768,
# so I changed (in main.c) how screen_[x,y] is obtained; it seems that gtk
# 1.2 cannot give the geometry of viewport, so I borrowed from the source
# of xvidtune the code for calling XF86VidModeGetModeLine, this requires
# the linking option -lXxf86vm.
#LIBS      += -lXxf86vm

PROGRAM   = qiv
OBJS      = main.o image.o event.o options.o utils.o xmalloc.o
HEADERS   = qiv.h
DEFINES   = $(patsubst %,-DEXTN_%, $(EXTNS)) \
            $(GETOPT_LONG) \
            -DSTATUSBAR_FONT='$(STATUSBAR_FONT)' \
            -DCOMMENT_FONT='$(COMMENT_FONT)' \
            -DCENTER=$(CENTER) \
            -DFILTER=$(FILTER) \
            -DCURSOR=$(CURSOR) \
            $(MAGIC) \
            $(EXIF) \
            $(LCMS)

ifndef GETOPT_LONG
OBJS     += lib/getopt.o lib/getopt1.o
OBJS_G   += lib/getopt.g lib/getopt1.g
endif

ifdef LCMS
INCLUDES  += $(shell $(PKG_CONFIG) --cflags lcms2)
LIBS      += $(shell $(PKG_CONFIG) --libs lcms2) -ljpeg -ltiff
endif

ifdef EXIF
INCLUDES  += $(shell $(PKG_CONFIG) --cflags libexif)
LIBS      += $(shell $(PKG_CONFIG) --libs libexif)
endif

ifdef MAGIC
LIBS    += -lmagic
endif

PROGRAM_G = qiv-g
OBJS_G    = $(OBJS:.o=.g)
DEFINES_G = $(DEFINES) -DDEBUG

######################################################################

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(OBJS) $(LIBS) -o $(PROGRAM)

$(OBJS): %.o: %.c $(HEADERS) Makefile
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -o $@

main.o: main.h

######################################################################

debug: $(PROGRAM_G)

$(PROGRAM_G): $(OBJS_G)
	$(CC) -g $(CFLAGS) $(DEFINES_G) $(OBJS_G) $(LIBS) -o $(PROGRAM_G)

$(OBJS_G): %.g: %.c $(HEADERS)
	$(CC) -c -g $(CFLAGS) $(DEFINES_G) $(INCLUDES) $< -o $@

######################################################################

clean :
	@echo "Cleaning up..."
	rm -f $(OBJS) $(OBJS_G)

distclean : clean
	rm -f $(PROGRAM) $(PROGRAM_G)

install: $(PROGRAM)
	@echo "Installing QIV..."
	@if [ ! -e $(PREFIX)/bin ]; then \
	  install -d -m 0755 $(PREFIX)/bin; \
	  echo install -d -m 0755 $(PREFIX)/bin; \
        fi
	install -s -m 0755 $(PROGRAM) $(PREFIX)/bin
	@if [ ! -e $(PREFIX)/man/man1 ]; then \
	  echo install -d -m 0755 $(PREFIX)/man/man1; \
	  install -d -m 0755 $(PREFIX)/man/man1; \
	fi
	install -m 0644 $(PROGRAM).1 $(PREFIX)/share/man/man1
	$(COMPRESS_PROG) $(PREFIX)/share/man/man1/$(PROGRAM).1
	@if [ ! -e $(PREFIX)/share/pixmaps ]; then \
	  echo install -d -m 0755 $(PREFIX)/share/pixmaps; \
	  install -d -m 0755 $(PREFIX)/share/pixmaps; \
	fi
	install -m 0644 qiv.png $(PREFIX)/share/pixmaps/qiv.png
	@if [ ! -e $(PREFIX)/share/applications ]; then \
	  echo install -d -m 0755 $(PREFIX)/share/applications; \
	  install -d -m 0755 $(PREFIX)/share/applications; \
	fi
	install -m 0644 qiv.desktop $(PREFIX)/share/applications/qiv.desktop
	@if ./qiv -f ./intro.jpg ; \
	then echo "-- Test Passed --" ; \
	else echo "-- Test Failed --" ; \
	fi
	@echo "\nDont forget to look into the \"qiv-command\" file and install it!\n-> cp qiv-command.example $(PREFIX)/bin/qiv-command\n\n"

# the end... ;-)
