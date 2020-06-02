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
charset_setup (mu_stream_t *pstr, mu_content_type_t ct, char const *charset)
{
  struct mu_mime_param *param;
  if (mu_assoc_lookup (ct->param, "charset", &param) == 0
      && mu_c_strcasecmp (param->value, charset))
    {
      mu_stream_t input = *pstr;
      char const *argv[] = { "iconv", NULL, NULL, NULL };
      mu_stream_t flt;
      int rc;
      
      argv[1] = param->value;
      argv[2] = charset;
      rc = mu_filter_chain_create (&flt, input,
				   MU_FILTER_ENCODE,
				   MU_STREAM_READ,
				   MU_ARRAY_SIZE (argv) - 1,
				   (char**) argv);
      if (rc)
	{
	  mu_error (_("can't convert from charset %s to %s: %s"),
		    param->value, charset, mu_strerror (rc));
	  return rc;
	}
      mu_stream_unref (input);
      *pstr = flt;
    }
  return 0;
}

static int
is_text_part (mu_content_type_t ct)
{
  if (mu_c_strcasecmp (ct->type, "text") == 0)
    return 1;
  if (mu_c_strcasecmp (ct->type, "application") == 0)
    {
      static char *textapp[] = {
	"x-cshell",
	"x-perl",
	"x-shell",
	"x-csource",
	NULL
      };
      int i;
      for (i = 0; textapp[i]; i++)
	if (mu_c_strcasecmp (ct->subtype, textapp[i]) == 0)
	  return 1;
    }
  return 0;
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

int
message_body_stream (mu_message_t msg, int unix_header, char const *charset,
		     mu_stream_t *pstr)
{
  int rc;
  mu_header_t hdr;
  mu_body_t body;
  mu_stream_t d_stream;
  mu_stream_t stream = NULL;
  char *encoding = NULL;
  char *buf;
  mu_content_type_t ct;
  
  /* Get the headers. */
  rc = mu_message_get_header (msg, &hdr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_header", NULL, rc);
      return rc;
    }

  /* Read and parse the Content-Type header. */
  rc = mu_header_aget_value_unfold (hdr, MU_HEADER_CONTENT_TYPE, &buf);
  if (rc == MU_ERR_NOENT)
    {
      buf = strdup ("text/plain");
      if (!buf)
	return errno;
    }
  else if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_header_aget_value_unfold", NULL, rc);
      return rc;
    }

  rc = mu_content_type_parse (buf, NULL, &ct);
  free (buf);
  buf = NULL;
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_content_type_parse", NULL, rc);
      return rc;
    }

  if (is_text_part (ct))
    /* Process only textual parts */
    {
      /* Get the body stream. */
      rc = mu_message_get_body (msg, &body);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_body", NULL, rc);
	  goto err;
	}
  
      rc = mu_body_get_streamref (body, &stream);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_body_get_streamref", NULL, rc);
	  goto err;
	}

      /* Filter it through the appropriate decoder. */
      rc = mu_header_aget_value_unfold (hdr,
					MU_HEADER_CONTENT_TRANSFER_ENCODING,
					&encoding);
      if (rc == 0)
	mu_rtrim_class (encoding, MU_CTYPE_SPACE);
      else if (rc == MU_ERR_NOENT)
	encoding = NULL;
      else if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_header_aget_value_unfold",
			   NULL, rc);
	  goto err;
	}

      if (encoding == NULL || *encoding == '\0')
	/* No need to filter */;
      else if ((rc = mu_filter_create (&d_stream, stream, encoding,
				       MU_FILTER_DECODE, MU_STREAM_READ)) == 0)
	{
	  mu_stream_unref (stream);
	  stream = d_stream;
	}
      else
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_filter_create", encoding, rc);
	  /* FIXME: continue anyway? */
	}

      /* Convert the content to the requested charset. */
      rc = charset_setup (&stream, ct, charset);
      if (rc)
	goto err;

      if (unix_header)
	{
	  rc = mu_filter_create (&d_stream, stream, "FROM",
				 MU_FILTER_ENCODE, MU_STREAM_READ);
	  if (rc == 0)
	    {
	      mu_stream_unref (stream);
	      stream = d_stream;
	    }
	  else
	    {
	      mu_diag_funcall (MU_DIAG_ERROR, "mu_filter_create", "FROM", rc);
	      /* continue anyway */
	    }
	}
    }
  else
    rc = MU_ERR_USER0;
 err:
  free (buf);
  free (encoding);
  mu_content_type_destroy (&ct);
  if (rc == 0)
    *pstr = stream;
  else
    mu_stream_destroy (&stream);
  return rc;
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
