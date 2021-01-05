/*
NAME
  strin - test whether mu_strin is functioning.

DESCRIPTION
  This test program reads data from mu_strin and prints it on stdout
  using stdio function call.

  The mu_strin object, as well as the other two standard streams, springs
  into existence the first time it is used by any function of the stream
  family.  Therefore, care is taken not to call any mailutils I/O function
  either directly or indirectly prior to the first read from mu_strin.
  
OPTIONS
  -noecho
      If mu_strin is attached to a tty, this option disables echoing of
      the data read.
  
LICENSE
  GNU Mailutils -- a suite of utilities for electronic mail
  Copyright (C) 2011-2021 Free Software Foundation, Inc.

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
  int i, rc;
  int echo_state = 0;
  size_t n;
  int ex = 0;
  
  for (i = 1; i < argc; i++)
    {
      char *arg = argv[i];
      if (arg[0] == '-')
	{
	  if (strcmp (arg, "-noecho") == 0)
	    {
	      MU_ASSERT (mu_stream_ioctl (mu_strin, MU_IOCTL_ECHO,
					  MU_IOCTL_OP_SET,
					  &echo_state));
	      echo_state = 1;
	    }
	  else
	    {
	      fprintf (stderr, "usage: %s [-noecho]\n", argv[0]);
	      return 1;
	    }
	}
    }
	
  while (1)
    {
      char c;
      if ((rc = mu_stream_read (mu_strin, &c, 1, &n)) != 0)
	{
	  fprintf (stderr, "mu_stream_read: %s\n", mu_strerror (rc));
	  ex = 1;
	  break;
	}
      if (n == 0)
	break;
      if (n != 1)
	{
	  fprintf (stderr, "read %zu bytes?\n", n);
	  ex = 1;
	  break;
	}
      fputc (c, stdout);
    }
	
  if (echo_state)
    MU_ASSERT (mu_stream_ioctl (mu_strin, MU_IOCTL_ECHO, MU_IOCTL_OP_SET,
				&echo_state));
  return ex;
}
