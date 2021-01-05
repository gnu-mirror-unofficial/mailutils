/*
NAME
  stream-getdelim - test the mu_stream_getdelim function.

DESCRIPTION
  This implements a simple memory-based stream and tests the
  mu_stream_getdelim function on a predefined stream content with
  various combinations of buffering type and buffer size settings.

  On success, returns 0.  On error, prints diagnostics on stderr and
  exits with a non-0 code or aborts.

  Before running each test, its short description is printed on stdout.

  For obvious reasons, libc functions are used for output.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <mailutils/types.h>
#include <mailutils/debug.h>
#include <mailutils/errno.h>
#include <mailutils/stream.h>
#include <mailutils/sys/stream.h>

char content[] =
  "AB\n"
  "\n"
  "\n"
  "CDEFG\n"
  "H\n"
  "IJ\n"
  "KLMNOPQRST\n"
  "UVWXYZ";

struct test_stream
{
  struct _mu_stream stream;
  char *ptr;
  size_t size;
  mu_off_t offset;
};

static int
ts_open (mu_stream_t stream)
{
  struct test_stream *ts = (struct test_stream *) stream;
  ts->ptr = content;
  ts->size = strlen (content);
  ts->offset = 0;
  return 0;
}

static int
ts_read (mu_stream_t stream, char *optr, size_t osize, size_t *nbytes)
{
  struct test_stream *ts = (struct test_stream *) stream;
  size_t n = 0;
  if (ts->ptr != NULL && ((size_t)ts->offset <= ts->size))
    {
      n = ts->size - ts->offset;
      if (n > osize)
	n = osize;
      memcpy (optr, ts->ptr + ts->offset, n);
      ts->offset += n;
    }
  if (nbytes)
    *nbytes = n;
  return 0;
}

static int
ts_seek (mu_stream_t stream, mu_off_t off, mu_off_t *presult)
{ 
  struct test_stream *ts = (struct test_stream *) stream;

  if (off < 0)
    return ESPIPE;
  ts->offset = off;
  *presult = off;
  return 0;
}

static int
ts_size (mu_stream_t stream, mu_off_t *psize)
{
  struct test_stream *ts = (struct test_stream *) stream;
  *psize = ts->size;
  return 0;
}  

int
test_stream_create (mu_stream_t *pstream)
{
  int rc;
  mu_stream_t stream;
  
  stream = _mu_stream_create (sizeof (struct test_stream), MU_STREAM_READ|MU_STREAM_SEEK);
  assert (stream != NULL);

  stream->open = ts_open;
  stream->read = ts_read;
  stream->size = ts_size;
  stream->seek = ts_seek;
  
  rc = mu_stream_open (stream);
  if (rc)
    mu_stream_destroy (&stream);
  else
    *pstream = stream;

  return rc;
}

void
runtest (mu_stream_t str)
{
  char *buf = NULL;
  size_t size = 0;
  mu_off_t off;
  size_t n;
  int rc;
  unsigned long start = 0;
  
  MU_ASSERT (mu_stream_seek (str, 0, MU_SEEK_SET, NULL));
  
  while ((rc = mu_stream_getdelim (str, &buf, &size, '\n', &n)) == 0
	 && n > 0)
    {
      size_t blen = strlen (buf);
      size_t len = strcspn (content + start, "\n");
      if (content[start+len] == '\n')
	len++;
      if (len != blen || memcmp (content + start, buf, len))
	{
	  fprintf (stderr, "at %lu: expected:\n", start);
	  fwrite (content + start, blen, 1, stderr);
	  fprintf (stderr, "\nfound:\n");
	  fwrite (buf, blen, 1, stderr);
	  fputc ('\n', stderr);
	  exit (1);
	}
      start += len;

      MU_ASSERT (mu_stream_seek (str, 0, MU_SEEK_CUR, &off));
      if (off != start)
	{
	  fprintf (stderr, "wrong offset reported (%lu, expected %lu\n",
		   (unsigned long)off, (unsigned long)start);
	  exit (2);
	}
    }
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_getline", NULL, rc);
      exit (1);
    }
  free (buf);
}

#define TESTBUFSIZE 512

static struct test
{
  char *name;
  enum mu_buffer_type type;
  size_t size;
} testtab[] = {
  { "No buffering", mu_buffer_none, 0 },
  { "Linear buffering", mu_buffer_line, TESTBUFSIZE },
  { "Linear buffering (small buffer)", mu_buffer_line, 1 },
  { "Full buffering (big buffer)",  mu_buffer_full, TESTBUFSIZE },
  { "Full buffering (moderate buffer)", mu_buffer_full, sizeof(content)/2 },
  { "Full buffering (small buffer)", mu_buffer_full, 1 },
  { NULL }
};

int
main (int argc, char **argv)
{
  mu_stream_t str;
  int i;
  
  MU_ASSERT (test_stream_create (&str));

  for (i = 0; testtab[i].name; i++)
    {
      printf ("%d: %s\n", i, testtab[i].name);
      MU_ASSERT (mu_stream_set_buffer (str, testtab[i].type, testtab[i].size));
      runtest (str);
    }
  
  mu_stream_destroy (&str);
  return 0;
}
