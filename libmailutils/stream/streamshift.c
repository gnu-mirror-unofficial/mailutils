/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2020 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. */

/*
 * This file implements a generic function for shifting file contents
 * in place.
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mailutils/types.h>
#include <mailutils/stream.h>
#include <mailutils/diag.h>

/* Shift contents of the stream starting at OFF_B to OFF_A */
static int
stream_shift_up (mu_stream_t str, mu_off_t off_a, mu_off_t off_b,
		 size_t bufsize)
{
  int rc;
  char *buffer = NULL;
  mu_off_t length;

  /* Eliminate obvious cases: */
  
  /* 1. Negative offsets */
  if (off_a < 0 || off_b < 0)
    return EINVAL;

  /* 2. Offsets out of order. */
  if (off_b < off_a)
    return EINVAL;

  /* 3. Offsets are equal.  Nothing to do. */
  if (off_b == off_a)
    return 0;
	
  rc = mu_stream_size (str, &length);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_size", NULL, rc);
      return rc;
    }

  /* 4. Offsets out of range. */
  if (off_a > length || off_b > length)
    return EINVAL;
  
  /* 5. Shift starting past the last byte: Nothing to do */
  if (off_b == length)
    return 0;
    
  /* Do the actual shift. */
    
  /* Select the buffer size */
  if (bufsize == 0)
    /* Try to shift in one go. */
    bufsize = length - off_b;
    
  /* Allocate the buffer */
  while (bufsize)
    {
      buffer = malloc (bufsize);
      if (buffer)
	break;
      bufsize /= 2;
    }
  if (!buffer)
    return ENOMEM;
    
  /* Shift */
  while (1)
    {
      size_t n;
	
      rc = mu_stream_seek (str, off_b, MU_SEEK_SET, NULL);
      if (rc)
	{
	  mu_diag_funcall(MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
	  break;
	}

      rc = mu_stream_read (str, buffer, bufsize, &n);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_read", NULL, rc);
	  break;
	}

      if (n == 0)
	break;

      off_b += n;
      
      rc = mu_stream_seek (str, off_a, MU_SEEK_SET, NULL);
      if (rc)
	{
	  mu_diag_funcall(MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
	  break;
	}

      rc = mu_stream_write (str, buffer, n, NULL);
      if (rc)
	{
	  mu_diag_funcall(MU_DIAG_ERROR, "mu_stream_write", NULL, rc);
	  break;
	}
      off_a += n;
    }

  if (rc == 0)
    {
      rc = mu_stream_truncate (str, off_a);
      if (rc)
	mu_diag_funcall(MU_DIAG_ERROR, "mu_stream_truncate", NULL, rc);
    }
  
  free (buffer);
  return rc;
}

/* Shift contents of the stream starting at OFF_A to OFF_B */
static int
stream_shift_down (mu_stream_t str, mu_off_t off_a, mu_off_t off_b,
		   size_t bufsize)
{
  int rc;
  char *buffer = NULL;
  mu_off_t length;
  mu_off_t nshift;
	
  /* Eliminate obvious cases */

  /* 1. Negative offsets */
  if (off_a < 0 || off_b < 0)
    return EINVAL;

  /* 2. Offsets out of order. */
  if (off_b < off_a)
    return EINVAL;

  /* 3. Offsets are equal.  Nothing to do. */
  if (off_b == off_a)
    return 0;
	
  rc = mu_stream_size (str, &length);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_size", NULL, rc);
      return rc;
    }

  /* 4. Offsets out of range. */
  if (off_a > length || off_b > length)
    return EINVAL;

  /* Do the actual shift. */

  /* Select the buffer size */
  if (bufsize == 0 || bufsize > length - off_b)
    /* Try to shift in one go. */
    bufsize = length - off_a;

  /* Allocate the buffer */
  while (bufsize)
    {
      buffer = malloc (bufsize);
      if (buffer)
	break;
      bufsize /= 2;
    }
  if (!buffer)
    return ENOMEM;
  
  nshift = off_b - off_a;
  off_b = length;
	
  /* Shift */
	
  do
    {
      size_t n;
		
      if (off_b - off_a >= bufsize)
	{
	  n = bufsize;
	}
      else
	{
	  n = off_b - off_a;
	}
				
      off_b -= n;
      rc = mu_stream_seek (str, off_b, MU_SEEK_SET, NULL);
      if (rc)
	{
	  mu_diag_funcall(MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
	  break;
	}

      rc = mu_stream_read (str, buffer, n, NULL);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_read", NULL, rc);
	  break;
	}

      rc = mu_stream_seek (str, nshift - (mu_off_t)n, MU_SEEK_CUR, NULL);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
	  break;
	}
      rc = mu_stream_write (str, buffer, n, NULL);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_write", NULL, rc);
	  break;
	}
      mu_stream_flush (str);
    }
  while (off_a < off_b);
	
  free (buffer);
  return rc;
}

/* Shift contents of the stream starting at OFF_B to OFF_A */
int
mu_stream_shift (mu_stream_t str, mu_off_t off_a, mu_off_t off_b,
		 size_t bufsize)
{
  mu_off_t needle, size;
  int rc;

  /* Save current position for restoring it later. */
  rc = mu_stream_seek (str, 0, MU_SEEK_CUR, &needle);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
      return rc;
    }

  /* Do the actual job.*/
  if (off_b > off_a)
    rc = stream_shift_up (str, off_a, off_b, bufsize);
  else
    rc = stream_shift_down (str, off_b, off_a, bufsize);

  if (rc == 0)
    {
      /* Restore the stream position on success.
	 If old position is greater than the actual stream size, leave
	 the stream pointer where it is (i.e. at end).
      */
      rc = mu_stream_seek (str, 0, MU_SEEK_END, &size);
      if (rc == 0 && needle < size)
	{
	  rc = mu_stream_seek (str, needle, MU_SEEK_SET, NULL);
	}
    }

  return rc;
}
