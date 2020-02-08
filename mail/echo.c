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

#include "mail.h"

#if !HAVE_DECL_ENVIRON
extern char **environ;
#endif

/*
 * ec[ho] string ...
 */

static int
echo_runcmd (char **ret, const char *str, size_t len, char **argv, void *closure)
{
  int rc;
  mu_stream_t ps;
  mu_stream_t outs;
  size_t i;
  int status = MU_WRDSE_OK;
  char buf[128];
  size_t n;

  *ret = NULL;
  for (i = 0; argv[i]; i++)
    ;
  rc = mu_prog_stream_create (&ps, argv[0], i, argv, 0, NULL, MU_STREAM_READ);
  if (rc)
    {
      mu_error (_("Can't run %s: %s"), argv[0], mu_strerror (rc));
      return MU_WRDSE_USERERR;
    }

  rc = mu_memory_stream_create (&outs, MU_STREAM_RDWR);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_memory_stream_create", NULL, rc);
      mu_stream_destroy (&ps);
      return MU_WRDSE_USERERR;
    }

  while ((rc = mu_stream_read (ps, buf, sizeof (buf), &n)) == 0 && n > 0)
    {
      int wn = mu_stream_write (outs, buf, n, NULL);
      if (wn)
	{
	  mu_error (_("error writing to temporary stream: %s"),
		    mu_strerror (wn));
	  status = MU_WRDSE_USERERR;
	  break;
	}
    }

  if (status == MU_WRDSE_OK && rc)
    {
      mu_error (_("error reading %s output: %s"), argv[0], mu_strerror (rc));
      status = MU_WRDSE_USERERR;
    }

  mu_stream_destroy (&ps);

  if (status == MU_WRDSE_OK)
    {
      mu_off_t size;
      char *p;

      mu_stream_size (outs, &size);
      p = malloc (size + 1);
      if (p)
	{
	  mu_stream_seek (outs, 0, MU_SEEK_SET, NULL);
	  rc = mu_stream_read (outs, p, size, NULL);
	  if (rc == 0)
	    {
	      p[size] = 0;
	      *ret = p;
	    }
	  else
	    {
	      free (p);
	      mu_error (_("error reading from temporary stream: %s"),
			mu_strerror (rc));
	      status = MU_WRDSE_USERERR;
	    }
	}
      else
	status = MU_WRDSE_NOSPACE;
    }

  mu_stream_destroy (&outs);

  return status;
}

static int
echo (char *s, int *nl)
{
  int rc;
  struct mu_wordsplit ws;
  int wsflags = MU_WRDSF_NOSPLIT | MU_WRDSF_QUOTE | MU_WRDSF_ENV;
  size_t len;

  ws.ws_env = (const char **) environ;
  ws.ws_command = echo_runcmd;

  rc = mu_wordsplit (s, &ws, wsflags);
  switch (rc)
    {
    case MU_WRDSE_OK:
      break;

    case MU_WRDSE_USERERR:
      /* error message already displayed */
      mu_wordsplit_free (&ws);
      return 1;

    default:
      mu_error ("%s", mu_wordsplit_strerror (&ws));
      mu_wordsplit_free (&ws);
      return 1;
    }

  len = strlen (ws.ws_wordv[0]);
  mu_stream_write (mu_strout, ws.ws_wordv[0], len, NULL);
  *nl = len > 0 && ws.ws_wordv[0][len-1] == '\n';
  mu_wordsplit_free (&ws);
  return 0;
}

int
mail_echo (int argc, char **argv)
{
  if (argc > 1)
    {
      int i;
      int nl = 0;
      for (i = 1; i < argc; i++)
	{
	  if (i > 1)
	    mu_printf (" ");
	  if (echo (argv[i], &nl))
	    break;
	}
      if (!nl)
	mu_printf ("\n");
    }
  return 0;
}
