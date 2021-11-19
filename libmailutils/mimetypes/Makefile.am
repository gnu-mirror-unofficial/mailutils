noinst_LTLIBRARIES = libmimetypes.la
libmimetypes_la_SOURCES = \
 grammar.c \
 lexer.c \
 grammar.h\
 eval.c

AM_CPPFLAGS = \
  $(MU_LIB_COMMON_INCLUDES) -I/libmailutils

AM_YFLAGS=-vtd
AM_LFLAGS=-d

grammar.c grammar.h: grammar.y
lexer.c: lexer.l

BUILT_SOURCES = \
  grammar.c grammar.h \
  lexer.c

EXTRA_DIST = grammar.y lexer.l

.l.c:
	$(FLEX) -o $@ $(AM_LFLAGS) $<
.y.c:
	$(BISON) -o $@ $(AM_YFLAGS) $<
