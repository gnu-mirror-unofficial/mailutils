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

/* ta[g] [msglist] */
/* unt[ag] [msglist] */

static int
tag_message (msgset_t *msgset, mu_message_t mesg, void *arg)
{
  mu_attribute_t attr;
  int *action = arg;

  mu_message_get_attribute (mesg, &attr);
  if (*action)
    mu_attribute_set_userflag (attr, MAIL_ATTRIBUTE_TAGGED);
  else
    mu_attribute_unset_userflag (attr, MAIL_ATTRIBUTE_TAGGED);
  return 0;
}

int
mail_tag (int argc, char **argv)
{
  int action = argv[0][0] != 'u';
  
  return util_foreach_msg (argc, argv, MSG_NODELETED|MSG_SILENT,
			   tag_message, &action);
}
