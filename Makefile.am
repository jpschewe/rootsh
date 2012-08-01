include $(top_srcdir)/Makefile.am.coverage

SUBDIRS = src test
EXTRA_DIST = contrib src/gnugetopt.h src/getopt.c src/getopt1.c src/write2syslog.h test.sh rootsh.1
man_MANS = rootsh.1

# needs to be in all Makefile.am's that need GCOV flags
if HAVE_GCOV
AM_CFLAGS = $(COVERAGE_CFLAGS)
AM_CXXFLAGS = $(COVERAGE_CFLAGS)
AM_LDFLAGS = $(COVERAGE_LDFLAGS)
endif

dist-hook:
	for cvsdir in `find $(distdir) -name CVS -print`; do \
	  rm -rf $$cvsdir ; \
	done ;
