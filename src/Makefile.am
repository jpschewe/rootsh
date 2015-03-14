AUTOMAKE_OPTIONS=subdir-objects
bin_PROGRAMS = rootsh
rootsh_SOURCES = rootsh.c
rootsh_SOURCES += write2syslog.c
rootsh_SOURCES += configParser.c
rootsh_LDADD = @LIBOBJS@

if HAVE_GCOV
AM_CFLAGS = $(COVERAGE_CFLAGS)
AM_CXXFLAGS = $(COVERAGE_CFLAGS)
AM_LDFLAGS = $(COVERAGE_LDFLAGS)

AM_CFLAGS += -DCONFIGFILE="\"@sysconfdir@/rootsh.cfg\""
AM_CXXFLAGS += -DCONFIGFILE="\"@sysconfdir@/rootsh.cfg\""
else
AM_CFLAGS = -DCONFIGFILE="\"@sysconfdir@/rootsh.cfg\""
AM_CXXFLAGS = -DCONFIGFILE="\"@sysconfdir@/rootsh.cfg\""
endif

