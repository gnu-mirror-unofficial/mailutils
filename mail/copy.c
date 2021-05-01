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
#include <mailutils/locker.h>

/*
 * c[opy] [file]
 * c[opy] [msglist] file
 * C[opy] [msglist]
 */

struct append_stat
{
  size_t size;
  size_t lines;
};

static int
append_to_mailbox (mu_url_t url, msgset_t *msglist, int mark,
		   struct append_stat *totals)
{
  int status;
  mu_mailbox_t mbx;
  msgset_t *mp;
  size_t size;
  mu_message_t msg;
  mu_url_t url_copy;

  mu_url_dup (url, &url_copy);
  if ((status = mu_mailbox_create_from_url (&mbx, url_copy)) != 0)
    {
      mu_url_destroy (&url_copy);
      mu_error (_("Cannot create mailbox %s: %s"), mu_url_to_string (url),
		   mu_strerror (status));
      return 1;
    }
  mu_mailbox_attach_ticket (mbx);
  if ((status = mu_mailbox_open (mbx, MU_STREAM_CREAT | MU_STREAM_APPEND)) != 0)
    {
      mu_error (_("Cannot open mailbox %s: %s"), mu_url_to_string (url),
		   mu_strerror (status));
      mu_mailbox_destroy (&mbx);
      return 1;
    }

  for (mp = msglist; mp; mp = mp->next)
    {
      status = util_get_message (mbox, msgset_msgno (mp), &msg);
      if (status)
	break;

      status = mu_mailbox_append_message (mbx, msg);
      if (status)
	{
	  mu_error (_("Cannot append message: %s"), mu_strerror (status));
	  break;
	}

      mu_message_size (msg, &size);
      totals->size += size;
      mu_message_lines (msg, &size);
      totals->lines += size;

      if (mark)
	{
	  mu_attribute_t attr;
	  mu_message_get_attribute (msg, &attr);
	  mu_attribute_set_userflag (attr, MAIL_ATTRIBUTE_SAVED);
	}
    }

  mu_mailbox_close (mbx);
  mu_mailbox_destroy (&mbx);
  return 0;
}

static int
append_to_file (char const *filename, msgset_t *msglist, int mark,
		struct append_stat *totals)
{
  int status;
  msgset_t *mp;
  mu_stream_t ostr, mstr;
  mu_message_t msg;
  mu_locker_t locker;
  mu_locker_hints_t hints = {
    .flags = MU_LOCKER_FLAG_TYPE | MU_LOCKER_FLAG_RETRY,
    .type = MU_LOCKER_TYPE_KERNEL
  };
  mu_stream_stat_buffer stat;

  status = mu_file_stream_create (&ostr, filename,
				  MU_STREAM_CREAT|MU_STREAM_APPEND);
  if (status)
    {
      mu_error (_("Cannot open output file %s: %s"),
		filename, mu_strerror (status));
      return 1;
    }

  status = mu_locker_create_ext (&locker, filename, &hints);
  if (status)
    {
      mu_error (_("Cannot create locker %s: %s"),
		filename, mu_strerror (status));
      mu_stream_unref (ostr);
      return 1;
    }
  mu_locker_lock_mode (locker, mu_lck_exc);

  status = mu_locker_lock (locker);
  if (status)
    {
      mu_error (_("Cannot lock %s: %s"),
		filename, mu_strerror (status));
      mu_locker_destroy (&locker);
      mu_stream_unref (ostr);
      return 1;
    }

  mu_stream_set_stat (ostr,
		      MU_STREAM_STAT_MASK (MU_STREAM_STAT_OUT) |
		      MU_STREAM_STAT_MASK (MU_STREAM_STAT_OUTLN),
		      stat);
  
  for (mp = msglist; mp; mp = mp->next)
    {
      mu_envelope_t env;
      const char *s, *d;
      int n;
      
      status = util_get_message (mbox, msgset_msgno (mp), &msg);
      if (status)
	break;

      status = mu_message_get_envelope (msg, &env);
      if (status)
	{
	  mu_error (_("Cannot get envelope: %s"), mu_strerror (status));
	  break;
	}

      status = mu_envelope_sget_sender (env, &s);
      if (status)
	{
	  mu_error (_("Cannot get envelope sender: %s"), mu_strerror (status));
	  break;
	}

      status = mu_envelope_sget_date (env, &d);
      if (status)
	{
	  mu_error (_("Cannot get envelope date: %s"), mu_strerror (status));
	  break;
	}

      status = mu_stream_printf (ostr, "From %s %s\n%n", s, d, &n);
      if (status)
	{
	  mu_error (_("Write error: %s"), mu_strerror (status));
	  break;
	}

      status = mu_message_get_streamref (msg, &mstr);
      if (status)
	{
	  mu_error (_("Cannot get message: %s"), mu_strerror (status));
	  break;
	}

      status = mu_stream_copy_nl (ostr, mstr, 0, NULL);
      if (status)
	{
	  mu_error (_("Cannot append message: %s"), mu_strerror (status));
	  break;
	}

      mu_stream_unref (mstr);

      if (mark)
	{
	  mu_attribute_t attr;
	  mu_message_get_attribute (msg, &attr);
	  mu_attribute_set_userflag (attr, MAIL_ATTRIBUTE_SAVED);
	}
    }

  mu_stream_close (ostr);
  mu_stream_unref (ostr);

  mu_locker_unlock (locker);
  mu_locker_destroy (&locker);

  totals->size = stat[MU_STREAM_STAT_OUT];
  totals->lines = stat[MU_STREAM_STAT_OUTLN];

  return 0;
}

/*
 * mail_copy0() is shared between mail_copy() and mail_save().
 * argc, argv -- argument count & vector
 * mark -- whether we should mark the message as saved.
 */
int
mail_copy0 (int argc, char **argv, int mark)
{
  mu_url_t url;
  msgset_t *msglist = NULL;
  struct append_stat totals = { 0, 0 };
  int rc;
  char *filename;

  if (mu_isupper (argv[0][0]))
    {
      if (msgset_parse (argc, argv, MSG_NODELETED, &msglist))
	return 1;
      filename = util_outfolder_name (util_get_sender (msgset_msgno (msglist), 1));
    }
  else
    {
      filename = argc >= 2 ? argv[--argc] : getenv ("MBOX");
      if (msgset_parse (argc, argv, MSG_NODELETED, &msglist))
	return 1;
    }

  if (mail_expand_name (filename, &url))
    return 1;
  filename = (char*) mu_url_to_string (url);
  if (mu_url_is_scheme (url, "file") || mu_url_is_scheme (url, "mbox"))
    rc = append_to_file (filename, msglist, mark, &totals);
  else
    rc = append_to_mailbox (url, msglist, mark, &totals);
  if (rc == 0)
    mu_printf ("\"%s\" %3lu/%-5lu\n", filename,
	       (unsigned long) totals.lines, (unsigned long) totals.size);
  mu_url_destroy (&url);
  msgset_free (msglist);
  return 0;
}

int
mail_copy (int argc, char **argv)
{
  return mail_copy0 (argc, argv, 0);
}
