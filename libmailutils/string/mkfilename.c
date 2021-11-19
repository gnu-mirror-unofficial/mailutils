/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2021 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library.  If not,
   see <http://www.gnu.org/licenses/>. */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mailutils/alloc.h>
#include <mailutils/util.h>

/*
 * Given directory name DIR, file name FILE and optional suffix SUF,
 * return full pathname composed from these three.  In the resulting
 * string, DIR and FILE are separated by exactly one '/', unless DIR
 * is one or more '/' characters, in which case the two strings are
 * simply concatenated.  If SUF is supplied, it is appended to the
 * resulting string without additional separators.
 *
 * Corner cases:
 *   all three arguments are NULL or empty
 *     Return NULL and set errno to EINVAL.
 *   dir is NULL
 *     Return FILE and SUF concatenated.
 *   dir is a sequence of one or more '/'
 *     Return concatenation of arguments.
 *   file is NULL, suf is not NULL
 *     Same as mu_make_file_name_suf(dir, suf, NULL);
 *   file is NULL, suf is NULL
 *     Return allocated copy of DIR.
 */

#define DIRSEP '/'

char *
mu_make_file_name_suf (const char *dir, const char *file, const char *suf)
{
  char *tmp;
  size_t dirlen, suflen, fillen;
  size_t len;
  int sep = 0;
  
  dirlen = dir ? strlen (dir) : 0;
  fillen = file ? strlen (file) : 0;
  suflen = suf ? strlen (suf) : 0;

  len = suflen + fillen;
  if (dirlen == 0)
    {
      if (len == 0)
	{
	  errno = EINVAL;
	  return NULL;
	}
    }
  else
    {
      int i = 0;

      if (len)
	sep = DIRSEP;
      
      if (dir[0] == DIRSEP)
	{
	  for (i = 0; dir[i+1] == DIRSEP; i++);
	  sep = DIRSEP;
	}
      
      while (dirlen > i && dir[dirlen-1] == DIRSEP)
	dirlen--;
    }
  len += dirlen;
  if (sep)
    len++;

  tmp = malloc (len + 1);
  if (tmp)
    {
      if (dir)
	memcpy (tmp, dir, dirlen);
      if (sep)
	tmp[dirlen++] = sep;
      if (fillen)
	memcpy (tmp + dirlen, file, fillen);
      if (suflen)
	memcpy (tmp + dirlen + fillen, suf, suflen);
      tmp[len] = 0;
    }
  return tmp;
}
