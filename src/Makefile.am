# $Id$
# $URL$

AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -std=c++0x

bin_PROGRAMS = tscan

tscan_SOURCES = tscan.cxx Alpino.cxx cgn.cxx sem.cxx intensify.cxx formal.cxx conn.cxx general.cxx situation.cxx afk.cxx adverb.cxx ner.cxx wordstats.cxx structstats.cxx sentstats.cxx parstats.cxx docstats.cxx utils.cxx

check_SCRIPTS = \
	test.sh

TESTS = $(check_SCRIPTS)

TESTS_ENVIRONMENT = \
	LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib \
	tscan_bin=@top_srcdir@/src
