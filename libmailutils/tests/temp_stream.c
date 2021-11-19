/*
NAME
  temp_stream - test for the temp_stream implementation

SYNOPSIS
  temp_stream

DESCRIPTION
  A temporary streams works as a memory stream until its size reaches
  a preconfigured threshold value.  Then, it writes all data collected
  so far to a temporary file and from then on operates as a file stream.

  This test program creates a temporary stream with a threshold of MAXMEM
  bytes.  It writes exactly MAXMEM bytes to the stream, reads that data
  back to ensure they are the same and verifies that the stream has not
  yet switched to the temporary file mode.

  Then, another byte is written, which should trigger conversion to
  temporary file and the stream mode is tested again.

  Finally, MAXMEM-1 more bytes are written, read back and compared.

  The exit status is 0 if all the above passed as expected, and 1
  otherwise.  If any unhandled error occurred, the program aborts.

  To test the current stream mode, the MU_IOCTL_FD ioctl is used.  If
  the stream responds successfully to the MU_IOCTL_FD_GET_BORROW ioctl
  operation, then it is a file stream.

SEE ALSO
  libmailutils/stream/test_stream.c - implementation of the temporary
  stream.

LICENSE
  This file is part of GNU mailutils.
  Copyright (C) 2020-2021 Free Software Foundation, Inc.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <mailutils/mailutils.h>
#include <mailutils/sys/stream.h>
#include <mailutils/sys/temp_stream.h>

#define MAXMEM 32

extern int mu_temp_stream_create (mu_stream_t *pstream, size_t max_size);

static void
verify (mu_stream_t str, int len)
{
  char buf[2*MAXMEM];
  int i;
  
  MU_ASSERT (mu_stream_seek (str, 0, MU_SEEK_SET, NULL));
  MU_ASSERT (mu_stream_read (str, buf, len, NULL));
  for (i = 0; i < len; i++)
    {
      if (buf[i] != i)
	{
	  mu_error ("bad octet %d: %d", i, buf[i]);
	  exit (1);
	}
    }
}

static int
is_file_backed_stream (mu_stream_t str)
{
  int state;
  return mu_stream_ioctl (str, MU_IOCTL_FD, MU_IOCTL_FD_GET_BORROW, &state)
         == 0;
}

int
main (int argc, char **argv)
{
  mu_stream_t str;
  char i;
  
  MU_ASSERT (mu_temp_stream_create (&str, MAXMEM));
  for (i = 0; i < MAXMEM; i++)
    {
      MU_ASSERT (mu_stream_write (str, &i, 1, NULL));
    }

  verify (str, MAXMEM);

  if (is_file_backed_stream (str))
    {
      mu_error ("stream switched to file backend too early");
      return 1;
    }

  MU_ASSERT (mu_stream_write (str, &i, 1, NULL));
  ++i;
  if (!is_file_backed_stream (str))
    {
      mu_error ("stream failed to switch to file backend");
      return 1;
    }
      
  for (; i < 2*MAXMEM; i++)
    {
      MU_ASSERT (mu_stream_write (str, &i, 1, NULL));
    }

  verify (str, 2*MAXMEM);

  mu_stream_destroy (&str);

  return 0;
}
