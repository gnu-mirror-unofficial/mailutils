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
#include <stdlib.h>
#include <mailutils/assoc.h>
#include <mailutils/sys/mailcap.h>

int
mu_mailcap_entry_sget_field (mu_mailcap_entry_t ent, char const *name,
			    char const **pval)
{
  struct mailcap_field *fp;

  if (!ent || !name)
    return EINVAL;
  if (!pval)
    return MU_ERR_OUT_PTR_NULL;
  fp = mu_assoc_get (ent->fields, name);
  if (!fp)
    return MU_ERR_NOENT;
  if (pval)
    {
      if (fp->type == fld_string)
	*pval = fp->strval;
      else
	*pval = NULL;
    }
  return 0;
}

int
mu_mailcap_entry_aget_field (mu_mailcap_entry_t ent, char const *name,
			    char **pval)
{
  int rc;
  char const *val;

  rc = mu_mailcap_entry_sget_field (ent, name, &val);
  if (rc == 0)
    {
      if (val)
	{
	  char *copy = strdup (val);
	  if (!copy)
	    return ENOMEM;
	  *pval = copy;
	}
      else
	*pval = NULL;
    }
  return rc;
}

int
mu_mailcap_entry_get_field (mu_mailcap_entry_t ent,
			   char const *name,
			   char *buffer, size_t buflen,
			   size_t *pn)
{
  int rc;
  char const *val;
  size_t len;

  if (!ent)
    return EINVAL;

  rc = mu_mailcap_entry_sget_field (ent, name, &val);
  if (rc)
    return rc;

  if (val)
    {
      len = strlen (val);

      if (buffer)
	{
	  if (len > buflen)
	    len = buflen;
	  memcpy (buffer, val, len);
	  buffer[len] = 0;
	}
    }
  else
    {
      len = 0;
      if (buffer)
	buffer[0] = 0;
    }

  if (pn)
    *pn = len;
  return 0;
}
