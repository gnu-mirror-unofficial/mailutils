# This file is part of GNU Mailutils.
# Copyright (C) 2011-2019 Free Software Foundation, Inc.
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

AM_CPPFLAGS = $(MU_LIB_COMMON_INCLUDES) 
noinst_PROGRAMS = ckiconv
LDADD = $(MU_LIB_MAILUTILS)

TESTSUITE_AT += \
 anystate.at\
 append00.at\
 append01.at\
 close-expunge.at\
 create01.at\
 create02.at\
 examine.at\
 expunge.at\
 fetch.at\
 id.at\
 IDEF0955.at\
 IDEF0956.at\
 list.at\
 search.at\
 select.at\
 status.at


