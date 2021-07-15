/*
NAME
  fltcnt - checks transport reference counter changes during filter creation.

SYNOPSIS
  fltcnt [NAME...]
  
DESCRIPTION
  Any filter creation call should behave as follows:

  On success, it shall increment the reference counter in its "transport"
  argument by 1.
  On error, the reference counter shall remain unchanged.

  The following functions failed to meet these requirements in mailutils
  versions prior to 2021-07-15:

  mu_filter_chain_create
    On error, this function decremented the transport reference counter.
    This happened, in particular, if an invalid filter has been requested.
    However, if improper arguments were given to a valid filter, the
    mu_filter_stream_create bug described below would compensate for this.

    To check for this bug, the program does two mu_filter_chain_create
    calls: one with a valid filter name, and another one with an inexisting
    filter name.  Reference counter values are verified after each call.
    
  mu_filter_stream_create
    If the mu_filter_xcode_t function failed in mu_filter_init request,
    mu_filter_stream_create would leave the transport reference counter
    incremented by one.

  By default, the program runs both tests in this order.  If any of them
  fails, it prints two diagnostic messages: the first one describes where
  exactly the program failed to meet its expectations and the second one
  gives the name of the test that failed.

  To run a single test, give its name as a command line argument.
 
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
test_mu_filter_chain_create (mu_stream_t in)
{
  mu_stream_t flt;
  int rc;
  char *fargv[] = { "7bit", "+", "7bit", NULL };
  int init_ref_count;

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

static enum mu_filter_result
xfail_transcoder (void *xdata,
		  enum mu_filter_command cmd,
		  struct mu_filter_io *iobuf)
{
  if (cmd == mu_filter_init)
    {
      iobuf->errcode = MU_ERR_USER0;
      return mu_filter_failure;
    }
  return mu_filter_ok;
}

int
test_mu_filter_stream_create (mu_stream_t in)
{
  int rc;
  int init_ref_count;
  mu_stream_t flt;
  void *data = malloc(1);
  
  /* Save the initial reference counter. */
  init_ref_count = in->ref_count;

  rc = mu_filter_stream_create (&flt, in, 
				MU_FILTER_ENCODE, 
				xfail_transcoder,
				data, MU_STREAM_READ);

  if (rc != MU_ERR_USER0)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_filter_stream_create", NULL, rc);
      return 1;
    }

  if (in->ref_count != init_ref_count)
    {
      mu_error ("after failed mu_filter_stream_create: unexpected ref_count: %lu",
		(unsigned long) in->ref_count);
      return 1;
    }
  return 0;
}

int
main (int argc, char **argv)
{
  mu_stream_t in;
  static struct testtab
  {
    char *name;
    int (*test) (mu_stream_t);
  } testtab[] = {
    { "mu_filter_chain_create", test_mu_filter_chain_create },
    { "mu_filter_stream_create", test_mu_filter_stream_create },
    { NULL }
  };
  int i;
  
  /* Create input stream and increase its reference counter. */ 
  MU_ASSERT (mu_stdio_stream_create (&in, MU_STDIN_FD, 0));
  mu_stream_ref (in);

  for (i = 0; testtab[i].name; i++)
    {
      int rc;
      
      if (argc > 1)
	{
	  int j;
	  for (j = 1; j < argc; j++)
	    if (strcmp (argv[j], testtab[i].name) == 0)
	      break;
	  if (j == argc)
	    continue;
	}
      rc = testtab[i].test (in);
      if (rc)
	{
	  mu_error ("%s: FAIL", testtab[i].name);
	  return rc;
	}
    }
  return 0;
}
