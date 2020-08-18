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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <mailutils/mailutils.h>

int
main (int argc, char **argv)
{
  char *names = NULL;
  int showunset = 0;
  struct mu_option options[] = {
    { "names", 0, "NAME[;NAME...]", MU_OPTION_DEFAULT,
      "show only selected categories", mu_c_string, &names },
    { "showunset", 0, NULL, MU_OPTION_DEFAULT,
      "show unset debug categories as well", mu_c_incr, &showunset },
    MU_OPTION_END
  };  
  
  mu_set_program_name (argv[0]);
  mu_stdstream_setup (MU_STDSTREAM_RESET_NONE);

  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_SINGLE_DASH,
                 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC, "Mailutils debug specification test tool",
		 MU_CLI_OPTION_PROG_ARGS, "SPEC",
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
                 MU_CLI_OPTION_RETURN_ARGV, &argv,
		 MU_CLI_OPTION_END);
  
  
  if (argc != 1)
    {
      mu_error ("exactly one argument expected; try %s -help for more info",
		mu_program_name);
      return 0;
    }

  mu_debug_parse_spec (argv[0]);
  
  mu_debug_format_spec (mu_strout, names, showunset);
  mu_printf ("\n");
  
  return 0;
}

    
