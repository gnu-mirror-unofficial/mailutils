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

void
dm_count (mu_mailbox_t mbx)
{
  size_t n;
  MU_ASSERT (mu_mailbox_messages_count (mbx, &n));
  mu_printf ("%lu\n", (unsigned long) n);
}

void
dm_uidvalidity (mu_mailbox_t mbx)
{
  unsigned long v;
  MU_ASSERT (mu_mailbox_uidvalidity (mbx, &v));
  mu_printf ("%lu\n", v);
}
  
void
dm_uidnext (mu_mailbox_t mbx)
{
  size_t n;
  MU_ASSERT (mu_mailbox_uidnext (mbx, &n));
  mu_printf ("%lu\n", (unsigned long) n);
}

void
dm_recent (mu_mailbox_t mbx)
{
  size_t n;
  MU_ASSERT (mu_mailbox_messages_recent (mbx, &n));
  mu_printf ("%lu\n", (unsigned long) n);
}

void
dm_unseen (mu_mailbox_t mbx)
{
  size_t n;
  MU_ASSERT (mu_mailbox_message_unseen (mbx, &n));
  mu_printf ("%lu\n", (unsigned long) n);
}

typedef void (*dm_action) (mu_mailbox_t);

static struct
{
  char *name;
  dm_action act;
} actions[] = {
  { "count", dm_count },
  { "uidnext", dm_uidnext },
  { "uidvalidity", dm_uidvalidity },
  { "recent", dm_recent },
  { "unseen", dm_unseen },
  { NULL }
};

static dm_action
get_action (char const *s)
{
  size_t i;

  for (i = 0; actions[i].name; i++)
    if (strcmp (actions[i].name, s) == 0)
      return actions[i].act;

  return NULL;
}

int
main (int argc, char **argv)
{
  mu_mailbox_t mbx;
  
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
  
  MU_ASSERT (mu_mailbox_create_default (&mbx, NULL));
  MU_ASSERT (mu_mailbox_open (mbx, MU_STREAM_READ));

  while (argc--)
    {
      char *a = *argv++;
      dm_action f;

      f = get_action (a);
      if (!f)
	{
	  mu_error ("%s: unrecognized action", a);
	  return 1;
	}

      f (mbx);
    }
  return 0;
}
