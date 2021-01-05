/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2018-2021 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library.  If not, see
   <http://www.gnu.org/licenses/>. */

#include "mu_scm.h"
#include <mailutils/sys/stream.h>

struct port_stream {
	struct _mu_stream stream;
	SCM port;
};

static int
port_stream_read (struct _mu_stream *str, char *buf, size_t bufsize,
		  size_t *pnread)
{
  struct port_stream *pstr = (struct port_stream *)str;
  size_t n = scm_c_read (pstr->port, buf, bufsize);
  if (pnread)
    *pnread = n;
  return 0;
}

static int
port_stream_write (struct _mu_stream *str, const char *buf, size_t bufsize,
		   size_t *pnwrite)
{
  struct port_stream *pstr = (struct port_stream *)str;
  scm_c_write (pstr->port, buf, bufsize);
  if (pnwrite)
    *pnwrite = bufsize;
  return 0;
}

static int
port_stream_seek (struct _mu_stream *str, mu_off_t off, mu_off_t *presult)
{
  struct port_stream *pstr = (struct port_stream *)str;
  SCM ret = scm_seek (pstr->port,
		      scm_from_signed_integer (off),
		      scm_from_int (SEEK_SET));
  if (presult)
    *presult = scm_to_int64 (ret); /* FIXME: scm_to_off_t is not exposed */
  return 0;
}

static int
port_stream_truncate (mu_stream_t stream, mu_off_t size)
{
  struct port_stream *pstr = (struct port_stream *)stream;
  scm_truncate_file (pstr->port, scm_from_signed_integer (size));
  return 0;
}

static void
port_stream_done (struct _mu_stream *str)
{
  struct port_stream *pstr = (struct port_stream *)str;
  scm_gc_unprotect_object (pstr->port);
}

static int
port_stream_size (struct _mu_stream *str, mu_off_t *psize)
{
  struct port_stream *pstr = (struct port_stream *)str;
  mu_off_t cur;
  int rc;
  SCM ret;
  
  rc = mu_stream_seek (str, 0, MU_SEEK_CUR, &cur);
  if (rc)
    return rc;
  ret = scm_seek (pstr->port,
		  scm_from_signed_integer (0),
		  scm_from_int (SEEK_END));
  rc = mu_stream_seek (str, cur, MU_SEEK_SET, NULL);
  if (rc == 0)
    *psize = scm_to_int64 (ret);
  return rc;
}

int
mu_scm_port_stream_create (mu_stream_t *pstream, SCM port)
{
  char *mode;
  int flags = MU_STREAM_SEEK|_MU_STR_OPEN;
  struct port_stream *pstr;

  mode = scm_to_locale_string (scm_port_mode (port));
  if (strchr (mode, 'r'))
    flags |= MU_STREAM_READ;
  if (strchr (mode, 'w'))
    flags |= MU_STREAM_WRITE;
  free (mode);
  
  pstr = (struct port_stream *) _mu_stream_create (sizeof (*pstr), flags);
  if (!pstr)
    return ENOMEM;
  pstr->stream.read = port_stream_read;
  pstr->stream.write = port_stream_write;
  pstr->stream.seek = port_stream_seek;
  pstr->stream.size = port_stream_size;
  pstr->stream.truncate = port_stream_truncate;
  pstr->stream.done = port_stream_done;
  pstr->port = port;
  scm_gc_protect_object (port);
  *pstream = (mu_stream_t) pstr;
  return 0;
}



