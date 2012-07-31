bin_PROGRAMS = rootsh
rootsh_SOURCES = rootsh.c
rootsh_SOURCES += write2syslog.c
rootsh_SOURCES += configParser.c
rootsh_LDADD = @LIBOBJS@

AM_CFLAGS = -DCONFIGFILE="\"@sysconfdir@/rootsh.cfg\""
