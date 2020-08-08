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

/* Implementation of temp_stream.

   Temp_stream combines the functionality of memory and temp_file streams.
   Streams of that type function as memory streams until their size reaches
   a preconfigured threshold value.  Once it is reached, the stream storage
   is automatically converted to temporary file, and all data written so far
   are transferred to the new storage.  If the temporary file cannot be
   created, the stream continues to operate in memory-based mode.
   
   The stream is created using the following call:
   
      int mu_temp_stream_create (mu_stream_t *pstream, size_t threshold)

   If threshold is 0, the threshold value is first looked up in the
   environment variable MU_TEMP_FILE_THRESHOLD (which should contain
   a string suitable for input to mu_strtosize).  If it is not set or
   unparsable, the mu_temp_file_threshold_size global is used instead.

   Two special values of MU_TEMP_FILE_THRESHOLD alter the behavior of
   mu_temp_stream_create:

     "0"    -  the function creates a pure tempfile-based stream
               (equivalent to mu_temp_file_stream_create).
     "inf"  -  the function returns a pure memory-based stream
               (equivalent to mu_memory_stream_create).
 */
#include <stdlib.h>
#include <errno.h>
#include <mailutils/stream.h>
#include <mailutils/sys/temp_stream.h>
#include <mailutils/cstr.h>
#include <mailutils/diag.h>
#include <mailutils/debug.h>
#include <mailutils/errno.h>

static int
temp_stream_write (struct _mu_stream *str, const char *buf, size_t size,
		   size_t *ret_size)
{
  struct _mu_temp_stream *ts = (struct _mu_temp_stream *)str;
  
  if (ts->s.mem.offset + size > ts->max_size)
    {
      int rc;
      mu_stream_t temp_file;
      rc = mu_temp_file_stream_create (&temp_file, NULL, 0);
      if (rc == 0)
	{
	  if (ts->s.mem.ptr == NULL)
	    rc = 0;
	  else
	    {
	      size_t s = 0;

	      while (s < ts->s.mem.size)
		{
		  size_t n = ts->s.mem.size - s;
		  size_t wrn;
		  
		  rc = temp_file->write (temp_file, ts->s.mem.ptr + s, n, &wrn);
		  if (rc)
		    break;
		  s += wrn;
		}
	      
	      if (rc == 0)
		{
		  mu_off_t res;
		  rc = temp_file->seek (temp_file, str->offset, &res);
		}
	    }
	  
	  if (rc == 0)
	    {
	      /* Preserve the necessary stream data */
	      temp_file->ref_count = str->ref_count;
	      temp_file->buftype = str->buftype;
	      temp_file->buffer = str->buffer;
	      temp_file->level = str->level;
	      temp_file->pos = str->pos;

	      temp_file->statmask = str->statmask;
	      temp_file->statbuf = str->statbuf;

	      /* Deinitialize previous stream backend */
	      ts->s.stream.done (str);

	      /* Replace it with the newly created one. */
	      memcpy (&ts->s.file, temp_file, sizeof (ts->s.file));

	      /* Reclaim the memory used by the stream object */
	      free (temp_file);

	      /* Write data to the new stream. */
	      return ts->s.stream.write (str, buf, size, ret_size);
	    }
	}
      else
	{
	  mu_diag_funcall (MU_DIAG_WARNING, "mu_temp_file_stream_create",
			   NULL, rc);
	  /* Switch to plain memory stream mode */
	  ts->s.stream.write = ts->saved_write;
	}
    }

  return ts->saved_write (str, buf, size, ret_size);
}

size_t mu_temp_file_threshold_size = 4096;

int
mu_temp_stream_create (mu_stream_t *pstream, size_t max_size)
{
  int rc;
  mu_stream_t stream;
  struct _mu_temp_stream *str;

  if (max_size == 0)
    {
      char *s;
      if ((s = getenv ("MU_TEMP_FILE_THRESHOLD")) != NULL)
	{
	  char *p;

	  if (strcmp(p, "inf") == 0)
	    return mu_memory_stream_create (&stream, MU_STREAM_RDWR);
	  
	  rc = mu_strtosize (s, &p, &max_size);
	  if (rc == 0)
	    {
	      if (max_size == 0)
		return mu_temp_file_stream_create (pstream, NULL, 0);
	    }
	  else
	    mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		      ("failed parsing MU_TEMP_FILE_THRESHOLD value: %s near %s",
		       mu_strerror (rc), p));
	}
      if (max_size == 0)
	max_size = mu_temp_file_threshold_size;
    }
  
  rc = mu_memory_stream_create (&stream, MU_STREAM_RDWR);
  if (rc)
    return rc;

  str = realloc (stream, sizeof (*str));
  if (!str)
    {
      mu_stream_destroy (&stream);
      return ENOMEM;
    }

  str->max_size = max_size;
  str->saved_write = str->s.stream.write;
  str->s.stream.write = temp_stream_write;

  *pstream = (mu_stream_t) str;
  return rc;
}

