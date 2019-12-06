# This file is part of GNU Mailutils.
# Copyright (C) 2010-2019 Free Software Foundation, Inc.
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

## -------------------------- ##
## Non-installable programs
## -------------------------- ##

AM_CPPFLAGS = @MU_LIB_COMMON_INCLUDES@ 
noinst_PROGRAMS = \
 bs\
 cwdrepl\
 fldel\
 lstuid\
 mbdel\
 msgset\
 mimetest\
 smtpsend\
 ufms

LDADD = \
 ${MU_LIB_MAILBOX}\
 ${MU_LIB_MAILER}\
 ${MU_LIB_AUTH}\
 @MU_AUTHLIBS@\
 ${MU_LIB_MAILUTILS}

smtpsend_LDADD = \
 ${MU_LIB_MAILER} \
 @MU_AUTHLIBS@\
 ${MU_LIB_MAILUTILS}

cwdrepl_LDADD = ${MU_LIB_MAILUTILS}

## ------------ ##
## Test suite.  ##
## ------------ ##

TESTSUITE_AT += \
 cwdrepl.at\
 fldel.at\
 lstuid00.at\
 lstuid01.at\
 lstuid02.at\
 mbdel.at\
 mime.at\
 mime01.at\
 smtp-msg.at\
 smtp-str.at\
 ufms.at


