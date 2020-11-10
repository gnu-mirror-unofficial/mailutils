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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <mailutils/errno.h>
#include <mailutils/cctype.h>
#include <mailutils/cstr.h>

static inline int
consume_number (char const **verstr, int *pn)
{
  unsigned long n;
  char *p;

  if (*verstr)
    {
      n = strtoul (*verstr, &p, 10);
      if ((n == 0 && errno == ERANGE) || n > INT_MAX)
	{
	  return ERANGE;
	}
      *verstr = p;
    }
  else
    n = 0;
  *pn = n;
  return 0;
}

int
mu_version_string_parse (char const *verstr, int version[3], char **endp)
{
  int rc;
  int v[3];
  
  if ((rc = consume_number (&verstr, &v[0])) == 0)
    {
      if (*verstr)
	{
	  if (*verstr == '.')
	    ++verstr;
	  else
	    rc = MU_ERR_PARSE;
	}
    }

  if (rc == 0 && (rc = consume_number (&verstr, &v[1])) == 0)
    {
      if (*verstr == '.')
	{
	  verstr++;
	  rc = consume_number (&verstr, &v[2]);
	}
      else if (!*verstr || mu_ispunct (*verstr))
	{
	  v[2] = 0;
	}
      else
	rc = MU_ERR_PARSE;
    }

  if (endp)
    *endp = (char*)verstr;

  if (rc == 0)
    {
      if (*verstr)
	{
	  if (!endp || !mu_ispunct (*verstr))
	    return MU_ERR_PARSE;
	}
      memcpy (version, v, sizeof(v));
    }
  
  return rc;
}

static inline int
numsufcmp (char const *as, char const *bs, int *res)
{
  int an, bn;

  if (*as == 0)
    an = 0;
  else
    {
      as++;
      if (consume_number (&as, &an) || *as)
	return -1;
    }
  if (*bs == 0)
    bn = 0;
  else {
    bs++;
    if (consume_number (&bs, &bn) || *bs)
      return -1;
  }
  *res = an - bn;
  return 0;
}
  
int
mu_version_string_cmp (char const *a, char const *b, int ignoresuf, int *res)
{
  int va[3], vb[3];
  char *sa, *sb;
  int rc;

  if ((rc = mu_version_string_parse (a, va, &sa)) != 0 ||
      (rc = mu_version_string_parse (b, vb, &sb)) != 0)
    return rc;

  if (va[0] > vb[0])
    *res = 1;
  else if (va[0] < vb[0])
    *res = -1;
  else if (va[1] > vb[1])
    *res = 1;
  else if (va[1] < vb[1])
    *res = -1;
  else if (va[2] > vb[2])
    *res = 1;
  else if (va[2] < vb[2])
    *res = -1;
  else if (!ignoresuf)
    {
      if (numsufcmp (sa, sb, res))
	*res = strcmp (sa, sb);
    }
  else
    *res = 0;
  
  return 0;
}
