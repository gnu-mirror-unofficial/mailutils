/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2021 Free Software Foundation, Inc.

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

struct _mu_debug_port
{
  mu_stream_t stream;
  int level;
};

static scm_t_port_type *scm_mu_debug_port_type;

SCM
mu_scm_make_debug_port (int level)
{
  struct _mu_debug_port *dp;
  mu_stream_t str;
  
  if (mu_dbgstream_create (&str, level))
    return SCM_BOOL_F;

  dp = scm_gc_typed_calloc (struct _mu_debug_port);
  dp->level = level;
  dp->stream = str;
  return scm_c_make_port (scm_mu_debug_port_type, SCM_BUF0|SCM_WRTNG,
			  (scm_t_bits) dp);
}

#define MU_DEBUG_PORT(x) ((struct _mu_debug_port *) SCM_STREAM (x))

static void
_mu_debug_port_close (SCM port)
{
  struct _mu_debug_port *dp = MU_DEBUG_PORT (port);

  if (dp && dp->stream)
    {
      mu_stream_flush (dp->stream);
      mu_stream_destroy (&dp->stream);
    }
}

static size_t
_mu_debug_port_write (SCM port, SCM src, size_t start, size_t count)
{
  struct _mu_debug_port *dp = MU_DEBUG_PORT (port);

  mu_stream_write (dp->stream, SCM_BYTEVECTOR_CONTENTS (src) + start, count,
		   NULL);
  return count;
}

static int
_mu_debug_port_print (SCM exp, SCM port, scm_print_state * pstate)
{
  scm_puts ("#<Mailutis debug port>", port);
  return 1;
}

void
mu_scm_debug_port_init (void)
{
  scm_mu_debug_port_type = scm_make_port_type ("mu-debug-port",
					       NULL,
					       _mu_debug_port_write);
  scm_set_port_print (scm_mu_debug_port_type, _mu_debug_port_print);
  scm_set_port_close (scm_mu_debug_port_type, _mu_debug_port_close);
  scm_set_port_needs_close_on_gc (scm_mu_debug_port_type, 1);
}
