TESTS = testConfigParser

check_PROGRAMS = testConfigParser

testConfigParser_SOURCES = testConfigParser.c $(top_builddir)/src/configParser.c $(top_builddir)/src/configParser.h

if HAVE_GCOV
AM_CFLAGS = $(COVERAGE_CFLAGS)
AM_CXXFLAGS = $(COVERAGE_CFLAGS)
AM_LDFLAGS = $(COVERAGE_LDFLAGS)
endif
