/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2020 Free Software Foundation, Inc.

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

static int
show_part (struct mime_descend_closure *closure, void *data)
{
  char *msp;
  size_t size;
  
  msp = msgset_str (closure->msgset);
  mu_printf ("%-16s %-25s", msp, closure->type);
  free (msp);
  
  mu_message_size (closure->message, &size);
  if (size < 1024)
    mu_printf (" %4lu", (unsigned long) size);
  else if (size < 1024*1024)
    mu_printf ("%4luK", (unsigned long) size / 1024);
  else
    mu_printf ("%4luM", (unsigned long) size / 1024 / 1024);
  mu_printf ("\n");
  return 0;
}

static int
show_struct (msgset_t *msgset, mu_message_t msg, void *data)
{
  struct mime_descend_closure mclos;
  
  mclos.hints = 0;
  mclos.msgset = msgset;
  mclos.message = msg;
  mclos.type = NULL;
  mclos.encoding = NULL;
  mclos.parent = NULL;
  
  mime_descend (&mclos, show_part, NULL);

    /* Mark enclosing message as read */
  if (mu_mailbox_get_message (mbox, msgset_msgno (msgset), &msg) == 0)
    util_mark_read (msg);

  return 0;
}

int
mail_struct (int argc, char **argv)
{
  return util_foreach_msg (argc, argv, MSG_NODELETED|MSG_SILENT,
			   show_struct, NULL);
}
