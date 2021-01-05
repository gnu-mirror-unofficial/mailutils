/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2013-2021 Free Software Foundation, Inc.

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

#include <assert.h>
#include <mailutils/mailutils.h>

char *text = "From: root\n\
\n\
This is a test message.\n\
oo\n\
";

static void
cli_a (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  mu_header_t *hdr = opt->opt_ptr;
  char *p;
  
  p = strchr (arg, ':');
  if (!p)
    {
      mu_parseopt_error (po, "%s: invalid append header format", arg);
      exit (po->po_exit_error);
    }
    
  *p++ = 0;
  while (*p && mu_isspace (*p))
    p++;
  MU_ASSERT (mu_header_set_value (*hdr, arg, p, 1));
}

static void
cli_t (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  mu_stream_t *stream = opt->opt_ptr;
  mu_wordsplit_t ws;

  if (mu_wordsplit (arg, &ws,
		    MU_WRDSF_NOSPLIT | MU_WRDSF_DEFFLAGS))
    {
      mu_parseopt_error (po, "mu_wordsplit: %s", mu_wordsplit_strerror (&ws));
      exit (po->po_exit_error);
    }
  else
    MU_ASSERT (mu_stream_write (*stream, ws.ws_wordv[0],
				strlen (ws.ws_wordv[0]), NULL));
  mu_wordsplit_free (&ws);
}

static void
cli_l (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  mu_stream_t *stream = opt->opt_ptr;
  mu_off_t off;
  int whence = MU_SEEK_SET;
  char *p;
  
  errno = 0;
  off = strtol (arg, &p, 10);
  if (errno || *p)
    {
      mu_parseopt_error (po, "%s: invalid offset", arg);
      exit (po->po_exit_error);
    }

  if (off < 0)
    whence = MU_SEEK_END;
  MU_ASSERT (mu_stream_seek (*stream, off, whence, NULL));
}

int
main (int argc, char **argv)
{
  mu_message_t msg;
  mu_stream_t stream = NULL;
  mu_header_t hdr;
  mu_body_t body;

  struct mu_option options[] = {
    { "add-header", 'a', "HDR:VAL", MU_OPTION_DEFAULT,
      "add header to the message", mu_c_string, &hdr, cli_a },
    { "seek", 'l', "OFF", MU_OPTION_DEFAULT,
      "seek to the given position in message stream", mu_c_string, &stream,
      cli_l },
    { "text", 't', "TEXT", MU_OPTION_DEFAULT,
      "write given text to the message stream in current position",
      mu_c_string, &stream, cli_t },
    MU_OPTION_END
  };
    
  mu_set_program_name (argv[0]);

  mu_static_memory_stream_create (&stream, text, strlen (text));
  MU_ASSERT (mu_stream_to_message (stream, &msg));
  mu_stream_unref (stream);
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_message_get_body (msg, &body));
  MU_ASSERT (mu_body_get_streamref (body, &stream));
  MU_ASSERT (mu_stream_seek (stream, 0, MU_SEEK_END, NULL));

  //parse arguments
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC, "test message modifications",
		 MU_CLI_OPTION_END);

  mu_stream_unref (stream);

  MU_ASSERT (mu_message_get_streamref (msg, &stream));
  MU_ASSERT (mu_stream_copy (mu_strout, stream, 0, NULL));
  mu_stream_unref (stream);

  return 0;
}

  
