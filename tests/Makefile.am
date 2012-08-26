check_PROGRAMS = test_id3

TESTS = test_id3

test_id3_SOURCES = test_id3.c

INCLUDES = -I$(top_srcdir)/include -I/usr/local/include

if MACOSX
test_id3_LDADD = -L/usr/local/lib $(top_srcdir)/librioutil/librioutil.la -lIOKit
PREBIND_FLAGS = -prebind
else
test_id3_LDADD = -L/usr/local/lib $(top_srcdir)/librioutil/librioutil.la
endif

test_id3_LDFLAGS = $(PREBIND_FLAGS)
test_id3_DEPENDENCIES = $(top_srcdir)/librioutil/librioutil.la
