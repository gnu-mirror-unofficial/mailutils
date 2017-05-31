%top {
/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2005, 2007, 2009-2012, 2014-2017 Free Software
   Foundation, Inc.

   GNU Mailutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Mailutils is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
}

%{
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <mimeview.h>
#include <mimetypes-decl.h>  
#include <mailutils/io.h>
 
static struct mu_locus loc;
static int newline;

static mu_opool_t pool;
 
static unsigned 
digit_to_number (char c)
{
  return (unsigned) (c >= '0' && c <= '9' ? c-'0' :
                     c >= 'A' && c <= 'Z' ? c-'A'+10 :
                     c-'a'+10);
}

static void
advance_locus (void)
{
  if (newline)
    {
      loc.mu_line++;
      loc.mu_col = 1;
    }
  yylloc.beg = loc;
  loc.mu_col += yyleng;
  yylloc.end = loc;
  yylloc.end.mu_col--;
  
  newline = yytext[yyleng-1] == '\n';
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_SET_LOCUS, &loc);
}

#define YY_USER_ACTION advance_locus ();

%}

%option nounput
%option noinput
	  
%s RULE ARGS
X [0-9a-fA-F]
IDENT [a-zA-Z_\.][a-zA-Z0-9_\.-]*
WS [ \t]*
%%
     /* Comments */
^#.*\n               ;
     /* Tokens */
\\\n                 ;
\n+                  { loc.mu_line += yyleng - 1; return EOL; }
{WS}                 ;
<INITIAL,RULE>^[^ \t\n/]+"/"[^ \t\n]+ {
  mu_opool_append (pool, yytext, yyleng);
  mu_opool_append_char (pool, 0);
  yylval.string.ptr = mu_opool_finish (pool, &yylval.string.len);
  BEGIN (RULE);
  return TYPE;
}

<RULE>"priority"/"(" {
  return PRIORITY;
}
<RULE>{IDENT}/"(" {
  mu_opool_append (pool, yytext, yyleng);
  mu_opool_append_char (pool, 0);
  yylval.string.ptr = mu_opool_finish (pool, &yylval.string.len);
  return IDENT;
}
<RULE>{IDENT}              {
  mu_opool_append (pool, yytext, yyleng);
  mu_opool_append_char (pool, 0);
  yylval.string.ptr = mu_opool_finish (pool, &yylval.string.len);
  return STRING;
}
<RULE,ARGS>\"[^"\n]*\"        {
  mu_opool_append (pool, yytext+1, yyleng-2);
  mu_opool_append_char (pool, 0);
  yylval.string.ptr = mu_opool_finish (pool, &yylval.string.len);
  return STRING;
}
<RULE,ARGS>"<"({X}{X})+">" {
  int i;
  for (i = 0; i < yyleng; i += 2)
    {
      mu_opool_append_char (pool, digit_to_number (yytext[i])*16
                                  + digit_to_number (yytext[i+1]));
    }
  yylval.string.ptr = mu_opool_finish (pool, &yylval.string.len);
  return STRING;
}
<ARGS>[^ \t<\n),<"]+/[),<"] {
  mu_opool_append (pool, yytext, yyleng);
  mu_opool_append_char (pool, 0);
  yylval.string.ptr = mu_opool_finish (pool, &yylval.string.len);
  return STRING;
}
<RULE>[^ \t<\\\n)+,&]/[ \t\\\n)+,&] {
  mu_opool_append (pool, yytext, yyleng);
  mu_opool_append_char (pool, 0);
  yylval.string.ptr = mu_opool_finish (pool, &yylval.string.len);
  return STRING;
}
<ARGS>[^ \t<\\\n),]/[ \t\\\n] {
  mu_opool_append (pool, yytext, yyleng);
  mu_opool_append_char (pool, 0);
  yylval.string.ptr = mu_opool_finish (pool, &yylval.string.len);
  return STRING;
}
  /* Special cases: && and ||. Docs don't say anything about them, but
     I've found them in my mime.types file...         --Sergey */
"&&"  return '+';
"||"  return ',';
  /* Operators */
<RULE>"!"|"+"|","|"("|")"|"/"  return yytext[0];
<ARGS>","        return yytext[0];
<ARGS>")"        { BEGIN (RULE); return yytext[0]; }
<*>. {
  mu_error ("invalid character '%c', state %d", yytext[0], YYSTATE);
  return BOGUS;
}
%%
int
mimetypes_open (const char *name)
{
  struct stat st;
  int mode;

  yy_flex_debug = mu_debug_level_p (MU_DEBCAT_MIME, MU_DEBUG_TRACE4);

  if (stat (name, &st))
    {
      mu_error (_("cannot stat `%s': %s"), name, mu_strerror (errno));
      return -1;
    }
  
  if (S_ISDIR (st.st_mode))
    loc.mu_file = mu_make_file_name (name, "mime.types");
  else
    loc.mu_file = mu_strdup (name);
  loc.mu_line = 1;
  loc.mu_col = 1;
  newline = 0;

  yyin = fopen (loc.mu_file, "r");
  if (!yyin)
    {
      mu_error (_("cannot open `%s': %s"), loc.mu_file, mu_strerror (errno));
      free (loc.mu_file);
      return -1;
    }

  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_GET_MODE, &mode);
  mode |= MU_LOGMODE_LOCUS;
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_SET_MODE, &mode);
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_SET_LOCUS, &loc);

  mu_opool_create (&pool, MU_OPOOL_ENOMEMABRT);
  return 0;
}

void
mimetypes_close ()
{
  int mode;

  fclose (yyin);
  /* FIXME: Don't free (loc.mu_file), because it is referenced by
     mu_locus structures in the parse tree */

  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_GET_MODE, &mode);
  mode &= ~MU_LOGMODE_LOCUS;
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_SET_MODE, &mode);
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_SET_LOCUS, NULL);
}

int
yyerror (char *s)
{
  mu_error (s);
  return 0;
}

int
yywrap ()
{
  return 1;
}

struct mimetypes_string *     
mimetypes_string_dup (struct mimetypes_string *s)
{
  mu_opool_append (pool, s, sizeof *s);
  return mu_opool_finish (pool, NULL);
}

void *
mimetypes_malloc (size_t size)
{
  mu_opool_alloc (pool, size);
  return mu_opool_finish (pool, NULL);
}

void
lex_arglist (int enable)
{
  if (enable)
    BEGIN (ARGS);
  else
    BEGIN (RULE);
}

void
lex_concat (struct concat_segm *p, struct mimetypes_string *ret)
{
  for (; p; p = p->next)
    {
      mu_opool_appendz (pool, p->val);
    }
  mu_opool_append_char (pool, 0);
  ret->ptr = mu_opool_finish (pool, &ret->len);
}
