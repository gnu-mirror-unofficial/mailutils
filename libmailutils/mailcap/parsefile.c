/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2021 Free Software Foundation, Inc.

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

#include <config.h>
#include <stdlib.h>
#include <mailutils/sys/mailcap.h>
#include <mailutils/stream.h>
#include <mailutils/locus.h>

int
mu_mailcap_parse_file (mu_mailcap_t mailcap, char const *file_name)
{
  int rc;
  mu_stream_t str;
  struct mu_locus_point point;

  rc = mu_file_stream_create (&str, file_name, MU_STREAM_READ);
  if (rc)
    return rc;
  mu_locus_point_init (&point);
  mu_locus_point_set_file (&point, file_name);
  point.mu_line = 1;
  rc = mu_mailcap_parse (mailcap, str, &point);
  mu_locus_point_deinit (&point);
  mu_stream_destroy (&str);
  return rc;
}
