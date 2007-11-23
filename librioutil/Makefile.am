libdir = $(prefix)/lib

INCLUDES = -I$(top_srcdir)/include

EXTRA_DIST = rio.c rioio.c mp3.c wma.c downloadable.c byteorder.c \
		cksum.c bsd.c driver_libusb.c playlist.c driver_usbdevfs.c \
		usbdrvlinux.c driver_macosx.c driver_standard.c genre.h \
		song_managment.c mp3tech.c mp3tech.h id3.c

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
LDFLGS = -framework "CoreFoundation"
else
DRIVER = driver_standard.c
FLGS = -O3
endif
endif
endif

CFLAGS = $(FLGS)

# new libtool eliminates the need for seperate OS X section
lib_LTLIBRARIES = librioutil.la

librioutil_la_SOURCES = rio.c rioio.c mp3.c mp3tech.c wma.c downloadable.c \
			byteorder.c song_managment.c cksum.c bsd.c hexdump.c \
			playlist.c id3.c $(DRIVER)

librioutil_la_LDFLAGS = -version-info 5:1:4 $(LDFLGS)

librioutil_la_LIBADD =