SUBDIRS = src
EXTRA_DIST = contrib

dist-hook:
	for cvsdir in `find $(distdir) -name CVS -print`; do \
	  rm -rf $$cvsdir ; \
	done ;
