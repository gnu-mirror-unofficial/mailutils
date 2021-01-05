/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2021 Free Software Foundation, Inc.

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

#include <config.h>
#include <mailutils/mailutils.h>

/* globtest PATTERN [WORD...]
 */   
int
main (int argc, char **argv)
{
  char *pattern = NULL;
  int flags = 0;
  int rc;
  int i;
  int sub_opt = 0, icase_opt = 0, collapse_opt = 0;
  struct mu_option options[] = {
    { "icase", 'i', NULL, MU_OPTION_DEFAULT,
      "ignore case", mu_c_incr, &icase_opt },
    { "sub", 's', NULL, MU_OPTION_DEFAULT,
      "treat each wildcard as regexp parenthesized group",
      mu_c_incr, &sub_opt },
    { "collapse", 'c', NULL, MU_OPTION_DEFAULT,
      "collapse contiguous runs of *", mu_c_incr, &collapse_opt },
    MU_OPTION_END
  };    
        
  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC, "convert globbing pattern to regexp",
		 MU_CLI_OPTION_PROG_ARGS, "PATTERN [STRING ...]",
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
		 MU_CLI_OPTION_RETURN_ARGV, &argv,
		 MU_CLI_OPTION_END);

  if (icase_opt)
    flags |= MU_GLOBF_ICASE;
  if (sub_opt)
    flags |= MU_GLOBF_SUB;
  if (collapse_opt)
    flags |= MU_GLOBF_COLLAPSE;
  
  if (argc == 0)
    {
      mu_error ("pattern must be given; try %s --help for details",
		mu_program_name);
      return 1;
    }

  i = 0;
  pattern = argv[i++];
  
  if (i == argc)
    {
      char *regstr;
      
      rc = mu_glob_to_regex (&regstr, pattern, flags);
      if (rc)
	{
	  mu_error ("convert: %s", mu_strerror (rc));
	  return 1;
	}
      mu_printf ("%s\n", regstr);
      free (regstr);
    }
  else
    {
      regex_t regex;
      size_t nmatch = 0;
      regmatch_t *matches = NULL;
      
      rc = mu_glob_compile (&regex, pattern, flags);
      if (rc)
	{
	  mu_error ("compile: %s", mu_strerror (rc));
	  return 1;
	}

      if (flags & MU_GLOBF_SUB)
	{
	  nmatch = regex.re_nsub + 1;
	  matches = mu_calloc (nmatch, sizeof matches[0]);
	}
      
      for (; i < argc; i++)
	{
	  char *a = argv[i];
	  rc = regexec (&regex, a, nmatch, matches, 0);
	  mu_printf ("%s: %s\n", a, rc == 0 ? "OK" : "NO");
	  if (flags & MU_GLOBF_SUB)
	    {
	      size_t j;

	      for (j = 0; j < nmatch; j++)
 		printf ("%02zu: %.*s\n", j,
			(int) (matches[j].rm_eo - matches[j].rm_so),
			a + matches[j].rm_so);
	    }
	}
    }
  return 0;
}
