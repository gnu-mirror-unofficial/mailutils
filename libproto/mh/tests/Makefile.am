# This file is part of GNU Mailutils.
# Copyright (C) 2019-2020 Free Software Foundation, Inc.
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

## -------------------------- ##
## Non-installable programs
## -------------------------- ##

AM_CPPFLAGS = \
 $(MU_LIB_COMMON_INCLUDES) \
 -I$(top_srcdir)\
 -I$(top_srcdir)/libmailutils/tests

noinst_PROGRAMS = \
 mbop

LDADD =  -L$(top_builddir)/libmailutils/tests -lmu_tesh $(MU_LIB_MH) $(MU_LIB_MAILUTILS)

## ------------ ##
## Test suite.  ##
## ------------ ##

TESTSUITE_AT += \
 append.at\
 attr.at\
 autodetect.at\
 body.at\
 count.at\
 delete.at\
 envelope.at\
 header.at\
 notify.at\
 qget.at\
 uid.at\
 uidnext.at\
 uidvalidity.at\
 uidvol.at




