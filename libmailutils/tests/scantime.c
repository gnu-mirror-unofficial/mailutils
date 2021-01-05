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
  char *format = "%d-%b-%Y%? %H:%M:%S %z";
  char *buf = NULL;
  size_t size = 0;
  size_t n;
  int line;

  struct mu_option options[] = {
    { "format", 0, "FMT", MU_OPTION_DEFAULT,
      "set scanning format", mu_c_string, &format },
    MU_OPTION_END
  };

  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_SINGLE_DASH,
		 MU_CLI_OPTION_PROG_DOC, "mu_scan_datetime tester",
		 MU_CLI_OPTION_END);

  line = 0;
  while ((rc = mu_stream_getline (mu_strin, &buf, &size, &n)) == 0 && n > 0)
    {
      char *endp;
      struct tm tm;
      struct mu_timezone tz;

      line++;
      mu_ltrim_class (buf, MU_CTYPE_BLANK);
      mu_rtrim_class (buf, MU_CTYPE_ENDLN);
      if (!*buf)
	continue;
      rc = mu_scan_datetime (buf, format, &tm, &tz, &endp);
      switch (rc)
	{
	case 0:
	  break;
	  
	case MU_ERR_PARSE:
	  if (*endp)
	    mu_error ("%d: parse failed near %s", line, endp);
	  else
	    mu_error ("%d: parse failed at end of input", line);
	  continue;

	case MU_ERR_FORMAT:
	  mu_error ("%d: error in format string near %s", line, endp);
	  continue;

	default:
	  mu_error ("%d: %s", line, mu_strerror (rc));
	  exit (1);
	}
      if (*endp)
	mu_printf ("# %d: stopped at %s\n", line, endp);
      mu_printf ("sec=%d,min=%d,hour=%d,mday=%d,mon=%d,year=%d,wday=%d,yday=%d,tz=%d\n",
		 tm.tm_sec, tm.tm_min, tm.tm_hour, tm.tm_mday, tm.tm_mon,
		 tm.tm_year, tm.tm_wday, tm.tm_yday, tz.utc_offset);
    }
  
  if (rc)
    {
      mu_error ("%s", mu_strerror (rc));
      return 1;
    }
  return 0;
}
