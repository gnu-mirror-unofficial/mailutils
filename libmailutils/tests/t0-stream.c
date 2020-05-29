/* This file is part of GNU Mailutils test suite
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

/*
 * Mailutils versions up to release-3.9-11-ge863e55 contained a bug in stream
 * buffering code, due to which a write occurring after a read caused the
 * buffer to be flushed and the stream pointer advanced to the next chunk.
 * As a result, the data were written to the next buffer.  This test verifies
 * that it is no longer the case by
 *   1. Reading 128 bytes at position 10
 *   2. Writing 2 bytes at current position.
 *   3. Reverting to position 138
 *   4. Reading 2 bytes.
 * Obviously, the bytes obtained in 4 should be the same as those written in 2.
 */

#include <config.h>
#include <mailutils/mailutils.h>

char mem[1024];

int
main(int argc, char **argv)
{
  mu_stream_t str;
  char buf[128];
  static char pattern[] = { 'A', 'B' };

  memset (mem, '.', sizeof (mem));
  MU_ASSERT (mu_fixed_memory_stream_create (&str, mem, sizeof (mem),
					    MU_STREAM_RDWR));
  MU_ASSERT (mu_stream_set_buffer (str, mu_buffer_full, 512));

  MU_ASSERT (mu_stream_seek (str, 10, MU_SEEK_SET, NULL));
  MU_ASSERT (mu_stream_read (str, buf, sizeof (buf), NULL));
  MU_ASSERT (mu_stream_write (str, pattern, sizeof (pattern), NULL));
  
  MU_ASSERT (mu_stream_seek (str, -2, MU_SEEK_CUR, NULL));
  MU_ASSERT (mu_stream_read (str, buf, sizeof (pattern), NULL));
  if (memcmp (buf, pattern, sizeof (pattern)))
    {
      int i;
      static int bs = 64;
      fprintf (stderr, "FAIL\n");
      mu_stream_flush (str);
      for (i = 0; i < sizeof (mem); i++)
	{
	  if (i % bs == 0)
	    {
	      if (i)
		putchar ('\n');
	      printf ("%03X: ", i);
	    }
	  putchar (mem[i]);
	}
      putchar ('\n');
      return 1;
    }
  return 0;
}
  
