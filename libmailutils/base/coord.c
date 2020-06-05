/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2020 Free Software Foundation, Inc.

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

/*
 * Functions for dealing with message part coordinates.
 */

#include <config.h>
#include <stdlib.h>
#include <errno.h>
#include <mailutils/message.h>

int
mu_coord_alloc (mu_coord_t *ptr, size_t n)
{
  mu_coord_t p = calloc (n + 1, sizeof (p[0]));
  if (!p)
    return errno;
  p[0] = n;
  *ptr = p;
  return 0;
}

int
mu_coord_realloc (mu_coord_t *ptr, size_t n)
{
  if (!ptr)
    return EINVAL;
  if (!*ptr)
    return mu_coord_alloc (ptr, n);
  else
    {
      size_t i = mu_coord_length (*ptr);
      if (i != n)
	{
	  mu_coord_t nc = realloc (*ptr, (n + 1) * sizeof (nc[0]));
	  if (nc == NULL)
	    return ENOMEM;
	  for (; i <= n; i++)
	    nc[i] = 0;
	  *ptr = nc;
	}
    }
  return 0;
}      

int
mu_coord_dup (mu_coord_t orig, mu_coord_t *copy)
{
  size_t i, n = mu_coord_length (orig);
  int rc = mu_coord_alloc (copy, n);
  if (rc)
    return rc;
  for (i = 1; i <= n; i++)
    (*copy)[i] = orig[i];
  return 0;
}

static void
revstr (char *s, char *e)
{
  while (s < e)
    {
      char t = *s;
      *s++ = *--e;
      *e = t;
    }
}

char *
mu_coord_part_string (mu_coord_t c, size_t dim)
{
  size_t len = 0;
  size_t i;
  char *result, *p;
  
  for (i = 1; i <= dim; i++)
    {
      size_t n = c[i];
      do
	len++;
      while (n /= 10);
      len++;
    }

  result = malloc (len);
  if (!result)
    return NULL;
  p = result;
  
  for (i = 1; i <= dim; i++)
    {
      char *s;
      size_t n = c[i];
      if (i > 1)
	*p++ = '.';
      s = p;
      do
	{
	  unsigned x = n % 10;
	  *p++ = x + '0';
	}
      while (n /= 10);
      revstr(s, p);
    }
  *p = 0;

  return result;
}
