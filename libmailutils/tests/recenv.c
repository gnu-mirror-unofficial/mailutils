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

int
main (int argc, char **argv)
{
  mu_stream_t instr;
  mu_message_t msg;
  mu_envelope_t env;
  char const *sender;

  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_PROG_DOC, "Reads first message from FILE, "
		 "restores its envelope and prints the sender address.",
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
  MU_ASSERT (mu_message_reconstruct_envelope (msg, &env));
  MU_ASSERT (mu_envelope_sget_sender (env, &sender));
  mu_printf ("%s\n", sender);
  return 0;
}
