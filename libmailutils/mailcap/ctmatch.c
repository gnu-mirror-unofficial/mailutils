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
#include <mailutils/cctype.h>
#include <mailutils/util.h>

enum
  {
    MATCH = 0,
    NO_MATCH = 1
  };

/* Return MATCH (0) if content-type string TYPESTR matches one of
   patterns from PATLIST, a list of patterns delimited with DELIM.
   Whitespace is allowed at either side of patterns in PATLIST.
   Empty patterns are ignored.

   A "pattern" is a type/subtype specification. Matching is case-insensitive.
   If the subtype is '*', it matches any subtype from TYPESTR.
*/
int
mu_mailcap_string_match (char const *patlist, int delim, char const *typestr)
{
  char const *type;
  
  if (!patlist)
    return NO_MATCH;

  while (*patlist)
    {
      type = typestr;
      
      /* Skip whitespace and delimiters */
      while (*patlist && (*patlist == delim || mu_isspace (*patlist)))
	patlist++;

      /* Match type */
      do
	{
	  if (!*patlist || !*type
	      || mu_tolower (*patlist++) != mu_tolower (*type++))
	    goto retry;
	  if (*patlist == delim)
	    goto retry;
	}
      while (*patlist != '/');

      if (*type != '/')
	goto retry;
      patlist++;
      type++;

      if (*patlist == '*')
	return MATCH;

      /* Match subtype */
      while (*patlist && *patlist != delim && *type
	     && mu_tolower (*patlist) == mu_tolower (*type))
	{
	  patlist++;
	  type++;
	}

      /* Skip optional whitespace */
      while (*patlist && mu_isspace (*patlist))
	patlist++;

      if (*patlist == 0 || *patlist == delim)
	{
	  if (*type == 0)
	    return MATCH;
	}
    retry:
      /* Skip to the next pattern */
      do
	{
	  if (*patlist == 0)
	    return NO_MATCH;
	  patlist++;
	}
      while (*patlist != delim);
    }
  return NO_MATCH;
}

/* Return 0 if parsed content-type CT matches PATTERN. */
int
mu_mailcap_content_type_match (char const *patlist, int delim,
			       mu_content_type_t ct)
{
  char const *type;

  if (!patlist)
    return NO_MATCH;
  
  while (*patlist)
    {
      type = ct->type;
      
      /* Skip whitespace and delimiters */
      while (*patlist && (*patlist == delim || mu_isspace (*patlist)))
	patlist++;

      /* Match type */
      do
	{
	  if (!*patlist || !*type
	      || mu_tolower (*patlist++) != mu_tolower (*type++))
	    goto retry;
	  if (*patlist == delim)
	    goto retry;
	}
      while (*patlist != '/');

      if (*type != 0)
	goto retry;
      patlist++;

      if (*patlist == '*')
	return MATCH;

      /* Match subtype */
      type = ct->subtype;
      while (*patlist && *patlist != delim && *type
	     && mu_tolower (*patlist) == mu_tolower (*type))
	{
	  patlist++;
	  type++;
	}

      /* Skip optional whitespace */
      while (*patlist && mu_isspace (*patlist))
	patlist++;
      
      if (*patlist == 0 || *patlist == delim)
	{
	  if (*type == 0)
	    return MATCH;
	}
    retry:
      /* Skip to next pattern */
      do
	{
	  if (*patlist == 0)
	    return NO_MATCH;
	  patlist++;
	}
      while (*patlist != delim);
    }
  return NO_MATCH;
}
