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
 * q[uit]
 * <EOF>
 */

int
mail_quit (int argc MU_ARG_UNUSED, char **argv MU_ARG_UNUSED)
{
  if (mail_mbox_close ())
    return 1;
  exit (0);
}

int
mail_mbox_close (void)
{
  mu_url_t url = NULL;
  size_t held_count = 0;

  if (!mbox)
    return 0;

  if (!mailvar_is_true (mailvar_name_readonly))
    {
      if (mail_mbox_commit ())
	return 1;

      mu_mailbox_flush (mbox, 1);
    }
  
  mu_mailbox_get_url (mbox, &url);
  mu_mailbox_messages_count (mbox, &held_count);
  mu_printf (
           ngettext ("Held %lu message in %s\n",
                     "Held %lu messages in %s\n",
                     held_count),
           (unsigned long) held_count, util_url_to_string (url));
  mu_mailbox_close (mbox);
  mu_mailbox_destroy (&mbox);
  return 0;
}

enum mailbox_class
  {
    MBX_SYSTEM,
    MBX_MBOX,
    MBX_USER
  };

static enum mailbox_class
mailbox_classify (void)
{
  mu_url_t url;

  mu_mailbox_get_url (mbox, &url);
  if (strcmp (util_url_to_string (url), getenv ("MBOX")) == 0)
    return MBX_MBOX;
  else
    {
      mu_mailbox_t mb;
      mu_url_t u;
      mu_mailbox_create_default (&mb, NULL);
      mu_mailbox_get_url (mb, &u);
      if (strcmp (mu_url_to_string (u), mu_url_to_string (url)) == 0)
	return MBX_SYSTEM;
    }

  return MBX_USER;
}

int
mail_mbox_commit (void)
{
  unsigned int i;
  mu_mailbox_t dest_mbox = NULL;
  int saved_count = 0;
  mu_message_t msg;
  mu_attribute_t attr;
  int keepsave = mailvar_is_true (mailvar_name_keepsave);
  int hold = mailvar_is_true (mailvar_name_hold);

  enum mailbox_class class = mailbox_classify ();
  if (class != MBX_SYSTEM)
    {
      /* The mailbox we are closing is not a system one (%). Stay on the
	 safe side: retain both read and saved messages in the mailbox. */
      hold = 1;
      keepsave = 1;
    }

  for (i = 1; i <= total; i++)
    {
      int status;
      enum { ACT_KEEP, ACT_MBOX, ACT_DELETE } action = ACT_KEEP;

      if (util_get_message (mbox, i, &msg))
	return 1;

      mu_message_get_attribute (msg, &attr);
      if (mu_attribute_is_deleted (attr))
	continue;

      if (mu_attribute_is_userflag (attr, MAIL_ATTRIBUTE_PRESERVED))
	action = ACT_KEEP;
      else if (mu_attribute_is_userflag (attr, MAIL_ATTRIBUTE_MBOXED))
	action = ACT_MBOX;
      else if (class == MBX_SYSTEM)
	{
	  if (mu_attribute_is_userflag (attr, MAIL_ATTRIBUTE_SHOWN
					      | MAIL_ATTRIBUTE_TOUCHED))
	    action = hold ? ACT_KEEP : ACT_MBOX;
	  else if (mu_attribute_is_userflag (attr, MAIL_ATTRIBUTE_SAVED))
	    {
	      if (keepsave)
		action = hold ? ACT_KEEP : ACT_MBOX;
	      else
		action = ACT_DELETE;
	    }
	}

      switch (action)
	{
	case ACT_KEEP:
	  if (mu_attribute_is_read (attr))
	    mu_attribute_set_seen (attr);
	  break;

	case ACT_MBOX:
	  if (!dest_mbox)
	    {
	      char *name = getenv ("MBOX");
	       
	      if ((status = mu_mailbox_create_default (&dest_mbox, name)) != 0)
		{
		  mu_error (_("Cannot create mailbox %s: %s"), name,
                              mu_strerror (status));
		  return 1;
		}
              if ((status = mu_mailbox_open (dest_mbox,
					     MU_STREAM_WRITE | MU_STREAM_CREAT))
		  != 0)
		{
		  mu_error (_("Cannot open mailbox %s: %s"), name,
                              mu_strerror (status));
		  return 1;
		}
	    }

	  status = mu_mailbox_append_message (dest_mbox, msg);
	  if (status)
	    {
	      mu_url_t url = NULL;
	      mu_mailbox_get_url (dest_mbox, &url);
	      mu_error (_("Cannot append message to %s: %s"),
			util_url_to_string (url),
			mu_strerror (status));
	    }
	  else
	    {
	      mu_attribute_set_deleted (attr);
	      saved_count++;
	    }
	  break;

	case ACT_DELETE:
	  mu_attribute_set_deleted (attr);
	  break;
	}
    }

  if (saved_count)
    {
      mu_url_t u = NULL;

      mu_mailbox_get_url (dest_mbox, &u);
      mu_printf (
              ngettext ("Saved %d message in %s\n",
                        "Saved %d messages in %s\n",
			saved_count),
              saved_count, util_url_to_string (u));
      mu_mailbox_close (dest_mbox);
      mu_mailbox_destroy (&dest_mbox);
    }
  return 0;
}
