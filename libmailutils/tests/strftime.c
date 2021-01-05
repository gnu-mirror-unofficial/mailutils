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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mailutils/error.h>
#include <mailutils/errno.h>
#include <mailutils/datetime.h>
#include <mailutils/stream.h>
#include <mailutils/cctype.h>
#include <mailutils/cstr.h>
#include <mailutils/stdstream.h>
#include <mailutils/cli.h>

int
main (int argc, char **argv)
{
  int rc;
  char *format = "%c";
  char *buf = NULL;
  size_t size = 0;
  size_t n;
  struct mu_timezone tz, *tzp = NULL;
  char *tzstr = NULL;
  
  struct mu_option options[] = {
    { "format", 0, "FMT", MU_OPTION_DEFAULT,
      "set scanning format", mu_c_string, &format },
    { "tz", 0, "UTCOFF", MU_OPTION_DEFAULT,
      "set time zone", mu_c_string, &tzstr },
    MU_OPTION_END
  };
  
  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_SINGLE_DASH,
		 MU_CLI_OPTION_PROG_DOC, "mu_c_streamftime tester",
		 MU_CLI_OPTION_END);

  memset (&tz, 0, sizeof tz);
  if (tzstr)
    {
      int sign;
      int n = atoi (tzstr);
      if (n < 0)
	{
	  sign = -1;
	  n = - n;
	}
      else
	sign = 1;
      tz.utc_offset = sign * ((n / 100 * 60) + n % 100) * 60;
      tzp = &tz;
    }
    
  while ((rc = mu_stream_getline (mu_strin, &buf, &size, &n)) == 0 && n > 0)
    {
      char *p;
      struct tm *tm;
      time_t t;

      mu_rtrim_class (buf, MU_CTYPE_ENDLN);

      if (*buf == ';')
	{
	  mu_printf ("%s\n", buf);
	  continue;
	}
      t = strtoul (buf, &p, 10);
      if (*p)
	{
	  mu_error ("bad input line near %s", p);
	  continue;
	}

      tm = gmtime (&t);
      mu_c_streamftime (mu_strout, format, tm, tzp);
      mu_printf ("\n");
    }
  
  if (rc)
    {
      mu_error ("%s", mu_strerror (rc));
      return 1;
    }
  return 0;
}
