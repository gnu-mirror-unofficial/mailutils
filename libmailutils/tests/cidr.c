/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2011-2020 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <mailutils/mailutils.h>

static void
print_bytes (unsigned char *b, size_t l)
{
  for (; l; l--, b++)
    printf (" %02x", *b);
  printf ("\n");
}

static int ipv6_simplify;

struct mu_option options[] = {
  { "simplify", 's', NULL, MU_OPTION_DEFAULT,
    "simplify IPv6 addresses", mu_c_bool, &ipv6_simplify },
  MU_OPTION_END
};  

int
main (int argc, char **argv)
{
  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
                 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC, "cidr test tool",	
		 MU_CLI_OPTION_PROG_ARGS, "CIDR [CIDR...]",
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
                 MU_CLI_OPTION_RETURN_ARGV, &argv,
		 MU_CLI_OPTION_END);
	 
  if (argc == 0)
    {
      mu_error ("required argument missing; try %s --help for more info",
		mu_program_name);
      return 1;
    }

  while (argc--)
    {
      char *arg = *argv++;
      struct mu_cidr cidr;
      int rc;
      char *str;

      rc = mu_cidr_from_string (&cidr, arg);
      if (rc)
	{
	  mu_error ("%s: %s", arg, mu_strerror (rc));
	  continue;
	}

      printf ("%s:\n", arg);
      printf ("family = %d\n", cidr.family);
      printf ("len = %d\n", cidr.len);
      printf ("address =");
      print_bytes (cidr.address, cidr.len);
      printf ("netmask =");
      print_bytes (cidr.netmask, cidr.len);
      rc = mu_cidr_format (&cidr, ipv6_simplify ? MU_CIDR_FMT_SIMPLIFY : 0,
			   &str);
      if (rc)
	{
	  mu_error ("cannot convert to string: %s", mu_strerror (rc));
	  return 2;
	}

      printf ("string = %s\n", str);
      free (str);
    }
  return 0;
}
	
	  
