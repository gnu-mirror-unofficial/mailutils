/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2012, 2014-2018 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <mailutils/errno.h>
#include <mailutils/filter.h>

/* CRLF filter.

   In decode mode, translates each \r\n to \n. Takes no arguments.

   In encode mode, translates each \n to \r\n. If created with the
   "-n" option, leaves each \r\n input sequence untranslated, thereby
   "normalizing" the output (hence the option name).
 */

enum crlf_state
{
  state_init,
  state_cr
};

struct crlf_encoder_state
{
  enum crlf_state cur;
  enum crlf_state last;
};

/* Move min(isize,osize) bytes from iptr to optr, replacing each \n
   with \r\n.  If state->last is state_cr, any input \r\n sequences
   remain untouched, otherwise \r is treated as a regular character. */
static enum mu_filter_result
_crlf_encoder (void *xd,
	       enum mu_filter_command cmd,
	       struct mu_filter_io *iobuf)
{
  size_t i, j;
  const unsigned char *iptr;
  size_t isize;
  char *optr;
  size_t osize;
  struct crlf_encoder_state *state = xd;
  
  switch (cmd)
    {
    case mu_filter_init:
      state->cur = state_init;
    case mu_filter_done:
      return mu_filter_ok;
    default:
      break;
    }
  
  iptr = (const unsigned char *) iobuf->input;
  isize = iobuf->isize;
  optr = iobuf->output;
  osize = iobuf->osize;

  for (i = j = 0; i < isize && j < osize; i++)
    {
      unsigned char c = *iptr++;
      if (c == '\n')
	{
	  if (state->cur == state_cr)
	    {
	      state->cur = state_init;
	      optr[j++] = c;
	    }
	  else if (j + 1 == osize)
	    {
	      if (i == 0)
		{
		  iobuf->osize = 2;
		  return mu_filter_moreoutput;
		}
	      break;
	    }
	  else
	    {
	      optr[j++] = '\r';
	      optr[j++] = '\n';
	    }
	}
      else if (c == '\r' && state->last == state_cr)
	{
	  state->cur = state_cr;
	  optr[j++] = c;
	}
      else
	{
	  state->cur = state_init;
	  optr[j++] = c;
	}
    }
  iobuf->isize = i;
  iobuf->osize = j;
  return mu_filter_ok;
}

/* Move min(isize,osize) bytes from iptr to optr, replacing each \r\n
   with \n. */
static enum mu_filter_result
_crlf_decoder (void *xd MU_ARG_UNUSED,
	       enum mu_filter_command cmd,
	       struct mu_filter_io *iobuf)
{
  size_t i, j;
  const unsigned char *iptr;
  size_t isize;
  char *optr;
  size_t osize;

  switch (cmd)
    {
    case mu_filter_init:
    case mu_filter_done:
      return mu_filter_ok;
    default:
      break;
    }
  
  iptr = (const unsigned char *) iobuf->input;
  isize = iobuf->isize;
  optr = iobuf->output;
  osize = iobuf->osize;

  for (i = j = 0; i < isize && j < osize; i++)
    {
      unsigned char c = *iptr++;
      if (c == '\r')
	{
	  if (i + 1 == isize)
	    break;
	  if (*iptr == '\n')
	    continue;
	}
      optr[j++] = c;
    }
  iobuf->isize = i;
  iobuf->osize = j;
  return mu_filter_ok;
}

static int
alloc_state (void **pret, int mode, int argc, const char **argv)
{
  struct crlf_encoder_state *st;
  
  switch (mode)
    {
    case MU_FILTER_ENCODE:
      st = malloc (sizeof (*st));
      if (!st)
	return ENOMEM;
      st->cur = state_init;
      if (argc == 2 && strcmp (argv[1], "-n") == 0)
	st->last = state_cr;
      else
	st->last = state_init;
      *pret = st;
      break;

    case MU_FILTER_DECODE:
      *pret = NULL;
    }
  return 0;
}
  

static struct _mu_filter_record _crlf_filter = {
  "CRLF",
  alloc_state,
  _crlf_encoder,
  _crlf_decoder
};

mu_filter_record_t mu_crlf_filter = &_crlf_filter;


/* For compatibility with previous versions */
static struct _mu_filter_record _rfc822_filter = {
  "RFC822",
  alloc_state,
  _crlf_encoder,
  _crlf_decoder
};

mu_filter_record_t mu_rfc822_filter = &_rfc822_filter;


