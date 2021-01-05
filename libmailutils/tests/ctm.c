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

#include "config.h"
#include "mailutils/mailutils.h"

void
usage (void)
{
  mu_stream_printf (mu_strerr, "ctm [-c] [-n] PATLIST TYPE [TYPE...]\n");
  mu_stream_printf (mu_strerr, "checks content-type matchers\n");
  mu_stream_printf (mu_strerr, "Options:\n");
  mu_stream_printf (mu_strerr, "   -c   test mu_mailcap_content_type_match\n");
  mu_stream_printf (mu_strerr, "   -n   set delim=0\n");
}

int
main (int argc, char **argv)
{
  int delim = ',';
  int ctflag = 0;
  char *arg;
  char *pattern;

  mu_stdstream_setup (MU_STDSTREAM_RESET_NONE);
  while (--argc && (arg = *++argv)[0] == '-')
    {
      if (strcmp (arg, "--") == 0)
	{
	  argc--;
	  argv++;
	  break;
	}
      else
	{
	  while (*++arg)
	    {
	      switch (*arg)
		{
		case 'c':
		  ctflag = 1;
		  break;

		case 'n':
		  delim = 0;
		  break;

		default:
		  mu_error ("unrecognized option: -%c", *arg);
		  usage ();
		  return 1;
		}
	    }
	}
    }

  if (argc < 2)
    {
      usage ();
      return 1;
    }

  argc--;
  pattern = *argv++;
  while (argc--)
    {
      int rc;

      arg = *argv++;
      if (ctflag)
	{
	  struct mu_content_type ct;
	  char *p;

	  memset (&ct, 0, sizeof (ct));
	  ct.type = arg;
	  p = strchr (arg, '/');
	  if (!p)
	    {
	      mu_error ("%s: malformed argument\n", arg);
	      continue;
	    }
	  *p = 0;
	  ct.subtype = p + 1;
	  rc = mu_mailcap_content_type_match (pattern, delim, &ct);
	  *p = '/';
	}
      else
	rc = mu_mailcap_string_match (pattern, delim, arg);
      mu_printf ("%s: %d\n", arg, rc);
    }
  return 0;
}
