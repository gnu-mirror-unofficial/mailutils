/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. */

#include <mailutils/mailutils.h>

int
main (int argc, char **argv)
{
  mu_record_t rec;
  mu_url_t url;
  
  mu_set_program_name (argv[0]);
  mu_stdstream_setup (MU_STDSTREAM_RESET_NONE);
  mu_registrar_record (mu_dotmail_record);
  
  MU_ASSERT (mu_registrar_lookup_scheme ("dotmail", &rec));

  while (--argc)
    {
      char *name = *++argv;
      int n;
      MU_ASSERT (mu_url_create_hint (&url, name,
				     MU_URL_PARSE_SLASH | MU_URL_PARSE_LOCAL,
				     NULL));
      n = mu_record_is_scheme (rec, url, MU_FOLDER_ATTRIBUTE_FILE);
      mu_printf ("%s: %d\n", name, n);
      mu_url_destroy (&url);
    }
  return 0;
}
