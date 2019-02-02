/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2019 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library.  If not, see
   <http://www.gnu.org/licenses/>. */

#include <config.h>
#include <stdlib.h>
#include <mailutils/sys/mailcap.h>
#include <mailutils/wordsplit.h>
#include <mailutils/locus.h>
#include <mailutils/nls.h>
#include <mailutils/filter.h>
#include <mailutils/list.h>
#include <mailutils/stream.h>
#include <mailutils/opool.h>
#include <mailutils/cctype.h>

static int
create_entry (mu_mailcap_t mp, char *input, mu_mailcap_entry_t *pent)
{
  mu_mailcap_entry_t ent;
  struct mu_wordsplit ws;
  int rc;
  size_t i;

  ws.ws_delim = ";";
  ws.ws_escape[0] = ";\\";
  ws.ws_escape[1] = "";
  if (mu_wordsplit (input, &ws,
		    MU_WRDSF_DELIM
		    | MU_WRDSF_NOCMD
		    | MU_WRDSF_NOVAR
		    | MU_WRDSF_WS
		    | MU_WRDSF_ESCAPE))
    {
      if (mp->error.error)
	mp->error.error (mp->error.data, &mp->locus,
			 mu_wordsplit_strerror (&ws));
      return MU_ERR_FAILURE;
    }
  if (ws.ws_wordc < 2)
    {
      if (mp->error.error)
	mp->error.error (mp->error.data, &mp->locus, _("not enough fields"));
      mu_wordsplit_free (&ws);
      return MU_ERR_PARSE;
    }
  rc = mu_mailcap_entry_create (&ent, ws.ws_wordv[0], ws.ws_wordv[1]);
  if (rc == 0)
    {
      for (i = 2; i < ws.ws_wordc; i++)
	{
	  char *p = strchr (ws.ws_wordv[i], '=');
	  if (p)
	    {
	      *p++ = 0;
	      rc = mu_mailcap_entry_set_string (ent, ws.ws_wordv[i], p);
	    }
	  else
	    rc = mu_mailcap_entry_set_bool (ent, ws.ws_wordv[i]);
	  if (rc)
	    break;
	}
    }

  mu_wordsplit_free (&ws);

  if (rc == 0)
    {
      if (mp->flags & MU_MAILCAP_FLAG_LOCUS)
	{
	  ent->lrp = calloc (1, sizeof ent->lrp[0]);
	  if (ent->lrp)
	    {
	      mu_locus_range_init (ent->lrp);
	      rc = mu_locus_range_copy (ent->lrp, &mp->locus);
	    }
	  else
	    rc = errno;
	}
    }

  if (rc)
    {
      if (mp->error.error)
	mp->error.error (mp->error.data, &mp->locus, mu_strerror (rc));
      mu_mailcap_entry_destroy (&ent);
    }
  else
    *pent = ent;

  return rc;
}

int
mu_mailcap_parse (mu_mailcap_t mailcap, mu_stream_t input,
		  struct mu_locus_point const *pt)
{
  int rc;
  char *buffer = NULL;
  size_t bufsize = 0;
  size_t nread;
  mu_opool_t acc = NULL;
  mu_stream_t flt;
  const char *argv[] = { "inline-comment", "#", "-i", "#", NULL };
  int err = 0;

  rc = mu_filter_create_args (&flt, input,
			      argv[0],
			      MU_ARRAY_SIZE (argv) - 1, argv,
			      MU_FILTER_DECODE, MU_STREAM_READ);
  if (rc)
    return rc;

  mu_locus_range_init (&mailcap->locus);
  if (pt)
    {
      mu_locus_point_copy (&mailcap->locus.beg, pt);
      mu_locus_point_copy (&mailcap->locus.end, pt);
    }

  while ((rc = mu_stream_getline (flt, &buffer, &bufsize, &nread)) == 0)
    {
      mu_mailcap_entry_t entry;
      int cont = acc && mu_opool_size (acc) > 0;

      if (nread > 0)
	{
	  buffer[--nread] = 0;

	  if (buffer[0] == '#')
	    {
	      unsigned long n;
	      char *p;

	      errno = 0;
	      n = strtoul (buffer + 2, &p, 10);
	      if (errno == 0 && (*p == 0 || mu_isspace (*p)))
		mailcap->locus.beg.mu_line = mailcap->locus.end.mu_line = n;
	      continue;
	    }
	  else if (nread && buffer[nread-1] == '\\')
	    {
	      if (--nread > 0)
		{
		  if (!acc)
		    {
		      rc = mu_opool_create (&acc, MU_OPOOL_DEFAULT);
		      if (rc)
			break;
		    }
		  rc = mu_opool_append (acc, buffer, nread);
		  if (rc)
		    break;
		}

	      mailcap->locus.end.mu_line++;
	      continue;
	    }
	  else if (cont)
	    {
	      char *p;

	      rc = mu_opool_append (acc, buffer, nread);
	      if (rc)
		break;
	      rc = mu_opool_append_char (acc, 0);
	      if (rc)
		break;
	      p = mu_opool_finish (acc, NULL);
	      rc = create_entry (mailcap, p, &entry);
	      mu_opool_clear (acc);

	      mailcap->locus.beg.mu_line = ++mailcap->locus.end.mu_line;
	    }
	  else if (nread == 0)
	    {
	      mailcap->locus.beg.mu_line = ++mailcap->locus.end.mu_line;
	      continue;
	    }
	  else
	    {
	      rc = create_entry (mailcap, buffer, &entry);
	      mailcap->locus.beg.mu_line = ++mailcap->locus.end.mu_line;
	    }
	}
      else if (cont)
	{
	  /* Missing trailing newline */
	  char *p;

	  rc = mu_opool_append_char (acc, 0);
	  if (rc)
	    break;
	  p = mu_opool_finish (acc, NULL);
	  rc = create_entry (mailcap, p, &entry);
	  mu_opool_clear (acc);
	}
      else
	break;

      if (rc == 0)
	{
	  if (mailcap->selector.selector
	      && mailcap->selector.selector (entry, mailcap->selector.data) != 0)
	    {
	      mu_mailcap_entry_destroy (&entry);
	    }
	  else
	    {
	      mu_list_append (mailcap->elist, entry);
	    }
	}
      else if (rc == MU_ERR_PARSE)
	err = 1;
      else
	break;
    }
  mu_stream_destroy (&flt);
  mu_locus_range_deinit (&mailcap->locus);
  mu_opool_destroy (&acc);

  if (rc && err)
    rc = MU_ERR_PARSE;

  return rc;
}
