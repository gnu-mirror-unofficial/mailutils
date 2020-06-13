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

#include "readmsg.h"

static int
addset (int **set, int *n, unsigned val)
{
  int *tmp;
  tmp = realloc (*set, (*n + 1) * sizeof (**set));
  if (tmp == NULL)
    {
      if (*set)
        free (*set);
      *n = 0;
      *set = NULL;
      return ENOMEM;
    }
  *set = tmp;
  (*set)[*n] = val;
  (*n)++;
  return 0;
}

static int
is_number (const char *s)
{
  int result = 1;
  if (*s == '\0')
    result = 0;
  for (; *s; s++)
    {
      if (!mu_isdigit ((unsigned char)*s))
	{
	  result = 0;
	  break;
	}
    }
  return result;
}

static int
match_hdr (mu_header_t hdr, void *pattern)
{
  int rc;
  mu_iterator_t itr;
  int result = 1;
  
  rc = mu_header_get_iterator (hdr, &itr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_header_get_iterator", NULL, rc);
      return 1;
    }
  
  for (mu_iterator_first (itr); result && !mu_iterator_is_done (itr);
       mu_iterator_next (itr))
    {
      const char *value;
      char *tmp, *s;
      
      mu_iterator_current (itr, (void**)&value);
      tmp = strdup (value);
      if (!tmp)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "strdup", NULL, errno);
	  continue;
	}
      rc = mu_string_unfold (tmp, NULL);
      if (rc)
	{
	  free (tmp);
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_string_unfold", NULL, rc);
	  continue;
	}
      rc = mu_rfc2047_decode ("utf-8", tmp, &s);
      free (tmp);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_rfc2047_decode", NULL, rc);
	  continue;
	}	  
      tmp = s;
      if (pattern_match (pattern, tmp))
	result = 0;
      free (tmp);
    }
  mu_iterator_destroy (&itr);
  return result;
}

static int
matchmsgpart (mu_message_t msg, void *pattern)
{
  mu_header_t hdr;
  mu_stream_t str;
  char *buf = NULL;
  size_t bufsize = 0;
  size_t n;
  int result = 1;
  int rc;
  
  /* Get the headers. */
  rc = mu_message_get_header (msg, &hdr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_header", NULL, rc);
      return 1;
    }
  if (match_hdr (hdr, pattern) == 0)
    return 0;

  if (message_body_stream (msg, 0, "utf-8", &str))
    return 1;

  /* Look for a matching line. */
  while ((rc = mu_stream_getline (str, &buf, &bufsize, &n)) == 0 &&
	 n > 0)
    {
      buf[n-1] = 0;
      if (pattern_match (pattern, buf))
	{
	  result = 0;
	  break;
	}
    }

  free (buf);
  mu_stream_destroy (&str);
  return result;
}

static int
matchmsg (mu_message_t msg, void *pattern)
{
  int rc;
  mu_header_t hdr;
  mu_iterator_t itr;
  int result = 1;

  mu_message_get_header (msg, &hdr);
  if (match_hdr (hdr, pattern) == 0)
    return 0;
  rc = mu_message_get_iterator (msg, &itr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_iterator", NULL, rc);
      return 0;
    }
  for (mu_iterator_first (itr); !mu_iterator_is_done (itr);
       mu_iterator_next (itr))
    {
      mu_message_t partmsg;
      
      rc = mu_iterator_current (itr, (void**)&partmsg);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_iterator_current", NULL, rc);
	  continue;
	}
      if (matchmsgpart (partmsg, pattern) == 0)
	{
	  result = 0;
	  break;
	}
    }
  mu_iterator_destroy (&itr);
  return result;
}

/*
  According to ELM readmsg(1):

  1. A lone ``*'' means select all messages in the mailbox.

  2. A list of message numbers may be specified.  Values of ``0'' and
  ``$'' in the list both mean the last message in the mailbox.  For
  example:

  readmsg 1 3 0

  extracts three messages from the folder: the first, the third, and
  the last.

  3. Finally, the selection may be some text to match.  This will
  select a mail message which exactly matches the specified text. For
  example,

  readmsg staff meeting

  extracts the message which contains the words ``staff meeting.''
  Note that it will not match a message containing ``Staff Meeting'' -
  the matching is case sensitive.  Normally only the first message
  which matches the pattern will be printed.  The -a option discussed
  in a moment changes this.
*/

int
msglist (mu_mailbox_t mbox, int show_all, int argc, char **argv,
	 int **set, int *n)
{
  int i = 0;
  size_t total = 0;

  mu_mailbox_messages_count (mbox, &total);

  for (i = 0; i < argc; i++)
    {
      /* 1. A lone ``*'' means select all messages in the mailbox. */
      if (!strcmp (argv[i], "*"))
	{
	  size_t j;
	  /* all messages */
	  for (j = 1; j <= total; j++)
	    addset (set, n, j);
	  j = argc + 1;
	}
      /* 2. A list of message numbers may be specified.  Values of
	 ``0'' and ``$'' in the list both mean the last message in the
	 mailbox. */
      else if (!strcmp (argv[i], "$") || !strcmp (argv[i], "0"))
	{
	  addset (set, n, total);
	}
      /* 3. Finally, the selection may be some text to match.  This
	 will select a mail message which exactly matches the
	 specified text. */
      else if (!is_number (argv[i]))
	{
	  size_t j;
	  void *pat = pattern_init (argv[i]);
	  if (!pat)
	    continue;
	  for (j = 1; j <= total; j++)
	    {
	      mu_message_t msg = NULL;

	      mu_mailbox_get_message (mbox, j, &msg);
	      if (matchmsg (msg, pat) == 0)
		{
		  addset (set, n, j);
		  if (!show_all)
		    break;
		}
	    }
	  pattern_free (pat);
	}
      else if (mu_isdigit (argv[i][0]))
	{
	  /* single message */
	  addset (set, n, strtol (argv[i], NULL, 10));
	}
    }

  return 0;
}
