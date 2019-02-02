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
#include <mailutils/list.h>
#include <mailutils/sys/mailcap.h>

int
mu_mailcap_find_entry (mu_mailcap_t mailcap, char const *type,
		       mu_mailcap_entry_t *entry)
{
  if (!mailcap || !type)
    return EINVAL;
  if (!entry)
    return MU_ERR_OUT_PTR_NULL;
  return mu_list_locate (mailcap->elist, (void *) type, (void **) entry);
}
