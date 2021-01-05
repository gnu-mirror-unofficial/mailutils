# This file is part of GNU Mailutils.
# Copyright (C) 2007-2021 Free Software Foundation, Inc.
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

include $(top_srcdir)/testsuite/testsuite.am
AUTOTEST_INCLUDES += -I $(srcdir)/../../tests
TESTSUITE_AT += mda.at forward.at

noinst_PROGRAMS = testmda
testmda_SOURCES = testmda.c
testmda.c: $(srcdir)/../mda.c
	cp $(srcdir)/../mda.c testmda.c
BUILT_SOURCES = testmda.c
if MU_COND_DBM
 LIBMU_DBM=$(top_builddir)/libmu_dbm/libmu_dbm.la
endif
LDADD = \
 $(builddir)/../../lib/libmda.a\
 $(top_builddir)/lib/libmuscript.a\
 $(MU_APP_LIBRARIES)\
 $(MU_LIB_SIEVE)\
 $(MU_LIB_MAILBOX)\
 $(MU_LIB_AUTH)\
 $(MU_LIB_MAILER)\
 $(MU_AUTHLIBS)\
 $(MU_LIB_MAILUTILS) \
 @LIBMU_SCM@\
 @LIBMU_SCM_DEPS@\
 $(MU_LIB_PY)\
 $(LIBMU_DBM)\
 @GUILE_LIBS@\
 @PYTHON_LIBS@\
 @DBMLIBS@\
 $(MU_COMMON_LIBRARIES)

AM_CPPFLAGS = \
 -I$(top_srcdir)\
 $(MU_APP_COMMON_INCLUDES)\
 -I$(srcdir)/../../lib\
 -DTESTSUITE_CONFIG_FILE=\"testmda.conf\"



