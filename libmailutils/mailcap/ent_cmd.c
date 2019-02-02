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

int
mu_mailcap_entry_sget_command (mu_mailcap_entry_t ent, char const **pcommand)
{
  if (!ent)
    return EINVAL;
  if (!pcommand)
    return MU_ERR_OUT_PTR_NULL;
  *pcommand = ent->command;
  return 0;
}

int
mu_mailcap_entry_aget_command (mu_mailcap_entry_t ent, char **pcommand)
{
  if (!ent)
    return EINVAL;
  if (!pcommand)
    return MU_ERR_OUT_PTR_NULL;
  if (!(*pcommand = strdup (ent->command)))
    return ENOMEM;
  return 0;
}

int
mu_mailcap_entry_get_command (mu_mailcap_entry_t ent,
			      char *buffer, size_t buflen,
			      size_t *pn)
{
  size_t len;

  if (!ent)
    return EINVAL;
  len = strlen (ent->command);

  if (buffer)
    {
      if (len > buflen)
	len = buflen;
      memcpy (buffer, ent->command, len);
      buffer[len] = 0;
    }
  if (pn)
    *pn = len;
  return 0;
}
