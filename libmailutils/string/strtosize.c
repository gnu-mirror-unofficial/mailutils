/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2016-2021 Free Software Foundation, Inc.

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
#include <mailutils/errno.h>
#include <mailutils/cctype.h>
#include <mailutils/cstr.h>

#ifndef SIZE_MAX
# define SIZE_MAX (~((size_t)0))
#endif

/*
 * Converts the initial part of the string in STR to a size_t value.
 * A valid input consists of a sequence of decimal digits, and optional
 * size suffix: K, M, or G.  Any amount of whitespace is allowed at the
 * beginning of STR and between the last digit and size suffix.
 *
 * On success, the result is stored in RET_VAL and 0 is returned.
 * On error, RET_VAL remains untouched and one of the following error
 * codes is returned:
 *   EINVAL       - STR is NULL.
 *   MU_ERR_OUT_PTR_NULL
 *                RET_VAL is NULL.
 *   ERANGE       - Result is out of allowed range for size_t.
 *   MU_ERR_PARSE - (0) Unexpected character encountered or
 *                  (1) STR is empty or
 *                  (2) STR contains only whitespace characters or
 *                  (3) ENDP is NULL and the portion of STR that remains
 *                  after conversion contains characters other than
 *                  \0 and whitespace,
 *
 * If ENDP is not NULL, a pointer to the character in STR at which
 * the conversion has stopped is stored in *ENDP.
 */
int
mu_strtosize (char const *str, char **endp, size_t *ret_val)
{
  size_t n = 0;
  int rc = 0;
  char const *start, *s;
  
  if (!str)
    return EINVAL;
  if (!ret_val)
    return MU_ERR_OUT_PTR_NULL;

  s = start = mu_str_skip_class (str, MU_CTYPE_SPACE);
  
  while (*s && mu_isdigit (*s))
    {
      size_t x = n * 10 + *s - '0';
      if (x < n)
	{
	  rc = ERANGE;
	  goto end;
	}
      n = x;
      s++;
    }

  if (s == start)
    {
      rc = MU_ERR_PARSE;
      goto end;
    }

  str = s;
  s = mu_str_skip_class (s, MU_CTYPE_SPACE);
  
  switch (*s++)
    {
    case 'g':
    case 'G':
      if (SIZE_MAX / n < 1024)
	{
	  rc = ERANGE;
	  break;
	}
      n <<= 10;
    case 'm':
    case 'M':
      if (SIZE_MAX / n < 1024)
	{
	  rc = ERANGE;
	  break;
	}
      n <<= 10;
    case 'k':
    case 'K':
      if (SIZE_MAX / n < 1024)
	{
	  rc = ERANGE;
	  break;
	}
      n <<= 10;
      break;

    case 0:
      s = str;
      break;
	
    default:
      s = str;
      rc = MU_ERR_PARSE;
    }
 end:
  if (endp)
    *endp = (char*) s;
  else if (rc == 0 && *mu_str_skip_class (s, MU_CTYPE_SPACE))
    rc = MU_ERR_PARSE;
    
  if (rc == 0)
    *ret_val = n;
  return rc;
}
