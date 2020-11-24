/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2020 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <mailutils/types.h>
#include <mailutils/alloc.h>
#include <mailutils/error.h>
#include <mailutils/errno.h>
#include <mailutils/stream.h>
#include <mailutils/sys/stream.h>

#define STREAMCPY_MIN_BUF_SIZE 2
#define STREAMCPY_MAX_BUF_SIZE 16384

/* Copy SIZE bytes from SRC to DST.  If SIZE is 0, copy everything up to
   EOF.
   If the callback function CBF is not NULL, it will be called for
   each buffer-full of data read with the following arguments: pointer to
   the buffer, length of the data portion in buffer, pointer to the user-
   supplied callback data (CBD).
*/
int
mu_stream_copy_wcb (mu_stream_t dst, mu_stream_t src, mu_off_t size,
		    void (*cbf) (char *, size_t, void *), void *cbd,
		    mu_off_t *pcsz)
{
  int status;
  size_t bufsize, n;
  char *buf;
  mu_off_t total = 0;

  if (pcsz)
    *pcsz = 0;
  if (size == 0)
    {
      status = mu_stream_size (src, &size);
      switch (status)
	{
	case 0:
	  break;

	case ENOSYS:
	  size = 0;
	  break;

	default:
	  return status;
	}

      if (size)
	{
	  mu_off_t pos;
	  status = mu_stream_seek (src, 0, MU_SEEK_CUR, &pos);
	  switch (status)
	    {
	    case 0:
	      if (pos > size)
		return ESPIPE;
	      size -= pos;
	      break;

	    case EACCES:
	      mu_stream_clearerr (src);
	    case ENOSYS:
	      size = 0;
	      break;

	    default:
	      return status;
	    }
	}
    }

  bufsize = size;
  if (!bufsize)
    bufsize = STREAMCPY_MAX_BUF_SIZE;

  for (; (buf = malloc (bufsize)) == NULL; bufsize >>= 1)
    if (bufsize < STREAMCPY_MIN_BUF_SIZE)
      return ENOMEM;

  if (size)
    {
      while (size)
	{
	  size_t rdsize = bufsize < size ? bufsize : size;

	  status = mu_stream_read (src, buf, rdsize, &n);
	  if (status)
	    break;
	  if (n == 0)
	    break;
	  if (cbf)
	    cbf (buf, n, cbd);
	  status = mu_stream_write (dst, buf, n, NULL);
	  if (status)
	    break;
	  size -= n;
	  total += n;
	}

      if (!pcsz && size)
	status = EIO;
    }
  else
    {
      while ((status = mu_stream_read (src, buf, bufsize, &n)) == 0
	     && n > 0)
	{
	  if (cbf)
	    cbf (buf, n, cbd);
	  status = mu_stream_write (dst, buf, n, NULL);
	  if (status)
	    break;
	  total += n;
	}
    }
  
  if (pcsz)
    *pcsz = total;
  /* FIXME: When EOF error code is implemented:
  else if (total == 0)
    status = EOF;
  */
  free (buf);
  return status;
}

/* Copy SIZE bytes from SRC to DST.  If SIZE is 0, copy everything up to
   EOF. */
int
mu_stream_copy (mu_stream_t dst, mu_stream_t src, mu_off_t size,
		mu_off_t *pcsz)
{
  return mu_stream_copy_wcb (dst, src, size, NULL, NULL, pcsz);
}

static void
capture_last_char (char *buf, size_t size, void *data)
{
  int *n = data;
  if (buf[size-1] == '\n')
    {
      if (size == 1)
	{
	  ++*n;
	}
      else if (buf[size-2] == '\n')
	*n = 2;
      else
	*n = 1;
    }
  else
    *n = 0;
}

/* Same as mu_stream_copy, but also ensures that the copied data end with
   two '\n' characters. */
int
mu_stream_copy_nl (mu_stream_t dst, mu_stream_t src, mu_off_t size,
		   mu_off_t *pcsz)
{
  int lc = 0;
  int status = mu_stream_copy_wcb (dst, src, size, capture_last_char, &lc,
				   pcsz);
  if (status == 0 && lc < 2)
    {
      status = mu_stream_write (dst, "\n\n", 2 - lc, NULL);
      if (status == 0 && pcsz)
	++ *pcsz;
    }
  return status;
}

				   
