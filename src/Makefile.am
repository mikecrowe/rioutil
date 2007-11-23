bin_PROGRAMS = rioutil

INCLUDES = -I$(top_srcdir)/include -I/usr/local/include

rioutil_SOURCES = main.c getopt.c getopt1.c main.h getopt.h

if MACOSX
rioutil_LDADD = -L/usr/local/lib $(top_srcdir)/librioutil/librioutil.la -lIOKit
CFLGS = -traditional-cpp
else
rioutil_LDADD = -L/usr/local/lib $(top_srcdir)/librioutil/librioutil.la
endif

rioutil_DEPENDENCIES = $(top_srcdir)/librioutil/librioutil.la
