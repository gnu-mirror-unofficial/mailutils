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

#include "mail.h"

/*
 * d[elete] [msglist]
 */

static int
mail_delete_msg (msgset_t *mspec, mu_message_t msg, void *data)
{
  mu_attribute_t attr;

  mu_message_get_attribute (msg, &attr);
  mu_attribute_set_deleted (attr);
  mu_attribute_unset_userflag (attr, MAIL_ATTRIBUTE_PRESERVED);
  cond_page_invalidate (msgset_msgno (mspec));
  return 0;
}

int
mail_delete (int argc, char **argv)
{
  int rc = util_foreach_msg (argc, argv, MSG_NODELETED|MSG_SILENT,
			     mail_delete_msg, NULL);

  if (mailvar_is_true (mailvar_name_autoprint))
    util_do_command("print");

  return rc;
}

