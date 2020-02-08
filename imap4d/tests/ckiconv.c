/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019-2020 Free Software Foundation, Inc.

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
#include <stdlib.h>
#include <assert.h>
#include <mailutils/mailutils.h>

static mu_stream_t
base64stream(int flg)
{
  mu_stream_t flt, str;
  int mode;

  switch (flg)
    {
    case MU_STREAM_READ:
      mode = MU_FILTER_DECODE;
      str = mu_strin;
      break;

    case MU_STREAM_WRITE:
      mode = MU_FILTER_ENCODE;
      str = mu_strout;
      break;

    default:
      abort ();
    }
  MU_ASSERT (mu_filter_create (&flt, str, "base64", mode, flg));
  return flt;
}

/* usage: ckiconv F T

   Reads base64-encoded stream from standard input, decodes and converts
   it from character set F to character set T, encodes the result back to
   base64 and prints it on the standard output.

   Exits with code 0 on success, 1 on failure.
*/

int
main (int argc, char **argv)
{
  mu_stream_t flt, input, output;
  char const *iargv[] = { "iconv", NULL, NULL, NULL };

  assert (argc == 3);
  iargv[1] = argv[1];
  iargv[2] = argv[2];
  mu_stdstream_setup (MU_STDSTREAM_RESET_NONE);
  input = base64stream(MU_STREAM_READ);
  output = base64stream(MU_STREAM_WRITE);
  MU_ASSERT (mu_filter_chain_create (&flt, input,
				     MU_FILTER_ENCODE,
				     MU_STREAM_READ,
				     MU_ARRAY_SIZE (iargv) - 1, (char**) iargv));
  mu_stream_unref (input);
  MU_ASSERT (mu_stream_copy (output, flt, 0, NULL));
  mu_stream_destroy (&flt);
  mu_stream_destroy (&output);
  return 0;
}
