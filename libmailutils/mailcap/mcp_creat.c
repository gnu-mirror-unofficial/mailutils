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
#include <mailutils/list.h>
#include <mailutils/sys/mailcap.h>
#include <mailutils/cctype.h>

static int
type_comp (const void *item, const void *data)
{
  struct _mu_mailcap_entry const *ent = item;
  return mu_mailcap_string_match (ent->type, 0, data);
}

int
mu_mailcap_create (mu_mailcap_t *pmailcap)
{
  mu_mailcap_t mailcap;
  int rc;

  if (pmailcap == NULL)
    return MU_ERR_OUT_PTR_NULL;

  mailcap = calloc (1, sizeof (*mailcap));
  if (!mailcap)
    return ENOMEM;

  mailcap->flags = MU_MAILCAP_FLAG_DEFAULT;
  rc = mu_list_create (&mailcap->elist);
  if (rc)
    {
      free (mailcap);
      return rc;
    }
  mu_list_set_destroy_item (mailcap->elist, mu_mailcap_entry_destroy_item);
  mu_list_set_comparator (mailcap->elist, type_comp);

  *pmailcap = mailcap;
  return 0;
}
