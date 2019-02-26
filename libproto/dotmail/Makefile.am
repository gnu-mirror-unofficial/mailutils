## This file is part of GNU Mailutils.
## Copyright (C) 2019 Free Software Foundation, Inc.
##
## GNU Mailutils is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as
## published by the Free Software Foundation; either version 3, or (at
## your option) any later version.
##
## GNU Mailutils is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. 

AM_CPPFLAGS = @MU_LIB_COMMON_INCLUDES@ 

lib_LTLIBRARIES = libmu_dotmail.la
libmu_dotmail_la_LDFLAGS=-version-info @VI_CURRENT@:@VI_REVISION@:@VI_AGE@
libmu_dotmail_la_LIBADD = ${MU_LIB_MAILUTILS}
libmu_dotmail_la_SOURCES = \
 dotmail.c\
 message.c\
 folder.c

SUBDIRS = . tests
