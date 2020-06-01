/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2020 Free Software Foundation, Inc.

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
#include <mailutils/types.h>
#include <mailutils/alloc.h>
#include <mailutils/cstr.h>
#include "muaux.h"

void
unistr_downcase (char const *input, char **output)
{
  /* nothing */
  *output = mu_strdup (input);
}

int
unistr_is_substring (char const *haystack, char const *needle)
{
  return mu_c_strcasestr (haystack, needle) != NULL;
}
