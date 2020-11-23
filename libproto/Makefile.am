## This file is part of GNU Mailutils.
## Copyright (C) 2006-2020 Free Software Foundation, Inc.
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

SUBDIRS = mbox mailer

if MU_COND_SUPPORT_POP
  SUBDIRS += pop
endif

if MU_COND_SUPPORT_IMAP
  SUBDIRS += imap
endif

if MU_COND_SUPPORT_MH
  SUBDIRS += mh
endif

if MU_COND_SUPPORT_MAILDIR
  SUBDIRS += maildir
endif

if MU_COND_SUPPORT_DOTMAIL
  SUBDIRS += dotmail
endif

if MU_COND_SUPPORT_MBOXRB
  SUBDIRS += mboxrb
endif
