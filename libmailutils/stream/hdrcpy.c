/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2010-2021 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mailutils/stream.h>
#include <mailutils/cctype.h>

static int
excmp (const void *a, const void *b)
{
  return strcmp (*(const char **)a, *(const char **)b);
}

/* Given a NULL-terminated list of header names in NAMES, create a
   corresponding expect table for use in mu_stream_header_copy. The
   expect table is an array of strings of equal length. Each element
   contains a header name from the input array, converted to lowercase,
   with a ':' appended to it and stuffed with 0 bytes to the maximum
   length. The maximum length is selected as the length of the longest
   string from NAMES plus one.
   The resulting array is sorted in lexicographical order.
   On success, return the created table. Store the number of entries in
   PCOUNT and the maximum length in PMAX.
   On error (ENOMEM) return NULL.
 */
static char **
make_exclusion_list (char **names, size_t *pcount, size_t *pmax)
{
  size_t i, j;
  size_t count = 0;
  size_t max_len = 0;
  char **exlist;
  char *p;

  for (i = 0; names[i]; i++)
    {
      size_t len = strlen (names[i]) + 1;
      if (len > max_len)
	max_len = len;
    }
  count = i;

  exlist = calloc (count, sizeof (exlist[0]) + max_len + 1);
  if (!exlist)
    return NULL;
  p = (char*)(exlist + count);
  for (i = 0; names[i]; i++, p += max_len + 1)
    {
      exlist[i] = p;
      for (j = 0; names[i][j]; j++)
	p[j] = mu_tolower (names[i][j]);
      p[j++] = ':';
      memset (p + j, 0, max_len - j + 1);
    }
  qsort (exlist, count, sizeof (exlist[0]), excmp);

  *pcount = count;
  *pmax = max_len;
  return exlist;
}

/* Assuming SRC is a stream of RFC 822 headers, copy it to DST, omitting
   headers from EXCLUDE_NAMES. Stop copying at the empty line ("\n\n")
   or end of input, whichever occurs first. The terminating newline is
   read from SRC, but not written to DST. This allows the caller to append
   to DST any additional headers.
   Returns mailutils error code.
   FIXME: Bail out early with MU_ERR_PARSE if the input is not a well-
   formed header stream. This can be checked in save_state_init and
   save_state_expect by ensuring that the character read is in the
   MU_CTYPE_HEADR class.
*/
int
mu_stream_header_copy (mu_stream_t dst, mu_stream_t src, char **exclude_names)
{
  int rc;
  size_t la_max;
  char *lookahead;
  size_t la_idx = 0;
  enum
  {
    save_state_init,
    save_state_expect,
    save_state_skip,
    save_state_copy,
    save_state_stop
  } state = save_state_init;
  int i = 0;
  int j = 0;
  char **exclude;
  size_t excount;

  exclude = make_exclusion_list (exclude_names, &excount, &la_max);
  if (!exclude)
    return ENOMEM;
  lookahead = malloc (la_max);
  if (!lookahead)
    {
      free (exclude);
      return ENOMEM;
    }

  while (state != save_state_stop)
    {
      char c;
      size_t n;

      rc = mu_stream_read (src, &c, 1, &n);
      if (rc || n == 0)
	break;

      if (state == save_state_init || state == save_state_expect)
	{
	  if (la_idx == la_max)
	    state = save_state_copy;
	  else
	    {
	      lookahead[la_idx++] = c;
	      c = mu_tolower (c);
	    }
	}

      switch (state)
	{
	case save_state_init:
	  if (c == '\n')
	    {
	      /* End of headers. */
	      state = save_state_stop;
	      break;
	    }

	  j = 0;
	  state = save_state_copy;
	  for (i = 0; i < excount; i++)
	    {
	      if (exclude[i][j] == c)
		{
		  j++;
		  state = save_state_expect;
		  break;
		}
	    }
	  break;

	case save_state_expect:
	  if (exclude[i][j] != c)
	    {
	      while (++i < excount)
		{
		  if (memcmp (exclude[i-1], exclude[i], j))
		    {
		      state = save_state_copy;
		      break;
		    }
		  if (exclude[i][j] == c)
		    break;
		}
	      if (i == excount)
		state = save_state_copy;
	      if (state == save_state_copy)
		break;
	    }

	  if (c == ':')
	    {
	      la_idx = 0;
	      state = save_state_skip;
	    }
	  else
	    {
	      j++;
	      if (exclude[i][j] == 0)
		state = save_state_copy;
	    }
	  break;

	case save_state_copy:
	  if (la_idx > 0)
	    {
	      rc = mu_stream_write (dst, lookahead, la_idx, NULL);
	      if (rc)
		break;
	      la_idx = 0;
	    }
	  rc = mu_stream_write (dst, &c, 1, NULL);
	  if (c == '\n')
	    state = save_state_init;
	  break;

	case save_state_skip:
	  if (c == '\n')
	    state = save_state_init;
	  break;

	default:
	  abort (); /* Should not happen */
	}
    }

  if (rc == 0)
    {
      if (la_idx > 1)
	rc = mu_stream_write (dst, lookahead, la_idx - 1, NULL);
    }

  free (lookahead);
  free (exclude);

  return rc;
}
