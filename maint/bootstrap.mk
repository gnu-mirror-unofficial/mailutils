# SYNOPSIS
#   make bootstrap
#
# DESRIPTION
#   Bootstrap a freshly cloned copy of mailutils.
#

ChangeLog: Makefile
	$(MAKE) ChangeLog

Makefile: Makefile.am configure
	./configure

configure: configure.ac
	./bootstrap
