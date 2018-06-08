/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2001, 2006-2007, 2009-2012, 2014-2018 Free
   Software Foundation, Inc.

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
#include <mailutils/io.h>

#ifndef HAVE_SCM_T_OFF
typedef off_t scm_t_off;
#endif

struct mu_port
{
  mu_stream_t stream;      /* Associated stream */
  SCM msg;                 /* Message the port belongs to */		
};

#define MU_PORT(x) ((struct mu_port *) SCM_STREAM (x))

static scm_t_port_type *scm_mu_port_type;

SCM
mu_port_make_from_stream (SCM msg, mu_stream_t stream, long mode)
{
  struct mu_port *mp;

  mp = scm_gc_typed_calloc (struct mu_port);
  mp->msg = msg;
  mp->stream = stream;
  return scm_c_make_port (scm_mu_port_type, mode | SCM_BUF0, (scm_t_bits) mp);
}

static void
mu_port_close (SCM port)
{
  struct mu_port *mp = MU_PORT (port);
  mu_stream_destroy (&mp->stream);
}

static size_t
mu_port_read (SCM port, SCM dst, size_t start, size_t count)
{
  struct mu_port *mp = MU_PORT (port);
  int status;
  size_t nread;
  
  status = mu_stream_read (mp->stream,
			   SCM_BYTEVECTOR_CONTENTS (dst) + start,
			   count,
			   &nread);
  if (status)
    mu_scm_error ("mu_port_read", status,
		  "Error reading from stream", SCM_BOOL_F);
  return nread;
}
  
static size_t
mu_port_write (SCM port, SCM src, size_t start, size_t count)
{
  struct mu_port *mp = MU_PORT (port);
  int status;
  size_t nwrite;

  status = mu_stream_write (mp->stream,
			    SCM_BYTEVECTOR_CONTENTS (src) + start, count,
			    &nwrite);
  if (status)
    mu_scm_error ("mu_port_read", status,
		  "Error reading from stream", SCM_BOOL_F);
  return nwrite;
}

static scm_t_off
mu_port_seek (SCM port, scm_t_off offset, int whence)
{
  struct mu_port *mp = MU_PORT (port);
  mu_off_t pos;
  int status;

  status = mu_stream_seek (mp->stream, offset, whence, &pos);
  if (status)
    pos = -1;
  return (scm_t_off) pos;
}

static void
mu_port_truncate (SCM port, mu_off_t length)
{
  struct mu_port *mp = MU_PORT (port);
  int status;
  status = mu_stream_truncate (mp->stream, length);
  if (status)
    mu_scm_error ("mu_port_truncate", status,
		  "Error truncating stream", SCM_BOOL_F);
}
  
static int
mu_port_print (SCM exp, SCM port, scm_print_state *pstate)
{
  struct mu_port *mp = MU_PORT (exp);
  mu_off_t size = 0;
  
  scm_puts ("#<", port);
  scm_print_port_mode (exp, port);
  scm_puts ("mu-port", port);
  if (mu_stream_size (mp->stream, &size) == 0)
    {
      char *buf;
      if (mu_asprintf (&buf, " %5lu", (unsigned long) size) == 0)
	{
	  scm_puts (buf, port);
	  scm_puts (" chars", port);
	  free (buf);
	}
    }
  scm_putc ('>', port);
  return 1;
}
     
void
mu_scm_port_init (void)
{
    scm_mu_port_type = scm_make_port_type ("mu-port",
					   mu_port_read, mu_port_write);
    scm_set_port_print (scm_mu_port_type, mu_port_print);
    scm_set_port_close (scm_mu_port_type, mu_port_close);
    scm_set_port_needs_close_on_gc (scm_mu_port_type, 1);
    scm_set_port_seek (scm_mu_port_type, mu_port_seek);
    scm_set_port_truncate (scm_mu_port_type, mu_port_truncate);
}
