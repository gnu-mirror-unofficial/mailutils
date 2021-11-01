%top {
/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2005-2021 Free Software Foundation, Inc.

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
#include <grammar.h>  
#include <mailutils/io.h>
 
static unsigned 
digit_to_number (char c)
{
  return (unsigned) (c >= '0' && c <= '9' ? c-'0' :
                     c >= 'A' && c <= 'Z' ? c-'A'+10 :
                     c-'a'+10);
}

static void finish_string (MIMETYPES_YYSTYPE *lvalp, MIMETYPES_YYLTYPE *llocp,
			   yyscan_t yyscanner);

#define PCTL (yyget_extra (yyscanner))
#define POOL PCTL->mth->pool
 
#define YY_USER_ACTION							\
  do									\
    {									\
      mu_linetrack_advance (PCTL->trk, yylloc, yytext, yyleng);		\
      mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,			\
		       MU_IOCTL_LOGSTREAM_SET_LOCUS_RANGE, yylloc);	\
    }									\
  while (0);

#define YYSTYPE         MIMETYPES_YYSTYPE
#define YYLTYPE         MIMETYPES_YYLTYPE

%}

%option prefix="mimetypes_yy"
%option reentrant noyywrap bison-bridge bison-locations
%option extra-type="struct parser_control *"

%x RULE ARGS ASTRING
X [0-9a-fA-F]
IDENT [a-zA-Z_\.][a-zA-Z0-9_\.-]*
WS [ \t][ \t]*
%%

<INITIAL>{
     /* Comments */
^#.*\n               ;
\n                   ;
^[^ \t\n/]+"/"[^ \t\n]+ {
  mu_opool_append (POOL, yytext, yyleng);
  mu_opool_append_char (POOL, 0);
  yylval->string.ptr = mu_opool_finish (POOL, &yylval->string.len);
  yylval->string.len--;
  BEGIN (RULE);
  return TYPE;
}

. {
  mu_error (_("type/subtype is missing")); 
  return BOGUS;
}
}

<RULE>{
\\\n                 ;
\n                   {
  BEGIN (INITIAL);
  return EOL;
}
{WS}                 ;

   /* Operators */
"!"|"+"|","|"("|")"|"/"  return yytext[0];
  /* Special cases: && and ||. Docs don't say anything about them, but
     I've found them in my mime.types file...         --Sergey */
"&&"  return '+';
"||"  return ',';

"priority"/"(" {
  return PRIORITY;
}

{IDENT}/"(" {
  mu_opool_append (POOL, yytext, yyleng);
  mu_opool_append_char (POOL, 0);
  yylval->string.ptr = mu_opool_finish (POOL, &yylval->string.len);
  BEGIN (ARGS);
  return IDENT;
} 

[a-zA-Z0-9_.-]+/[^(] {
  mu_opool_append (POOL, yytext, yyleng);
  yylval->string.ptr = mu_opool_finish (POOL, &yylval->string.len);
  return STRING;
}

. mu_error("unexpected character '%c'", yytext[0]);
}

<ARGS>{
"("|","    return yytext[0];
")"  {
  BEGIN (RULE);
  return yytext[0];
}
{WS} mu_error ("unexpected whitespace in argument list");
\n   {
  mu_error ("unexpected newline in argument list");
  return BOGUS;
}
. {
  mu_locus_point_copy (&PCTL->string_beg, &yylloc->beg);
  mu_linetrack_retreat (PCTL->trk, 1); 
  yyless (0);
  BEGIN (ASTRING);
}
}

<ASTRING>{
  /* Quoted string */
\"[^"\n]*\"        {
  mu_opool_append (POOL, yytext+1, yyleng-2);
}
"'"[^'\n]*"'" {
  mu_opool_append (POOL, yytext+1, yyleng-2);
}
  
  /* Hex string */
"<"({X}{X})+">" {
  int i;
  for (i = 1; i < yyleng - 2; i += 2)
    {
      mu_opool_append_char (POOL, digit_to_number (yytext[i])*16
                                  + digit_to_number (yytext[i+1]));
    }  
}

  /* Unquoted character sequence */
[^ \t\n,)<"']+/[^"'<] {
  mu_opool_append (POOL, yytext, yyleng);
}

[^ \t\n,)<"]+/< {
  mu_opool_append (POOL, yytext, yyleng);
}

[^ \t\n,)<"]+/["'] {
  mu_opool_append (POOL, yytext, yyleng);
}

\n   {
  mu_error ("unexpected newline in argument");
  mu_opool_clear (POOL);
  return BOGUS;
}

. {
  mu_linetrack_retreat (PCTL->trk, 1);
  yyless (0);
  BEGIN (ARGS);
  finish_string (yylval, yylloc, yyscanner);  
  return STRING;
}

<<EOF>> {
  mu_locus_range_deinit (yylloc);
  yypop_buffer_state (yyscanner);
  yyterminate ();
 }
}

%%
int
mimetypes_scanner_open (yyscan_t scanner, const char *name)
{
  struct stat st;
  char *filename;
  FILE *fp;
	
  yyset_debug (mu_debug_level_p (MU_DEBCAT_APP, MU_DEBUG_TRACE4), scanner);

  if (stat (name, &st))
    {
      mu_error (_("cannot stat `%s': %s"), name, mu_strerror (errno));
      return -1;
    }
  
  if (S_ISDIR (st.st_mode))
    filename = mu_make_file_name (name, "mime.types");
  else
    filename = mu_strdup (name);
  
  fp = fopen (filename, "r");
  if (!fp)
    {
      mu_error (_("cannot open `%s': %s"), filename, mu_strerror (errno));
      free (filename);
      return -1;
    }

  yypush_buffer_state (yy_create_buffer (fp, YY_BUF_SIZE, scanner), scanner);  
  free (filename);
  
  return 0;
}

/* Position input at the beginning of the next rule as a final part of error
   recovery */
void
lex_next_rule (MIMETYPES_YYLTYPE *llocp, yyscan_t yyscanner)
{
  /*
   * FIXME: whatever it is, it seems the only way to get unput and BEGIN
   * to work.  Flex does not provide reentrant interface for BEGIN.  As 
   * to unput, the yyunput could be used instead.  Unfortunately, it is
   * undocumented, so relying on it is no better than using yyguts_t.
   */
  struct yyguts_t * yyg = (struct yyguts_t*)yyscanner;
  mu_linetrack_t trk = yyget_extra (yyscanner)->trk;
  int c;
  int dbg = yyget_debug (yyscanner)
               || mu_debug_level_p (MU_DEBCAT_APP, MU_DEBUG_TRACE6);
  
  if (dbg)
    {
      YY_LOCATION_PRINT (stderr, *llocp);
      fprintf (stderr, ": started error recovery\n");
    }
  while ((c = input (yyscanner)) != EOF)
    {
      char ch = c;
      if (!mu_isspace (c) && mu_linetrack_at_bol (trk))
	{
	  unput (c);
	  break;
 	}
      mu_linetrack_advance (trk, llocp, &ch, 1);
    }
  if (dbg)
    {
      struct mu_locus_range lr = MU_LOCUS_RANGE_INITIALIZER;
      mu_linetrack_locus (trk, &lr.beg);
      YY_LOCATION_PRINT (stderr, lr);
      fprintf (stderr, ": finished error recovery\n");
      mu_locus_point_deinit (&lr.beg);
    }
  BEGIN (RULE);
  unput ('\n');
  mu_linetrack_retreat (trk, 1);
}

static void
finish_string (MIMETYPES_YYSTYPE *lvalp, MIMETYPES_YYLTYPE *llocp,
	       yyscan_t yyscanner)
{
  struct parser_control *pctl = yyget_extra (yyscanner);
  mu_opool_t pool = pctl->mth->pool;
  
  mu_opool_append_char (pool, 0);
  lvalp->string.ptr = mu_opool_finish (pool, &lvalp->string.len);
  lvalp->string.len--;
  
  mu_locus_point_copy (&llocp->end, &llocp->beg);
  llocp->end.mu_col--;
  mu_locus_point_copy (&llocp->beg, &pctl->string_beg);
  mu_locus_point_deinit (&pctl->string_beg);
  
  if (mu_debug_level_p (MU_DEBCAT_APP, MU_DEBUG_TRACE5))
    {
      size_t i;
      mu_debug_log_begin ("string %zu: ", lvalp->string.len);
      for (i = 0; i < lvalp->string.len; i++)
	if (mu_isprint (lvalp->string.ptr[i]))
	  mu_debug_log_cont ("%c", lvalp->string.ptr[i]);
        else
	  mu_debug_log_cont ("\\%03o", lvalp->string.ptr[i]);
      mu_debug_log_nl ();
    }
#if 0
  YY_LOCATION_PRINT (stderr, llocp);
  fprintf (stderr, ": %s\n", lvalp->string.ptr);
#endif
}  
