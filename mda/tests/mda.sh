# This file is part of GNU Mailutils.
# Copyright (C) 2007-2020 Free Software Foundation, Inc.
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

INPUT_MSG=$abs_top_srcdir/mda/tests/input.msg
dumpmail() {
    case $MU_DEFAULT_SCHEME in
	mbox)
	    sed -e '/^From /d'\
		-e /^X-IMAPbase:/d\
                -e /^X-UID:/d $1
	    ;;
	dotmail)
	    sed -e '/^\.$/d'\
		-e /^X-IMAPbase:/d\
                -e /^X-UID:/d $1
	    ;;
	mh)
	    sed -e /^X-IMAPbase:/d\
                -e /^X-UID:/d\
                -e /^X-Envelope-Sender:/d\
                -e /^X-Envelope-Date:/d $1/1
	    ;;
	maildir)
	    f=$(find $1/new -type f | head -n 1)
	    if test -n $f; then
		sed -e /^X-IMAPbase:/d\
                    -e /^X-UID:/d\
                    -e /^X-Envelope-Sender:/d\
                    -e /^X-Envelope-Date:/d $f
	    fi
	    ;;
	*)  # Should not happen
	    echo >&2 "Default mailbox format is uknown"
    esac
}    
