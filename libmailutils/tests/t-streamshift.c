/* 
NAME
  streamshift - test temporary stream shift
  
SYNOPSIS
  streamshift [-ad] [-b SIZE] [-i FILE] [-s SIZE] FROM TO

DESCRIPTION
  Creates a temporary stream of given size, initializes it with a
  predefined pattern or from supplied FILE, shifts its contents
  starting at offset FROM to new offset TO and verifies the result.

  Unless the -i option is given, the stream is initialized with a
  repeated pattern of 256 characters.
  
OPTIONS
  -a, --alnum
        Fill the pattern with alphanumeric characters only.
	
  -b, --bufsize=SIZE
        Set buffer size for the shift operation.  This option has
	effect only if the data are shifted in place, i.e. if
	mailfromd is compiled with mailutils version 3.9.90 or
	later.

  -d, --dump
        Dump the resulting stream on stdout.

  -i, --init-file=FILE
        Initialize temporary stream from FILE.

  -s, --init-size=SIZE
        Set initial size for the temporary stream.  Default is
	four times the MU_STREAM_DEFBUFSIZ constant (32768).

EXIT STATUS
   0    Success.
   1    Failure.
   2    Usage error.
	
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

#include <config.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <mailutils/mailutils.h>

static unsigned long
strnum (char const *str)
{
  char *p;
  unsigned long n;
  errno = 0;
  n = strtol (str, &p, 10);
  assert (*p == 0 && errno == 0);
  return n;
}

char pattern[256];

static void
gen_pattern (int alnum)
{
  int i, c;

  for (i = c = 0; i < sizeof(pattern); i++, c++)
    {
      if (alnum)
	{
	  while (!mu_isalnum (c))
	    c = (c + 1) % sizeof (pattern);
	}
      pattern[i] = c;
    }
}

static void
stream_fill (mu_stream_t str, size_t size)
{
  while (size)
    {
      size_t n = sizeof (pattern);
      if (n > size)
	n = size;
      MU_ASSERT (mu_stream_write (str, pattern, n, NULL));
      size -= n;
    }
}

static void
stream_verify_part (mu_stream_t str, int patstart, mu_off_t off, mu_off_t size)
{
  char buffer[sizeof(pattern)];
  char pat[sizeof(pattern)];
  int i;
  
  for (i = 0; i < sizeof(pat); i++)
    {
      pat[i] = pattern[patstart];
      patstart = (patstart + 1) % sizeof (pattern);
    }
	
  MU_ASSERT (mu_stream_seek (str, off, MU_SEEK_SET, NULL));
  while (size)
    {
      size_t n = sizeof (buffer);
      if (n > size)
	n = size;
      MU_ASSERT (mu_stream_read (str, buffer, n, NULL));
      if (memcmp (buffer, pat, n))
	{
	  mu_off_t pos;
	  MU_ASSERT (mu_stream_seek (str, 0, MU_SEEK_CUR, &pos));
	  fprintf (stderr, "%lu: chunk differs\n", pos - n);
	  exit (1);
	}
      size -= n;
    }
}

static void
stream_verify (mu_stream_t str, mu_off_t isize, mu_off_t a, mu_off_t b)
{
  mu_off_t size;
	
  MU_ASSERT (mu_stream_seek (str, 0, MU_SEEK_END, &size));
  if (size != isize + a - b)
    {
      fprintf (stderr,
	       "actual and expected sizes differ: %lu != %lu\n",
	       (unsigned long) size,
	       (unsigned long) (isize + a - b));
      exit (1);
    }
	
  stream_verify_part (str, 0, 0, a < b ? a : b);
  stream_verify_part (str, b % sizeof (pattern), a, size);
}

static void
file_verify_part (mu_stream_t str, mu_stream_t orig, mu_off_t str_off,
		  mu_off_t orig_off, mu_off_t size)
{
  char str_buf[sizeof (pattern)];
  char orig_buf[sizeof (pattern)];
	
  MU_ASSERT (mu_stream_seek (str, str_off, MU_SEEK_SET, NULL));
  MU_ASSERT (mu_stream_seek (orig, orig_off, MU_SEEK_SET, NULL));
  while (size)
    {
      size_t n = sizeof (str_buf);
      if (n > size)
	n = size;
      MU_ASSERT (mu_stream_read (str, str_buf, n, NULL));
      MU_ASSERT (mu_stream_read (orig, orig_buf, n, NULL));
      if (memcmp (str_buf, orig_buf, n))
	{
	  mu_off_t pos;
	  MU_ASSERT (mu_stream_seek (str, 0, MU_SEEK_CUR, &pos));
	  fprintf (stderr, "%lu: chunk differs\n", pos - n);
	  exit (1);
	}
      size -= n;
    }
}

static void
file_verify (mu_stream_t str, mu_stream_t orig, mu_off_t isize,
	     mu_off_t a, mu_off_t b)
{
  mu_off_t size;
	
  MU_ASSERT (mu_stream_seek (str, 0, MU_SEEK_END, &size));
  if (size != isize + a - b)
    {
      fprintf (stderr,
	       "actual and expected sizes differ: %lu != %lu\n",
	       (unsigned long) size,
	       (unsigned long) (isize + a - b));
      exit (1);
    }
  file_verify_part (str, orig, 0, 0, a < b ? a : b);
  file_verify_part (str, orig, a, b, size - a);
}

int
main (int argc, char **argv)
{
  int ascii_mode = 0;
  size_t bs = 0;
  mu_off_t from_off;
  mu_off_t to_off;
  mu_off_t init_size = 0;
  char *init_file = NULL;
  int dump_opt = 0;
  mu_stream_t temp;
  mu_stream_t str;

  struct mu_option options[] = {
    { "alnum", 'a', NULL, MU_OPTION_DEFAULT,
      "fill the pattern with alphanumeric characters only",
      mu_c_incr, &ascii_mode },
    { "bufsize", 'b', "N", MU_OPTION_DEFAULT,
      "size of the buffer for shift operations", mu_c_size, &bs },
    { "dump", 'd', NULL, MU_OPTION_DEFAULT,
      "dump the resulting stream on the stdout at the end of the run",
      mu_c_incr, &dump_opt },
    { "init-file", 'i', "FILE", MU_OPTION_DEFAULT,
      "initialize source stream from FILE",
      mu_c_string, &init_file },
    { "init-size", 's', "N", MU_OPTION_DEFAULT,
      "initial size of the stream", mu_c_off, &init_size },
    MU_OPTION_END
  };

  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC, "mu_stream_shift test",
		 MU_CLI_OPTION_PROG_ARGS, "FROM_OFF TO_OFF",
		 MU_CLI_OPTION_EX_USAGE, 2,
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
		 MU_CLI_OPTION_RETURN_ARGV, &argv,		 
		 MU_CLI_OPTION_END);

  if (argc != 2)
    {
      mu_error ("expected exactly two arguments; try %s --help for assistance",
		mu_program_name);
      exit(2);
    }

  from_off = strnum (argv[0]);
  to_off = strnum (argv[1]);
  
  MU_ASSERT (mu_temp_file_stream_create (&temp, NULL, 0));
  if (init_file)
    {
      MU_ASSERT (mu_file_stream_create (&str, init_file, MU_STREAM_READ));
      MU_ASSERT (mu_stream_copy (temp, str, 0, NULL));
      if (init_size)
	MU_ASSERT (mu_stream_truncate (temp, init_size));
    }
  else
    {
      gen_pattern (ascii_mode);
      if (!init_size)
	init_size = 4 * MU_STREAM_DEFBUFSIZ;
      stream_fill (temp, init_size);
    }
  MU_ASSERT (mu_stream_seek (temp, 0, MU_SEEK_CUR, &init_size));
	
  MU_ASSERT (mu_stream_shift (temp, to_off, from_off, bs));
  if (init_file)
    file_verify (temp, str, init_size, to_off, from_off);
  else
    stream_verify (temp, init_size, to_off, from_off);
  if (dump_opt)
    {
      MU_ASSERT (mu_stream_seek (temp, 0, MU_SEEK_SET, NULL));
      MU_ASSERT (mu_stream_copy (mu_strout, temp, 0, NULL));
    }	
  return 0;
}
