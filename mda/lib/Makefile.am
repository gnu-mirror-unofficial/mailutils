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

noinst_LIBRARIES = libmda.a
libmda_a_SOURCES = \
 deliver.c\
 forward.c\
 script.c\
 util.c

noinst_HEADERS = libmda.h

# # FIXME @MU_TCPWRAP_LIBRARIES@

AM_CPPFLAGS = -I${top_srcdir} @MU_APP_COMMON_INCLUDES@ @GUILE_INCLUDES@ \
 @PYTHON_INCLUDES@

if MU_COND_MAILBOX_QUOTA
  libmda_a_SOURCES += mailquota.c
  AM_CPPFLAGS += "-DUSE_MAILBOX_QUOTAS=1"
endif

