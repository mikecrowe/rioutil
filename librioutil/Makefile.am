libdir = $(prefix)/lib

INCLUDES = -I$(top_srcdir)/include

EXTRA_DIST = rio.c rioio.c mp3.c downloadable.c byteorder.c \
		cksum.c bsd.c driver_libusb.c playlist.c driver_usbdevfs.c \
		usbdrvlinux.c driver_macosx.c driver_standard.c genre.h \
		song_management.c id3.c

if WITH_USBDEVFS
DRIVER = driver_usbdevfs.c usbdrvlinux.c
FLGS = -g3
else
if WITH_LIBUSB
DRIVER = driver_libusb.c
FLGS = -g3
else
if MACOSX
DRIVER = driver_macosx.c
FLGS = -O3 -traditional-cpp
else
DRIVER = driver_standard.c
FLGS = -O3
endif
endif
endif

LDFLAGS=

# new libtool eliminates the need for seperate OS X section
lib_LTLIBRARIES = librioutil.la

librioutil_la_SOURCES = rio.c rioio.c mp3.c downloadable.c \
			byteorder.c song_management.c cksum.c bsd.c hexdump.c \
			playlist.c id3.c $(DRIVER)

librioutil_la_LDFLAGS = -version-info 5:1:4

librioutil_la_LIBADD =