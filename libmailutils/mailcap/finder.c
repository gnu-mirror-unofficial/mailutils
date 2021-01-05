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
#include <unistd.h>
#include <mailutils/sys/mailcap.h>
#include <mailutils/iterator.h>

static int
finder_init (mu_mailcap_finder_t finder,
	     int flags,
	     struct mu_mailcap_selector_closure *sc,
	     struct mu_mailcap_error_closure *ec,
	     char **file_names);

int
mu_mailcap_finder_create (mu_mailcap_finder_t *pfinder, int flags,
			  struct mu_mailcap_selector_closure *sc,
			  struct mu_mailcap_error_closure *ec,
			  char **file_names)
{
  int rc;
  mu_mailcap_finder_t finder;

  if (!pfinder)
    return MU_ERR_OUT_PTR_NULL;
  if (!file_names)
    return EINVAL;
  finder = calloc (1, sizeof *finder);
  if (!finder)
    return ENOMEM;
  rc = finder_init (finder, flags, sc, ec, file_names);
  if (rc)
    mu_mailcap_finder_destroy (&finder);
  else
    *pfinder = finder;
  return rc;
}

static int
finder_init (mu_mailcap_finder_t finder,
	     int flags,
	     struct mu_mailcap_selector_closure *sc,
	     struct mu_mailcap_error_closure *ec,
	     char **file_names)
{
  int rc;
  size_t i;

  rc = mu_mailcap_create (&finder->mcp);
  if (rc)
    return rc;
  mu_mailcap_set_flags (finder->mcp, flags);
  if (sc)
    {
      rc = mu_mailcap_set_selector (finder->mcp, sc);
      if (rc)
	return rc;
    }
  if (ec)
    {
      rc = mu_mailcap_set_error (finder->mcp, ec);
      if (rc)
	return rc;
    }

  for (i = 0; file_names[i]; i++)
    {
      if (access (file_names[i], F_OK))
	continue;
      mu_mailcap_parse_file (finder->mcp, file_names[i]);
    }

  return 0;
}

void
mu_mailcap_finder_destroy (mu_mailcap_finder_t *pfinder)
{
  if (pfinder && *pfinder)
    {
      mu_mailcap_finder_t finder = *pfinder;
      mu_iterator_destroy (&finder->itr);
      mu_mailcap_destroy (&finder->mcp);
      free (finder);
      *pfinder = NULL;
    }
}

int
mu_mailcap_finder_next_match (mu_mailcap_finder_t finder,
			      mu_mailcap_entry_t *ret_entry)
{
  int rc;

  if (!finder)
    return EINVAL;
  if (!ret_entry)
    return MU_ERR_OUT_PTR_NULL;
  if (!finder->itr)
    {
      rc = mu_mailcap_get_iterator (finder->mcp, &finder->itr);
      if (rc == 0)
	rc = mu_iterator_first (finder->itr);
    }
  else
    rc = mu_iterator_next (finder->itr);

  if (rc)
    return rc;

  if (mu_iterator_is_done (finder->itr))
    return MU_ERR_NOENT;

  return mu_iterator_current (finder->itr, (void**) ret_entry);
}
