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

int
mu_mailcap_entry_sget_type (mu_mailcap_entry_t ent, char const **ptype)
{
  if (!ent)
    return EINVAL;
  if (!ptype)
    return MU_ERR_OUT_PTR_NULL;
  *ptype = ent->type;
  return 0;
}

int
mu_mailcap_entry_aget_type (mu_mailcap_entry_t ent, char **ptype)
{
  if (!ent)
    return EINVAL;
  if (!ptype)
    return MU_ERR_OUT_PTR_NULL;
  if (!(*ptype = strdup (ent->type)))
    return ENOMEM;
  return 0;
}

int
mu_mailcap_entry_get_type (mu_mailcap_entry_t ent,
			   char *buffer, size_t buflen,
			   size_t *pn)
{
  size_t len;

  if (!ent)
    return EINVAL;
  len = strlen (ent->type);

  if (buffer)
    {
      if (len > buflen)
	len = buflen;
      memcpy (buffer, ent->type, len);
      buffer[len] = 0;
    }
  if (pn)
    *pn = len;
  return 0;
}
