/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019-2020 Free Software Foundation, Inc.

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
  mu_mailbox_t mbx;
  mu_message_t msg = NULL;
  char *mailbox_name = getenv ("MAIL");
  mu_message_qid_t qid;
  mu_stream_t str;
  
  mu_set_program_name (argv[0]);
  mu_stdstream_setup (MU_STDSTREAM_RESET_NONE);
  mu_registrar_record (mu_dotmail_record);

  argc--;
  argv++;

  if (argc && strcmp (argv[0], "-d") == 0)
    {
      mu_debug_enable_category ("mailbox", 7,
				MU_DEBUG_LEVEL_UPTO (MU_DEBUG_PROT));
      argc--;
      argv++;
    }

  MU_ASSERT (mu_mailbox_create_default (&mbx, mailbox_name));
  MU_ASSERT (mu_mailbox_open (mbx, MU_STREAM_READ|MU_STREAM_QACCESS));

  if (argc != 1)
    {
      mu_error ("only one argument is allowed");
      return 1;
    }

  qid = argv[0];
  MU_ASSERT (mu_mailbox_quick_get_message (mbx, qid, &msg));
  MU_ASSERT (mu_message_get_streamref (msg, &str));

  MU_ASSERT (mu_stream_copy (mu_strout, str, 0, NULL));
  mu_stream_destroy (&str);
  return 0;
}

  
