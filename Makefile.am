SUBDIRS = src
EXTRA_DIST = contrib src/gnugetopt.h src/getopt.c src/getopt1.c

dist-hook:
	for cvsdir in `find $(distdir) -name CVS -print`; do \
	  rm -rf $$cvsdir ; \
	done ;
