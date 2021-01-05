/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2020-2021 Free Software Foundation, Inc.

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
#include <stdlib.h>
#include <assert.h>
#include <mailutils/mailutils.h>

int
main (int argc, char **argv)
{
  mu_stream_t str;
  mu_message_t msg;
  mu_iterator_t itr;
  
  assert (argc == 2);
  MU_ASSERT (mu_file_stream_create (&str, argv[1], MU_STREAM_READ));
  MU_ASSERT (mu_stream_to_message (str, &msg));
  MU_ASSERT (mu_message_get_iterator (msg, &itr));
  for (mu_iterator_first (itr); !mu_iterator_is_done (itr);
       mu_iterator_next (itr))
    {
      mu_coord_t crd;
      mu_message_t msg;
      mu_stream_t str;
      char *s;
      
      MU_ASSERT (mu_iterator_current_kv (itr, (const void **)&crd,
					 (void**)&msg));
      s = mu_coord_string (crd);
      mu_printf ("%s:\n", s);
      free (s);
      MU_ASSERT (mu_message_get_streamref (msg, &str));
      MU_ASSERT (mu_stream_copy (mu_strout, str, 0, NULL));
      mu_printf ("\n");
    }
  return 0;
}
