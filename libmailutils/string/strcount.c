/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2016 Free Software Foundation, Inc.

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
#include <limits.h>
#include <mailutils/util.h>

/* Return the number of occurrences of the ASCII character CHR in the
   UTF-8 string STR. */
size_t
mu_str_count (char const *str, int chr)
{
  unsigned char c;
  size_t count = 0;
  int consume = 0;
  
  if (!str || chr < 0 || chr > UCHAR_MAX)
    return 0;
  
  while ((c = *str++) != 0)
    {
      if (consume)
	consume--;
      else if (c < 0xc0)
	{
	  if (c == chr)
	    count++;
	}
      else if (c & 0xc0)
	consume = 1;
      else if (c & 0xe0)
	consume = 2;
      else if (c & 0xf0)
	consume = 3;      
    }
  return count;
}
