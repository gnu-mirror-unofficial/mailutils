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

void
mu_mailcap_entry_field_deallocate (void *ptr)
{
  struct mailcap_field *flg = ptr;
  if (flg->type == fld_string)
    free (flg->strval);
  free (flg);
}

int
mu_mailcap_entry_set_bool (mu_mailcap_entry_t ent, char const *name)
{
  struct mailcap_field *fp, **fpp;
  int rc;

  if (!ent || !name)
    return EINVAL;

  rc = mu_assoc_install_ref (ent->fields, name, (void *)&fpp);
  switch (rc)
    {
    case 0:
      fp = malloc (sizeof (*fp));
      if (!fp)
	return ENOMEM;
      fp->type = fld_bool;
      *fpp = fp;
      break;

    case MU_ERR_EXISTS:
      fp = *fpp;
      if (fp->type == fld_string)
	{
	  free (fp->strval);
	  fp->strval = NULL;
	  fp->type = fld_bool;
	}
      break;
    }

  return rc;
}

int
mu_mailcap_entry_set_string (mu_mailcap_entry_t ent, char const *name,
			     char const *value)
{
  struct mailcap_field *fp, **fpp;
  int rc;
  char *copy;

  if (!ent || !name || !value)
    return EINVAL;

  rc = mu_assoc_install_ref (ent->fields, name, (void *)&fpp);
  switch (rc)
    {
    case 0:
      fp = malloc (sizeof (*fp));
      if (!fp)
	return ENOMEM;
      fp->strval = strdup (value);
      if (!fp->strval)
	{
	  free (fp);
	  return ENOMEM;
	}
      fp->type = fld_string;
      *fpp = fp;
      break;

    case MU_ERR_EXISTS:
      copy = strdup (value);
      if (!copy)
	return ENOMEM;

      fp = *fpp;
      if (fp->type == fld_string)
	free (fp->strval);
      else
	fp->type = fld_string;
      fp->strval = copy;
      break;
    }

  return rc;
}

int
mu_mailcap_entry_field_unset (mu_mailcap_entry_t ent, char const *name)
{
  if (!ent || !name)
    return EINVAL;
  return mu_assoc_remove (ent->fields, name);
}

int
mu_mailcap_entry_fields_count (mu_mailcap_entry_t ent, size_t *pcount)
{
  if (!ent)
    return EINVAL;
  return mu_assoc_count (ent->fields, pcount);
}

struct fields_iter_closure
{
  int (*action) (char const *, char const *, void *);
  void *data;
};

static int
fields_iter_action (char const *name, void *item, void *data)
{
  struct mailcap_field *flg = item;
  struct fields_iter_closure *clos = data;
  return clos->action (name, flg->type == fld_string ? flg->strval : NULL,
		       clos->data);
}

int
mu_mailcap_entry_fields_foreach (mu_mailcap_entry_t ent,
			      int (*action) (char const *, char const *, void *),
			      void *data)
{
  struct fields_iter_closure clos;
  if (!ent || !action)
    return EINVAL;
  clos.action = action;
  clos.data = data;
  return mu_assoc_foreach (ent->fields, fields_iter_action, &clos);
}

int
mu_mailcap_entry_fields_get_iterator (mu_mailcap_entry_t ent, mu_iterator_t *pitr)
{
  if (!ent)
    return EINVAL;
  if (!pitr)
    return MU_ERR_OUT_PTR_NULL;
  return mu_assoc_get_iterator (ent->fields, pitr);
}
