ACLOCAL_AMFLAGS =-I m4 --install

SUBDIRS = src include docs data webservice

EXTRA_DIST = bootstrap.sh AUTHORS NEWS tscan.cfg.example downloaddata.sh

ChangeLog: NEWS
	git pull; git2cl > ChangeLog
