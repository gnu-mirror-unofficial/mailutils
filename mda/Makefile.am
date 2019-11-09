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

SUBDIRS = lib 

if MU_COND_MDA
  SUBDIRS += mda
endif
if MU_COND_LMTPD
  SUBDIRS += lmtpd
endif
if MU_COND_PUTMAIL
  SUBDIRS += putmail
endif

EXTRA_DIST = tests/input.msg tests/mda.inc
