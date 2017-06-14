/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2017 Free Software Foundation, Inc.

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

#include <stdlib.h>
#include <errno.h>
#include <mailutils/types.h>
#include <mailutils/locus.h>
#include <mailutils/error.h>

int
mu_locus_point_set_file (struct mu_locus_point *pt, const char *filename)
{
  int rc;
  char const *ref;

  rc = mu_ident_ref (filename, &ref);
  if (rc)
    return rc;
  mu_ident_deref (pt->mu_file);
  pt->mu_file = ref;
  return 0;
}

int
mu_locus_point_init (struct mu_locus_point *pt, const char *filename)
{
  pt->mu_line = 0;
  pt->mu_col = 0;
  return mu_locus_point_set_file (pt, filename);
}

void
mu_locus_point_deinit (struct mu_locus_point *pt)
{
  mu_ident_deref (pt->mu_file);
  memset (pt, 0, sizeof *pt);
}

int
mu_locus_point_copy (struct mu_locus_point *dest,
		     struct mu_locus_point const *src)
{
  dest->mu_col = src->mu_col;
  dest->mu_line = src->mu_line;
  return mu_locus_point_set_file (dest, src->mu_file);
}

int
mu_locus_range_copy (struct mu_locus_range *dest,
		     struct mu_locus_range const *src)
{
  int rc;
  struct mu_locus_range tmp = MU_LOCUS_RANGE_INITIALIZER;
  
  rc = mu_locus_point_copy (&tmp.beg, &src->beg);
  if (rc == 0)
    {
      rc = mu_locus_point_copy (&tmp.end, &src->end);
      if (rc)
	mu_locus_point_deinit (&tmp.beg);
      else
	*dest = tmp;
    }
  return rc;
}

void
mu_locus_range_deinit (struct mu_locus_range *lr)
{
  mu_locus_point_deinit (&lr->beg);
  mu_locus_point_deinit (&lr->end);
}
