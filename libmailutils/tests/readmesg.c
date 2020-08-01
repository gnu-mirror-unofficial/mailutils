/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2020 Free Software Foundation, Inc.

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
#include <mailutils/mailutils.h>

static void
print_envelope (mu_message_t msg)
{
  mu_envelope_t env;
  char const *sender, *date;

  MU_ASSERT (mu_message_get_envelope (msg, &env));
  MU_ASSERT (mu_envelope_sget_sender (env, &sender));
  mu_printf ("Sender: %s\n", sender);  
  MU_ASSERT (mu_envelope_sget_date (env, &date));
  mu_printf ("Date: %s\n", date);
}

static void
print_stats (mu_message_t msg)
{
  size_t n;
      
  MU_ASSERT (mu_message_size (msg, &n));
  mu_printf ("Size: %lu\n", (unsigned long) n);
  MU_ASSERT (mu_message_lines (msg, &n));
  mu_printf ("Lines: %lu\n", (unsigned long) n);
}

static void
print_header (mu_message_t msg)
{
  mu_header_t hdr;
  size_t n;
  
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_get_field_count (hdr, &n));
  mu_printf ("Headers: %lu\n", (unsigned long) n);
  MU_ASSERT (mu_header_size (hdr, &n));
  mu_printf ("Header size: %lu\n", (unsigned long) n);
  MU_ASSERT (mu_header_lines (hdr, &n));
  mu_printf ("Header lines: %lu\n", (unsigned long) n);
}
  
static void
print_body (mu_message_t msg)
{
  mu_body_t body;
  mu_stream_t str;
  size_t n;
  
  MU_ASSERT (mu_message_get_body (msg, &body));

  MU_ASSERT (mu_body_size (body, &n));
  mu_printf ("Body size: %lu\n", (unsigned long) n);
  MU_ASSERT (mu_body_lines (body, &n));
  mu_printf ("Body lines: %lu\n", (unsigned long) n);
  
  MU_ASSERT (mu_body_get_streamref (body, &str));
  MU_ASSERT (mu_stream_copy (mu_strout, str, 0, NULL));
  mu_stream_destroy (&str);
}

int
main (int argc, char **argv)
{
  mu_stream_t instr;
  mu_message_t msg;

  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_PROG_DOC, "Reads first message from FILE and "
		 "prints its summary (envelope, stats, header and body).",
		 MU_CLI_OPTION_PROG_ARGS, "FILE",
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
		 MU_CLI_OPTION_RETURN_ARGV, &argv,
		 MU_CLI_OPTION_END);

  if (argc != 1)
    {
      mu_error ("required argument missing");
      return 2;
    }
  
  mu_set_user_email_domain ("localhost");
  MU_ASSERT (mu_file_stream_create (&instr, argv[0], MU_STREAM_READ));  
  MU_ASSERT (mu_stream_to_message (instr, &msg));

  print_envelope (msg);
  print_stats (msg);
  print_header (msg);
  print_body (msg);
  
  return 0;
}
