/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2020 Free Software Foundation, Inc.

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

%{
#include "mail.h"

#include <stdio.h>
#include <stdlib.h>

/* Defined in <limits.h> on some systems, but redefined in <regex.h>
   if we are using GNU's regex. So, undef it to avoid duplicate definition
   warnings. */

#ifdef RE_DUP_MAX
# undef RE_DUP_MAX
#endif
#include <regex.h>

struct header_data
{
  char *header;
  char *expr;
};

static msgset_t *msgset_select (int (*sel) (mu_message_t, void *),
				     void *closure, int rev,
				     unsigned int max_matches);
static int select_header (mu_message_t msg, void *closure);
static int select_body (mu_message_t msg, void *closure);
static int select_sender (mu_message_t msg, void *closure);
static int select_deleted (mu_message_t msg, void *closure);
static int check_set (msgset_t **pset);

int yyerror (const char *);
int yylex  (void);

static int msgset_flags = MSG_NODELETED;
static size_t message_count;
static msgset_t *result;
static mu_opool_t tokpool;

typedef int (*message_selector_t) (mu_message_t, void *);
static message_selector_t find_type_selector (int type);
%}

%union {
  char *string;
  int number;
  int type;
  msgset_t *mset;
}

%token <type> TYPE
%token <string> IDENT REGEXP HEADER BODY
%token <number> NUMBER
%type <mset> msgset msgspec msgexpr msg rangeset range partno number
%type <string> header

%%

input    : /* empty */
           {
	     result = msgset_make_1 (get_cursor ());
	   }
         | '.'
           {
	     result = msgset_make_1 (get_cursor ());
	   }
         | msgset
           {
	     result = $1;
	   }
         | '^'
           {
	     result = msgset_select (select_deleted, NULL, 0, 1);
	   }
         | '$'
           {
	     result = msgset_select (select_deleted, NULL, 1, 1);
	   }
         | '*'
           {
	     result = msgset_select (select_deleted, NULL, 0, total);
	   }
         | '-'
           {
	     result = msgset_select (select_deleted, NULL, 1, 1);
	   }
         | '+'
           {
	     result = msgset_select (select_deleted, NULL, 0, 1);
	   }
         ;

msgset   : msgexpr
         | msgset ',' msgexpr
           {
	     $$ = msgset_append ($1, $3);
	   }
         | msgset msgexpr
           {
	     $$ = msgset_append ($1, $2);
	   }
         ;

msgexpr  : msgspec
           {
	     $$ = $1;
	     if (check_set (&$$))
	       YYABORT;
	   }
         | '{' msgset '}'
           {
	     $$ = $2;
	   }
         | '!' msgexpr
           {
	     $$ = msgset_negate ($2);
	   }
         ;

msgspec  : msg
         | msg '.' rangeset 
           {
	     $$ = msgset_expand ($1, $3);
	     msgset_free ($1);
	     msgset_free ($3);
	   }
         | msg '[' rangeset ']'
           {
	     $$ = msgset_expand ($1, $3);
	     msgset_free ($1);
	     msgset_free ($3);
	   }
         | range
         ;

msg      : header REGEXP /* /.../ */
           {
	     struct header_data hd;
	     hd.header = $1;
	     hd.expr   = $2;
	     $$ = msgset_select (select_header, &hd, 0, 0);
	     if (!$$)
	       {
		 if ($1)
		   mu_error (_("No applicable messages from {%s:/%s}"), $1, $2);
		 else
		   mu_error (_("No applicable messages from {/%s}"), $2);
		 YYERROR;
	       }
	   }
         | BODY
           {
	     $$ = msgset_select (select_body, $1, 0, 0);
	     if (!$$)
	       {
		 mu_error (_("No applicable messages from {:/%s}"), $1);
		 YYERROR;
	       }
	   }
         | TYPE  /* :n, :d, etc */
           {
	     message_selector_t sel = find_type_selector ($1);
	     if (!sel)
	       {
		 yyerror (_("unknown message type"));
		 YYERROR;
	       }
	     $$ = msgset_select (sel, NULL, 0, 0);
	     if (!$$)
	       {
		 mu_error (_("No messages satisfy :%c"), $1);
		 YYERROR;
	       }
	   }
         | IDENT /* Sender name */
           {
	     $$ = msgset_select (select_sender, (void *)$1, 0, 0);
	     if (!$$)
	       {
		 mu_error (_("No applicable messages from {%s}"), $1);
		 YYERROR;
	       }
	   }
         ;

header   : /* empty */
           {
	     $$ = NULL;
	   }
         | HEADER
           {
	     $$ = $1;
	   }
         ;

rangeset : range
         | rangeset ',' range
           {
	     $$ = msgset_append ($1, $3);
	   }
         | rangeset range
           {
	     $$ = msgset_append ($1, $2);
	   }
         ;

range    : number
         | NUMBER '-' number
           {
	     if (msgset_length ($3) == 1)
	       {
		 $$ = msgset_range ($1, msgset_msgno ($3));
	       }
	     else
	       {
		 $$ = msgset_range ($1, msgset_msgno ($3) - 1);
		 if (!$$)
		   YYERROR;
		 msgset_append ($$, $3);
	       }
	   }
         | NUMBER '-' '*'
           {
	     $$ = msgset_range ($1, total);
	   }
         ;

number   : partno
           {
	   }
         | partno '.' rangeset 
           {
	     $$ = msgset_expand ($1, $3);
	     msgset_free ($1);
	     msgset_free ($3);
	   }
         | partno '[' rangeset ']'
           {
	     $$ = msgset_expand ($1, $3);
	     msgset_free ($1);
	     msgset_free ($3);
	   }
         ;

partno   : NUMBER
           {
	     $$ = msgset_make_1 ($1);
	   }
         | '(' rangeset ')'
           {
	     $$ = $2;
	   }
         ;
%%

static int xargc;
static char **xargv;
static int cur_ind;
static char *cur_p;

int
yyerror (const char *s)
{
  mu_stream_printf (mu_strerr, "%s: ", xargv[0]);
  mu_stream_printf (mu_strerr, "%s", s);
  if (!cur_p)
    mu_stream_printf (mu_strerr, _(" near end"));
  else if (*cur_p == 0)
    {
      int i =  (*cur_p == 0) ? cur_ind + 1 : cur_ind;
      if (i == xargc)
	mu_stream_printf (mu_strerr, _(" near end"));
      else
	mu_stream_printf (mu_strerr, _(" near %s"), xargv[i]);
    }
  else
    mu_stream_printf (mu_strerr, _(" near %s"), cur_p);
  mu_stream_printf (mu_strerr, "\n");
  return 0;
}

int
yylex (void)
{
  if (cur_ind == xargc)
    return 0;
  if (!cur_p)
    cur_p = xargv[cur_ind];
  if (*cur_p == 0)
    {
      cur_ind++;
      cur_p = NULL;
      return yylex ();
    }

  if (mu_isdigit (*cur_p))
    {
      yylval.number = strtoul (cur_p, &cur_p, 10);
      return NUMBER;
    }

  if (mu_isalpha (*cur_p))
    {
      char *p = cur_p;

      while (*cur_p && *cur_p != ',' && *cur_p != ':') 
	cur_p++;
      mu_opool_append (tokpool, p, cur_p - p);
      mu_opool_append_char (tokpool, 0);
      yylval.string = mu_opool_finish (tokpool, NULL);
      if (*cur_p == ':')
	{
	  ++cur_p;
	  return HEADER;
	}
      return IDENT;
    }

  if (*cur_p == '/')
    {
      char *p = ++cur_p;

      while (*cur_p && *cur_p != '/')
	cur_p++;

      mu_opool_append (tokpool, p, cur_p - p);
      mu_opool_append_char (tokpool, 0);
      yylval.string = mu_opool_finish (tokpool, NULL);

      if (*cur_p)
	cur_p++;

      return REGEXP;
    }

  if (*cur_p == ':')
    {
      cur_p++;
      if (*cur_p == '/')
	{
	  char *p = ++cur_p;
	  
	  while (*cur_p && *cur_p != '/')
	    cur_p++;

	  mu_opool_append (tokpool, p, cur_p - p);
	  mu_opool_append_char (tokpool, 0);
	  yylval.string = mu_opool_finish (tokpool, NULL);

	  if (*cur_p)
	    cur_p++;

	  return BODY;
	}
      if (*cur_p == 0)
	return 0;
      yylval.type = *cur_p++;
      return TYPE;
    }

  return *cur_p++;
}

int
msgset_parse (const int argc, char **argv, int flags, msgset_t **mset)
{
  int rc;
  xargc = argc;
  xargv = argv;
  msgset_flags = flags;
  cur_ind = 1;
  cur_p = NULL;
  result = NULL;

  mu_opool_create (&tokpool, MU_OPOOL_ENOMEMABRT);
  mu_mailbox_messages_count (mbox, &message_count);
  rc = yyparse ();
  if (rc == 0)
    {
      if (!result)
	{
	  util_noapp ();
	  rc = 1;
	}
      else
	{
	  if (result->crd[1] > message_count)
	    {
	      util_error_range (result->crd[1]);
	      msgset_free (result);
	      return 1;
	    }
	  *mset = result;
	}
    }
  mu_opool_destroy (&tokpool);
  return rc;
}

void
msgset_free (msgset_t *msg_set)
{
  msgset_t *next;

  while (msg_set)
    {
      next = msg_set->next;
      free (msg_set->crd);
      free (msg_set);
      msg_set = next;
    }
}

size_t
msgset_count (msgset_t *set)
{
  size_t count = 0;
  for (; set; set = set->next)
    count++;
  return count;
}

/* Create a message set consisting of a single msg_num and no subparts */
msgset_t *
msgset_make_1 (size_t number)
{
  msgset_t *mp;

  if (number == 0)
    return NULL;
  mp = mu_alloc (sizeof (*mp));
  mp->next = NULL;
  if (mu_coord_alloc (&mp->crd, 1))
    mu_alloc_die ();
  mp->crd[1] = number;
  return mp;
}

msgset_t *
msgset_dup (const msgset_t *set)
{
  msgset_t *mp;
  mp = mu_alloc (sizeof (*mp));
  mp->next = NULL;
  if (mu_coord_dup (set->crd, &mp->crd))
    mu_alloc_die ();
  return mp;
}

/* Append message set TWO to the end of message set ONE. Take care to
   eliminate duplicates. Preserve the ordering of both lists. Return
   the resulting set.

   The function is destructive: the set TWO is attached to ONE and
   eventually modified to avoid duplicates.
*/
msgset_t *
msgset_append (msgset_t *one, msgset_t *two)
{
  msgset_t *last;

  if (!one)
    return two;
  for (last = one; last->next; last = last->next)
    {
      msgset_remove (&two, msgset_msgno (last));
    }
  last->next = two;
  return one;
}

int
msgset_member (msgset_t *set, size_t n)
{
  for (; set; set = set->next)
    if (msgset_msgno (set) == n)
      return 1;
  return 0;
}

void
msgset_remove (msgset_t **pset, size_t n)
{
  msgset_t *cur = *pset, **pnext = pset;

  while (1)
    {
      if (cur == NULL)
	return;
      if (msgset_msgno (cur) == n)
	break;
      pnext = &cur->next;
      cur = cur->next;
    }
  *pnext = cur->next;
  cur->next = NULL;
  msgset_free (cur);
}

msgset_t *
msgset_negate (msgset_t *set)
{
  size_t i;
  msgset_t *first = NULL, *last = NULL;

  for (i = 1; i <= total; i++)
    {
      if (!msgset_member (set, i))
	{
	  msgset_t *mp = msgset_make_1 (i);
	  if (!first)
	    first = mp;
	  else
	    last->next = mp;
	  last = mp;
	}
    }
  return first;
}

msgset_t *
msgset_range (int low, int high)
{
  int i;
  msgset_t *mp, *first = NULL, *last = NULL;

  if (low == high)
    return msgset_make_1 (low);

  if (low >= high)
    {
      yyerror (_("range error"));
      return NULL;
    }

  for (i = 0; low <= high; i++, low++)
    {
      mp = msgset_make_1 (low);
      if (!first)
	first = mp;
      else
	last->next = mp;
      last = mp;
    }
  return first;
}

msgset_t *
msgset_expand (msgset_t *set, msgset_t *expand_by)
{
  msgset_t *i, *j;
  msgset_t *first = NULL, *last = NULL, *mp;

  for (i = set; i; i = i->next)
    for (j = expand_by; j; j = j->next)
      {
	mp = mu_alloc (sizeof *mp);
	mp->next = NULL;
	if (mu_coord_alloc (&mp->crd,
			    mu_coord_length (i->crd) + mu_coord_length (j->crd)))
	  mu_alloc_die ();
	memcpy (&mp->crd[1], &i->crd[1],
		mu_coord_length (i->crd) * sizeof i->crd[0]);
	memcpy (&mp->crd[1] + mu_coord_length (i->crd), &j->crd[1],
		mu_coord_length (j->crd) * sizeof j->crd[0]);

	if (!first)
	  first = mp;
	else
	  last->next = mp;
	last = mp;
      }
  return first;
}

msgset_t *
msgset_select (message_selector_t sel, void *closure, int rev,
	       unsigned int max_matches)
{
  size_t i, match_count = 0;
  msgset_t *first = NULL, *last = NULL, *mp;
  mu_message_t msg = NULL;

  if (max_matches == 0)
    max_matches = total;

  if (rev)
    {
      for (i = total; i > 0; i--)
	{
	  mu_mailbox_get_message (mbox, i, &msg);
	  if ((*sel) (msg, closure))
	    {
	      mp = msgset_make_1 (i);
	      if (!first)
		first = mp;
	      else
		last->next = mp;
	      last = mp;
	      if (++match_count == max_matches)
		break;
	    }
	}
    }
  else
    {
      for (i = 1; i <= total; i++)
	{
	  mu_mailbox_get_message (mbox, i, &msg);
	  if ((*sel) (msg, closure))
	    {
	      mp = msgset_make_1 (i);
	      if (!first)
		first = mp;
	      else
		last->next = mp;
	      last = mp;
	      if (++match_count == max_matches)
		break;
	    }
	}
    }
  return first;
}

int
select_header (mu_message_t msg, void *closure)
{
  struct header_data *hd = (struct header_data *)closure;
  mu_header_t hdr;
  char *contents;
  const char *header = hd->header ? hd->header : MU_HEADER_SUBJECT;

  mu_message_get_header (msg, &hdr);
  if (mu_header_aget_value (hdr, header, &contents) == 0)
    {
      if (mailvar_get (NULL, "regex", mailvar_type_boolean, 0) == 0)
	{
	  /* Match string against the extended regular expression(ignoring
	     case) in pattern, treating errors as no match.
	     Return 1 for match, 0 for no match.
	  */
          regex_t re;
          int status;
	  int flags = REG_EXTENDED;

	  if (mu_islower (header[0]))
	    flags |= REG_ICASE;
          if (regcomp (&re, hd->expr, flags) != 0)
	    {
	      free (contents);
	      return 0;
	    }
          status = regexec (&re, contents, 0, NULL, 0);
          free (contents);
	  regfree (&re);
          return status == 0;
	}
      else
	{
	  int rc;
	  mu_strupper (contents);
	  rc = strstr (contents, hd->expr) != NULL;
	  free (contents);
	  return rc;
	}
    }
  return 0;
}

int
select_body (mu_message_t msg, void *closure)
{
  char *expr = closure;
  int noregex = mailvar_get (NULL, "regex", mailvar_type_boolean, 0);
  regex_t re;
  int status = 0;
  mu_body_t body = NULL;
  mu_stream_t stream = NULL;
  int rc;
  
  if (noregex)
    mu_strupper (expr);
  else if (regcomp (&re, expr, REG_EXTENDED | REG_ICASE) != 0)
    return 0;

  mu_message_get_body (msg, &body);
  rc = mu_body_get_streamref (body, &stream);
  if (rc == 0)
    {
      char *buffer = NULL;
      size_t size = 0;
      size_t n = 0;
      
      while (status == 0
	     && (rc = mu_stream_getline (stream, &buffer, &size, &n)) == 0
	     && n > 0)
	{
	  if (noregex)
	    {
	      /* FIXME: charset */
	      mu_strupper (buffer);
	      status = strstr (buffer, expr) != NULL;
	    }
	  else
	    status = regexec (&re, buffer, 0, NULL, 0) == 0;
	}
      mu_stream_destroy (&stream);
      free (buffer);
      if (rc)
	mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_getline", NULL, rc);
    }
  else
    mu_diag_funcall (MU_DIAG_ERROR, "mu_body_get_streamref", NULL, rc);
  
  if (!noregex)
    regfree (&re);

  return status;
}

int
select_sender (mu_message_t msg, void *closure)
{
  char *needle = (char*) closure;
  char *sender = sender_string (msg);
  int status = strcmp (sender, needle) == 0;
  free (sender);
  return status;
}

static int
select_type_d (mu_message_t msg, void *unused MU_ARG_UNUSED)
{
  mu_attribute_t attr;

  if (mu_message_get_attribute (msg, &attr) == 0)
    return mu_attribute_is_deleted (attr);
  return 0;
}

static int
select_type_n (mu_message_t msg, void *unused MU_ARG_UNUSED)
{
  mu_attribute_t attr;

  if (mu_message_get_attribute (msg, &attr) == 0)
    return mu_attribute_is_recent (attr);
  return 0;
}

static int
select_type_o (mu_message_t msg, void *unused MU_ARG_UNUSED)
{
  mu_attribute_t attr;

  if (mu_message_get_attribute (msg, &attr) == 0)
    return mu_attribute_is_seen (attr);
  return 0;
}

static int
select_type_r (mu_message_t msg, void *unused MU_ARG_UNUSED)
{
  mu_attribute_t attr;

  if (mu_message_get_attribute (msg, &attr) == 0)
    return mu_attribute_is_read (attr);
  return 0;
}

static int
select_type_s (mu_message_t msg, void *unused MU_ARG_UNUSED)
{
  mu_attribute_t attr;

  if (mu_message_get_attribute (msg, &attr) == 0)
    return mu_attribute_is_userflag (attr, MAIL_ATTRIBUTE_SAVED);
  return 0;
}

static int
select_type_t (mu_message_t msg, void *unused MU_ARG_UNUSED)
{
  mu_attribute_t attr;

  if (mu_message_get_attribute (msg, &attr) == 0)
    return mu_attribute_is_userflag (attr, MAIL_ATTRIBUTE_TAGGED);
  return 0;
}

static int
select_type_T (mu_message_t msg, void *unused MU_ARG_UNUSED)
{
  mu_attribute_t attr;

  if (mu_message_get_attribute (msg, &attr) == 0)
    return !mu_attribute_is_userflag (attr, MAIL_ATTRIBUTE_TAGGED);
  return 0;
}

static int
select_type_u (mu_message_t msg, void *unused MU_ARG_UNUSED)
{
  mu_attribute_t attr;

  if (mu_message_get_attribute (msg, &attr) == 0)
    return !mu_attribute_is_read (attr);
  return 0;
}

struct type_selector
{
  int letter;
  message_selector_t func;
};

static struct type_selector type_selector[] = {
  { 'd', select_type_d },
  { 'n', select_type_n },
  { 'o', select_type_o },
  { 'r', select_type_r },
  { 's', select_type_s },
  { 't', select_type_t },
  { 'T', select_type_T },
  { 'u', select_type_u },
  { '/', NULL }, /* A pseudo-entry needed for msgtype_generator only */
  { 0 }
};

static message_selector_t
find_type_selector (int type)
{
  struct type_selector *p;
  for (p = type_selector; p->func; p++)
    {
      if (p->letter == type)
	return p->func;
    }
  return NULL;
}

#ifdef WITH_READLINE
char *
msgtype_generator (const char *text, int state)
{
  /* Allowed message types, plus '/'. The latter can folow a colon,
     meaning body lookup */
  static int i;
  char c;

  if (!state)
    {
      i = 0;
    }
  while ((c = type_selector[i].letter))
    {
      i++;
      if (!text[1] || text[1] == c)
	{
	  char *s = mu_alloc (3);
	  s[0] = ':';
	  s[1] = c;
	  s[2] = 0;
	  return s;
	}
    }
  return NULL;
}
#endif

int
select_deleted (mu_message_t msg, void *closure MU_ARG_UNUSED)
{
  mu_attribute_t attr= NULL;
  int rc;

  mu_message_get_attribute (msg, &attr);
  rc = mu_attribute_is_deleted (attr);
  return strcmp (xargv[0], "undelete") == 0 ? rc : !rc;
}

int
check_set (msgset_t **pset)
{
  int flags = msgset_flags;
  int rc = 0;

  if (!*pset)
    {
      util_noapp ();
      return 1;
    }
  if (msgset_count (*pset) == 1)
    flags ^= MSG_SILENT;
  if (flags & MSG_NODELETED)
    {
      msgset_t *p = *pset, *prev = NULL;
      msgset_t *delset = NULL;

      while (p)
	{
	  msgset_t *next = p->next;
	  if (util_isdeleted (msgset_msgno (p)))
	    {
	      if ((flags & MSG_SILENT) && (prev || next))
		{
		  /* Mark subset as deleted */
		  p->next = delset;
		  delset = p;
		  /* Remove it from the set */
		  if (prev)
		    prev->next = next;
		  else
		    *pset = next;
		}
	      else
		{
		  mu_error (_("%lu: Inappropriate message (has been deleted)"),
			    (unsigned long) msgset_msgno (p));
		  /* Delete entire set */
		  delset = *pset;
		  *pset = NULL;
		  rc = 1;
		  break;
		}
	    }
	  else
	    prev = p;
	  p = next;
	}

      if (delset)
	msgset_free (delset);

      if (!*pset)
	rc = 1;
    }

  return rc;
}


#if 0
void
msgset_print (msgset_t *mset)
{
  int i;
  mu_printf ("(");
  mu_printf ("%d .", mset->msg_part[0]);
  for (i = 1; i < mset->npart; i++)
    {
      mu_printf (" %d", mset->msg_part[i]);
    }
  mu_printf (")\n");
}

int
main(int argc, char **argv)
{
  msgset_t *mset = NULL;
  int rc = msgset_parse (argc, argv, &mset);

  for (; mset; mset = mset->next)
    msgset_print (mset);
  return 0;
}
#endif
