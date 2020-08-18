/*
NAME
  strout - test whether mu_strout or mu_strerr is functioning.

DESCRIPTION
  This test program reads data byte by byte from stdin using the stdio
  input functions and sends them to mu_strout or mu_strerr using the
  mu_stream_write function.

  Both mu_strout and mu_strerr spring into existence the first time they
  are used by any function of the stream family.  Therefore, care is
  taken not to call any mailutils I/O function either directly or
  indirectly prior to the first write to the stream being tested.

OPTIONS
  -err
      Write to mu_strerr.
      
  -reset
      Call mu_stdstream_setup with the appropriate MU_STDSTREAM_RESET_
      flag explicitly.
      
LICENSE
  GNU Mailutils -- a suite of utilities for electronic mail
  Copyright (C) 2011-2020 Free Software Foundation, Inc.

  GNU Mailutils is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3, or (at your option)
  any later version.

  GNU Mailutils is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mailutils/stream.h>
#include <mailutils/stdstream.h>
#include <mailutils/diag.h>
#include <mailutils/errno.h>

int
main (int argc, char **argv)
{
  mu_stream_t str = mu_strout;
  int i;
  char c;
  
  for (i = 1; i < argc; i++)
    {
      char *arg = argv[i];
      if (arg[0] == '-')
	{
	  if (strcmp (arg, "-out") == 0)
	    str = mu_strout;
	  else if (strcmp (arg, "-err") == 0)
	    str = mu_strerr;
	  else if (strcmp (arg, "-reset") == 0)
	    {
	      if (str == mu_strout)
		{
		  mu_stdstream_setup (MU_STDSTREAM_RESET_STROUT);
		  str = mu_strout;
		}
	      else
		{
		  mu_stdstream_setup (MU_STDSTREAM_RESET_STRERR);
		  str = mu_strerr;
		}
	    }
	  else
	    {
	      fprintf (stderr, "%s: unrecognized option %s\n", argv[0], arg);
	      return 1;
	    }
	}
    }

  while ((c = getchar ()) != EOF)
    {
      size_t n;
      int rc = mu_stream_write (str, &c, 1, &n);
      if (rc)
	{
	  fprintf (stderr, "mu_stream_write: %s", mu_strerror (rc));
	  return 1;
	}
      if (n != 1)
	{
	  fprintf (stderr, "wrote %zu bytes?\n", n);
	  return 1;
	}
    }
  
  return 0;
}
