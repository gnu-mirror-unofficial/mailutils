/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2019 Free Software Foundation, Inc.

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
#include <mailutils/errno.h>

int
mu_mailcap_entry_get_locus (mu_mailcap_entry_t ent, struct mu_locus_range *lrp)
{
  if (!ent)
    return EINVAL;
  if (!lrp)
    return MU_ERR_OUT_PTR_NULL;
  if (!ent->lrp)
    return MU_ERR_NOENT;
  return mu_locus_range_copy (lrp, ent->lrp);
}
