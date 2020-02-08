/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2020 Free Software Foundation, Inc.

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
#include <mailutils/mailcap.h>
#include <mailutils/diag.h>

static void
mailcap_error (void *closure, struct mu_locus_range const *loc, char const *msg)
{
  mu_diag_at_locus_range (MU_DIAG_ERR, loc, "%s", msg);
}

struct mu_mailcap_error_closure mu_mailcap_default_error_closure = {
  mailcap_error,
  NULL,
  NULL
};
