/*
NAME
  fltcnt - check how mu_filter_chain_create() changes the reference counter
           of its input stream.

DESCRIPTION
  On success, mu_filter_chain_create shall increment the reference counter
  if its "transport" argument by 1.  On error, the reference counter shall
  remain unchanged.  Versions of mailutils prior to 2021-07-15 failed to
  meet the latter requirement.
  
  The program checks how the input reference counter changes across two
  calls to mu_filter_chain_create: one that succeeds and other that fails.
  If the changes are as described above, it returns 0.  Otherwise it prints
  a diagnostics message on standard error and returns 1.

LICENSE
  GNU Mailutils -- a suite of utilities for electronic mail
  Copyright (C) 2021 Free Software Foundation, Inc.

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
#include <mailutils/mailutils.h>
#include <mailutils/sys/stream.h>

int
main (int argc, char **argv)
{
  mu_stream_t in, flt;
  int rc;
  char *fargv[] = { "7bit", "+", "7bit", NULL };
  int init_ref_count;

  /* Create input stream and increase its reference counter. */ 
  MU_ASSERT (mu_stdio_stream_create (&in, MU_STDIN_FD, 0));
  mu_stream_ref (in);
  /* Save the initial reference counter. */
  init_ref_count = in->ref_count;

  /*
   * First pass.
   *
   * Check if input reference counter increases by 1 after successfull
   * call to mu_filter_chain_create.
   */
  
  /* Create valid filter chain */
  rc = mu_filter_chain_create (&flt, in,
			       MU_FILTER_ENCODE,
			       MU_STREAM_READ,
			       MU_ARRAY_SIZE(fargv) - 1,
			       (char**) fargv);
  if (rc != 0)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "first mu_filter_chain_create", NULL, rc);
      return 1;
    }

  /* Check if ref_count value is as expected */
  if (in->ref_count != init_ref_count + 1)
    {
      mu_error ("success filter: unexpected ref_count: %lu",
		(unsigned long) in->ref_count);
      return 1;
    }

  /* Destroy the filter and check if ref_count dropped to its initial value. */
  mu_stream_destroy (&flt);
      
  if (in->ref_count != init_ref_count)
    {
      mu_error ("after destroying success filter: unexpected ref_count: %lu",
		(unsigned long) in->ref_count);
      return 1;
    }

  /*
   * Second pass.
   *
   * Check if input reference counter remains unchanged after a failed
   * call to mu_filter_chain_create.
   */

  /* Request unexisting filter. */
  fargv[2] = "there_is_no_such_filter";
  rc = mu_filter_chain_create (&flt, in,
			       MU_FILTER_ENCODE,
			       MU_STREAM_READ,
			       MU_ARRAY_SIZE(fargv) - 1,
			       (char**) fargv);
  if (rc == 0)
    {
      mu_error ("second mu_filter_chain_create succeeded where it should not");
      return 1;
    }

  if (in->ref_count != init_ref_count)
    {
      mu_error ("after failed filter attempt: unexpected ref_count: %lu",
		(unsigned long) in->ref_count);
      return 1;
    }
  
  return 0;
}

  
