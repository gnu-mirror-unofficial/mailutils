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

#include "mail.h"

/*
 * =
 */

int
mail_eq (int argc, char **argv)
{
  msgset_t *list = NULL;
  size_t n;

  switch (argc)
    {
    case 1:
      n = get_cursor ();
      if (n == 0)
        mu_error (_("No applicable message"));
      else
        mu_printf ("%lu\n", (unsigned long) n);
      break;

    case 2:
      if (msgset_parse (argc, argv, MSG_NODELETED, &list) == 0)
	{
	  if (msgset_msgno (list) <= total)
	    {
	      set_cursor (msgset_msgno (list));
	      mu_printf ("%lu\n", (unsigned long) msgset_msgno (list));
	    }
	  else
	    util_error_range (msgset_msgno (list));
	  msgset_free (list);
	}
      break;

    default:
      return 1;
    }
  
  return 0;
}
