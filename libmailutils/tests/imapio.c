/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2011-2021 Free Software Foundation, Inc.

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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mailutils/imapio.h>
#include <mailutils/errno.h>
#include <mailutils/error.h>
#include <mailutils/stream.h>
#include <mailutils/stdstream.h>
#include <mailutils/cli.h>

int
main (int argc, char **argv)
{
  int i, rc;
  int transcript = 0;
  mu_imapio_t io;
  mu_stream_t str;
  int imapio_mode;
  int server_opt = 0;
  struct mu_option options[] = {
    { "transcript", 't', NULL, MU_OPTION_DEFAULT,
      "enable transcript", mu_c_incr, &transcript },
    { "server", 's', NULL, MU_OPTION_DEFAULT,
      "server mode", mu_c_incr, &server_opt },
    MU_OPTION_END
  };    
  char *capa[] = { "debug", NULL };
  mu_stream_t dstr;
  int t = 1;
  
  /* Create a separate diagnostic stream, independent from mu_strerr */
  MU_ASSERT (mu_stdio_stream_create (&dstr, MU_STDERR_FD, 0));
  mu_stream_ioctl (dstr, MU_IOCTL_FD, MU_IOCTL_FD_SET_BORROW, &t);

  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_CAPABILITIES, capa,
		 MU_CLI_OPTION_PROG_DOC, "imap parser test tool",
		 MU_CLI_OPTION_END);

  imapio_mode = server_opt ? MU_IMAPIO_SERVER : MU_IMAPIO_CLIENT;

  MU_ASSERT (mu_iostream_create (&str, mu_strin, mu_strout));
  
  MU_ASSERT (mu_imapio_create (&io, str, imapio_mode));

  if (transcript)
    mu_imapio_trace_enable (io);
  mu_stream_unref (str);

  while ((rc = mu_imapio_getline (io)) == 0)
    {
      size_t wc;
      char **wv;

      MU_ASSERT (mu_imapio_get_words (io, &wc, &wv));
      if (wc == 0)
	break;

      mu_stream_printf (dstr, "%lu\n", (unsigned long) wc);
      for (i = 0; i < wc; i++)
	{
	  mu_stream_printf (dstr, "%d: '%s'\n", i, wv[i]);
	}
    }

  if (rc)
    mu_error ("mu_imap_getline: %s", mu_strerror (rc));

  mu_imapio_free (io);
  return 0;
}
