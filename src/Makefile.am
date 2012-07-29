bin_PROGRAMS = rootsh
rootsh_SOURCES = rootsh.c
if LOGTOSYSLOG
rootsh_SOURCES += write2syslog.c
endif
rootsh_SOURCES += configParser.c
rootsh_LDADD = @LIBOBJS@

AM_CFLAGS = -DCONFIGFILE="\"@sysconfdir@/rootsh.cfg\""
