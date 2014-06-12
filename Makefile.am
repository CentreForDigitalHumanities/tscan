# $Id: Makefile.am 7708 2011-01-05 13:47:18Z sloot $
# $URL: https://ilk.uvt.nl/svn/trunk/sources/Timbl6/Makefile.am $

ACLOCAL_AMFLAGS =-I m4 --install

SUBDIRS = src include docs data config webservice

EXTRA_DIST = bootstrap.sh AUTHORS TODO NEWS tscan.cfg.example

ChangeLog: NEWS
	svn update; svn2cl --include-rev
