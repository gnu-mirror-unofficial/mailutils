# This file is part of GNU Mailutils.
# Copyright (C) 2010-2020 Free Software Foundation, Inc.
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

include $(srcdir)/testsuite.am

EXTRA_DIST += \
 package.m4 \
 testsuite.inc\
 testsuite.am\
 lib\
 etc\
 spool\
 folder\
 maildir\
 mbop.c\
 mh 

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

noinst_DATA = package.m4

## -------------------------- ##
## Non-installable programs   ##
## -------------------------- ##

AM_CPPFLAGS = $(MU_LIB_COMMON_INCLUDES) 
noinst_PROGRAMS = \
 cwdrepl\
 mbox2dir\
 mockmail\
 mockmta

cwdrepl_LDADD =
mockmta_LDADD = $(TLS_LIBS)

## ------------ ##
## Test suite.  ##
## ------------ ##

TESTSUITE_AT += \
 cwdrepl.at\
 mockmail.at\
 mockmta.at


