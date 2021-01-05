/* This file is part of GNU Mailutils test suite
   Copyright (C) 2020-2021 Free Software Foundation, Inc.

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
#include <stdio.h>
#include <mailutils/mailutils.h>

char mem[1024] = "00000000\n"
                 "11111111\n"
                 "22222222\n"
                 "33333333\n";

static char pattern[] = { 'A', 'B' };

void
test1 (mu_stream_t str)
{
  char buf[128];
  size_t n;
  
  MU_ASSERT (mu_stream_readline (str, buf, sizeof (buf), &n));
  MU_ASSERT (mu_stream_seek (str, 2, MU_SEEK_SET, NULL));
  MU_ASSERT (mu_stream_write (str, pattern, sizeof (pattern), NULL));
}

void
test2 (mu_stream_t str)
{
  MU_ASSERT (mu_stream_flush (str));
}

void
test3 (mu_stream_t str)
{
  MU_ASSERT (mu_stream_seek (str, strlen(mem), MU_SEEK_SET, NULL));
  MU_ASSERT (mu_stream_write (str, pattern, sizeof (pattern), NULL));
}

void
test4 (mu_stream_t str)
{
  MU_ASSERT (mu_stream_write (str, "ZZ\n", 3, NULL));
}

static struct test {
  char *name;
  void (*fun) (mu_stream_t str);
  char *expect;
} test[] = {
  {
    "First pattern write",
    test1,
    "00000000\n"
    "11111111\n"
    "22222222\n"
    "33333333\n"
  },
  {
    "Flush",
    test2,
    "00AB0000\n"
    "11111111\n"
    "22222222\n"
    "33333333\n"
  },
  {
    "Second pattern write",
    test3,
    "00AB0000\n"
    "11111111\n"
    "22222222\n"
    "33333333\n"
  },
  {
    "Newline added",
    test4,
    "00AB0000\n"
    "11111111\n"
    "22222222\n"
    "33333333\n"
    "ABZZ\n"
  },
  
  { NULL }
};

int
main(int argc, char **argv)
{
  mu_stream_t str;
  int i;
  
  MU_ASSERT (mu_fixed_memory_stream_create (&str, mem, sizeof (mem),
					    MU_STREAM_RDWR));
  MU_ASSERT (mu_stream_set_buffer (str, mu_buffer_line, 512));

  for (i = 0; test[i].name; i++)
    {
      test[i].fun (str);
      if (memcmp (mem, test[i].expect, strlen (test[i].expect)))
	{
	  fprintf (stderr, "FAIL: %s\n", test[i].name);
	  fwrite (mem, 1, strlen (mem), stderr);
	  return 1;
	}
    }
  
  return 0;
}

