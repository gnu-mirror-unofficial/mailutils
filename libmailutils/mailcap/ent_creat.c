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
#include <mailutils/assoc.h>
#include <mailutils/sys/mailcap.h>

int
mu_mailcap_entry_create (mu_mailcap_entry_t *ret_ent,
			 char *type, char *command)
{
  int rc;
  mu_mailcap_entry_t ent = calloc (1, sizeof (*ent));

  if (!ent)
    return ENOMEM;
  if (!ret_ent)
    return MU_ERR_OUT_PTR_NULL;

  ent->type = strdup (type);
  ent->command = strdup (command);
  if (!ent->type || !ent->command)
    {
      mu_mailcap_entry_destroy (&ent);
      return ENOMEM;
    }
  rc = mu_assoc_create (&ent->fields, MU_ASSOC_ICASE);
  if (rc)
    mu_mailcap_entry_destroy (&ent);
  else
    {
      mu_assoc_set_destroy_item (ent->fields, mu_mailcap_entry_field_deallocate);
      *ret_ent = ent;
    }
  return rc;
}
