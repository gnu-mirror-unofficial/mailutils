# This file is part of GNU Mailutils.
# Copyright (C) 2007-2019 Free Software Foundation, Inc.
#
# GNU Mailutils is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 3, or (at
# your option) any later version.
#
# GNU Mailutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>.

EXTRA_DIST = $(TESTSUITE_AT) testsuite package.m4 input.msg
DISTCLEANFILES       = atconfig $(check_SCRIPTS)
MAINTAINERCLEANFILES = Makefile.in $(TESTSUITE)

## ------------ ##
## package.m4.  ##
## ------------ ##

$(srcdir)/package.m4: $(top_srcdir)/configure.ac
	$(AM_V_GEN){                                      \
	  echo '# Signature of the current package.'; \
	  echo 'm4_define([AT_PACKAGE_NAME],      [@PACKAGE_NAME@])'; \
	  echo 'm4_define([AT_PACKAGE_TARNAME],   [@PACKAGE_TARNAME@])'; \
	  echo 'm4_define([AT_PACKAGE_VERSION],   [@PACKAGE_VERSION@])'; \
	  echo 'm4_define([AT_PACKAGE_STRING],    [@PACKAGE_STRING@])'; \
	  echo 'm4_define([AT_PACKAGE_BUGREPORT], [@PACKAGE_BUGREPORT@])'; \
	} >$(srcdir)/package.m4

#

## ------------ ##
## Test suite.  ##
## ------------ ##

TESTSUITE_AT = \
 lmtpd.at\
 mda.at\
 forward.at\
 putmail.at\
 testsuite.at

TESTSUITE = $(srcdir)/testsuite
M4=m4

AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): package.m4 $(TESTSUITE_AT) $(top_srcdir)/testsuite/testsuite.inc
	$(AM_V_GEN)$(AUTOTEST) -I $(srcdir) -I $(top_srcdir)/testsuite testsuite.at -o $@.tmp
	$(AM_V_at)mv $@.tmp $@

atconfig: $(top_builddir)/config.status
	cd $(top_builddir) && ./config.status tests/$@

clean-local:
	@test ! -f $(TESTSUITE) || $(SHELL) $(TESTSUITE) --clean

check-local: atconfig atlocal $(TESTSUITE) 
	@$(SHELL) $(TESTSUITE)

noinst_PROGRAMS = testmda
testmda_SOURCES = testmda.c
testmda.c: ${srcdir}/../mda.c
	cp ${srcdir}/../mda.c testmda.c
BUILT_SOURCES = testmda.c
if MU_COND_DBM
 LIBMU_DBM=${top_builddir}/libmu_dbm/libmu_dbm.la
endif
testmda_LDADD = \
 ${builddir}/../lib/libmda.a\
 ${top_builddir}/lib/libmuscript.a\
 ${MU_APP_LIBRARIES}\
 ${MU_LIB_SIEVE}\
 ${MU_LIB_MAILBOX}\
 ${MU_LIB_AUTH}\
 ${MU_LIB_MAILER}\
 @MU_AUTHLIBS@\
 ${MU_LIB_MAILUTILS} \
 @MU_COMMON_LIBRARIES@\
 @LIBMU_SCM@\
 @LIBMU_SCM_DEPS@\
 @MU_LIB_PY@\
 $(LIBMU_DBM)\
 @GUILE_LIBS@\
 @PYTHON_LIBS@\
 @DBMLIBS@

testmda_CPPFLAGS = \
 -I${top_srcdir}\
 @MU_APP_COMMON_INCLUDES@\
 -I${srcdir}/../lib\
 -DTESTSUITE_CONFIG_FILE=\"testmda.conf\"

# Run the test suite on the *installed* tree.
#installcheck-local:
#	$(SHELL) $(TESTSUITE) AUTOTEST_PATH=$(exec_prefix)/bin


