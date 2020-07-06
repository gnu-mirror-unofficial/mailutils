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

include $(top_srcdir)/testsuite/testsuite.am
AUTOTEST_INCLUDES += -I $(top_srcdir)/libmailutils/wordsplit

## -------------------------- ##
## Non-installable programs
## -------------------------- ##

AM_CPPFLAGS = $(MU_LIB_COMMON_INCLUDES)

noinst_LTLIBRARIES = libmu_tesh.la
libmu_tesh_la_SOURCES = tesh.c tesh.h

noinst_PROGRAMS = \
 addr\
 cidr\
 conttype\
 ctm\
 debugspec\
 decode2047\
 dump\
 encode2047\
 exp\
 fltst\
 fsaf\
 fsaftomod\
 fsfolder\
 globtest\
 hdrcpy\
 imapio\
 listop\
 linetrack\
 logstr\
 mailcap\
 mcf\
 mimehdr\
 mimeitr\
 modtofsaf\
 msgset\
 modmesg\
 parseopt\
 prop\
 scantime\
 strftime\
 strin\
 strout\
 strtoc\
 t0-stream\
 t1-stream\
 t-streamshift\
 tempfile\
 tcli\
 tocrlf\
 url-comp\
 url-parse\
 vexp\
 wicket\
 wordwrap\
 wsp\
 xscript

fsfolder_LDADD = libmu_tesh.la $(LDADD)

linetrack_LDADD = libmu_tesh.la $(LDADD)

listop_LDADD = libmu_tesh.la $(LDADD)

LDADD = $(MU_LIB_MAILUTILS)

EXTRA_DIST += Encode Decode Wicketfile

TESTSUITE_AT += \
 address.at\
 base64d.at\
 base64e.at\
 debugspec.at\
 decode2047.at\
 dot.at\
 crlf.at\
 crlfdot.at\
 ctm.at\
 encode2047.at\
 exp.at\
 fromflt.at\
 fsaf.at\
 fsaftomod.at\
 fsfolder00.at\
 fsfolder01.at\
 fsfolder02.at\
 fsfolder03.at\
 fsfolder04.at\
 hdrcpy.at\
 hdrflt.at\
 htmlent.at\
 globtest.at\
 imapio.at\
 inline-comment.at\
 linecon.at\
 list.at\
 linetrack.at\
 logstr.at\
 mailcap.at\
 mimehdr.at\
 modmesg00.at\
 modmesg01.at\
 modmesg02.at\
 modmesg03.at\
 modtofsaf.at\
 msgset.at\
 parseopt00.at\
 parseopt01.at\
 parseopt02.at\
 parseopt03.at\
 parseopt04.at\
 parseopt05.at\
 parseopt06.at\
 parseopt07.at\
 parseopt08.at\
 parseopt09.at\
 parseopt10.at\
 parseopt11.at\
 parseopt12.at\
 parseopt13.at\
 parseopt14.at\
 parseopt15.at\
 parseopt16.at\
 parseopt17.at\
 parseopt18.at\
 parseopt19.at\
 parseopt20.at\
 parseopt21.at\
 parseopt22.at\
 parseopt23.at\
 parseopt24.at\
 parseopt25.at\
 parseopt26.at\
 parseopt27.at\
 parseopt28.at\
 parseopt_help00.at\
 parseopt_help01.at\
 parseopt_help02.at\
 parseopt_help03.at\
 parseopt_help04.at\
 parseopt_help05.at\
 parseopt_help06.at\
 parseopt_help07.at\
 parseopt_help08.at\
 parseopt_help09.at\
 parseopt_help10.at\
 parseopt_help11.at\
 parseopt_help12.at\
 prop.at\
 scantime.at\
 strftime.at\
 streamshift.at\
 strerr.at\
 strin.at\
 strout.at\
 strtoc.at\
 t0-stream.at\
 t1-stream.at\
 url.at\
 url-comp.at\
 xml.at\
 wicket.at\
 wordwrap00.at\
 wordwrap01.at\
 wordwrap02.at\
 wordwrap03.at\
 xscript.at

# ###########################
# Wordsplit testsuite
# ###########################

# The wordsplit-version.h header is to supply the version string to
# wsp and to force it to use the mailutils version of the wordsplit.h
# header.
EXTRA_DIST += wordsplit-version.h
$(srcdir)/wordsplit-version.h: $(top_srcdir)/configure.ac
	$(AM_V_GEN){\
	  if test -e $(top_srcdir)/libmailutils/wordsplit/.git; then \
	      wsversion=$$(cd $(top_srcdir)/libmailutils/wordsplit; git describe); \
	  else \
              wsversion="unknown"; \
	  fi;\
	  echo "#define WORDSPLIT_VERSION \"$wsversion\"";\
          echo '#include <mailutils/wordsplit.h>'; } \
         > $(srcdir)/wordsplit-version.h

noinst_PROGRAMS += wsp
wsp_SOURCES = 
nodist_wsp_SOURCES = wsp.c
wsp.o: $(srcdir)/wordsplit-version.h
VPATH = $(srcdir):$(top_srcdir)/libmailutils/wordsplit

