/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2021 Free Software Foundation, Inc.

   GNU Mailutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Mailutils is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. */

#include <config.h>
#include <stdlib.h>
#include <unicase.h>
#include <unistr.h>
#include <string.h>
#include <mailutils/types.h>
#include "muaux.h"

void
unistr_downcase (char const *input, char **output)
{
  size_t len;
  uint8_t *result = u8_tolower ((const uint8_t *)input, strlen (input)+1,
				NULL, NULL, NULL, &len);
  *output = (char*)result;
}

int
unistr_is_substring (char const *haystack, char const *needle)
{
  return u8_strstr ((const uint8_t*) haystack, (const uint8_t*) needle) != NULL;
}

int
unistr_is_substring_dn (char const *haystack, char const *needle)
{
  char *lc;
  int result;
  
  unistr_downcase (haystack, &lc);
  result = u8_strstr ((const uint8_t*) lc, (const uint8_t*) needle) != NULL;
  free (lc);
  return result;
}

