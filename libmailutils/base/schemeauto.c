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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <mailutils/url.h>

/* Returns true if SCHEME represents a local (autodetect) mail folder.  */
int
mu_scheme_autodetect_p (mu_url_t url)
{
  if (mu_url_is_scheme (url, "file"))
    {
      mu_url_expand_path (url);
      return 1;
    }
  return 0;
}

static int accuracy = MU_AUTODETECT_ACCURACY_AUTO;

void
mu_set_autodetect_accuracy (int v)
{
  accuracy = v;
}

int
mu_autodetect_accuracy (void)
{
  if (accuracy == MU_AUTODETECT_ACCURACY_AUTO)
    {
      char *p = getenv ("MU_AUTODETECT_ACCURACY");
      if (!p || strcmp (p, "default") == 0)
	accuracy = MU_AUTODETECT_ACCURACY_DEFAULT;
      else if (strcmp (p, "fast") == 0)
	accuracy = MU_AUTODETECT_ACCURACY_FAST;
      else if (strcmp (p, "max") == 0)
	accuracy = MU_AUTODETECT_ACCURACY_MAX;
      else
	accuracy = atoi (p);
    }
  return accuracy;
}
