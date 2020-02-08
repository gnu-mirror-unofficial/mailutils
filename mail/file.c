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

static mu_url_t prev_url;

/* Expand mail special characters:
 * #	    the previous file
 * &	    the current mbox
 * +file    the file named in the folder directory (set folder=foo)
 * %	    system mailbox
 * %user    system mailbox of the user
 * @        file given by the -f option
 */
int
mail_expand_name (const char *name, mu_url_t *purl)
{
  int rc;
  char *exp = NULL;

  if (strcmp (name, "#") == 0)
    {
      if (!prev_url)
	{
	  mu_error (_("No previous file"));
	  return -1;
	}
      else
	{
	  rc = mu_url_dup (prev_url, purl);
	  if (rc)
	    mu_diag_funcall (MU_DIAG_ERROR, "mu_url_dup", exp, rc);
	  return rc;
	}
    }

  if (secondary_url && strcmp (name, "@") == 0)
    {
      rc = mu_url_dup (secondary_url, purl);
      if (rc)
	mu_diag_funcall (MU_DIAG_ERROR, "mu_url_dup",
			 mu_url_to_string (secondary_url), rc);
      return rc;
    }

  if (strcmp (name, "&") == 0)
    {
      name = getenv ("MBOX");
      if (!name)
	{
	  mu_error (_("MBOX environment variable not set"));
	  return MU_ERR_FAILURE;
	}
      /* else fall through */
    }

  rc = mu_mailbox_expand_name (name, &exp);

  if (rc)
    mu_error (_("Failed to expand %s: %s"), name, mu_strerror (rc));

  rc = mu_url_create (purl, exp);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_url_create", exp, rc);
  free (exp);

  return rc;
}

/*
 * fi[le] [file]
 * fold[er] [file]
 */

int
mail_file (int argc, char **argv)
{
  if (argc == 1)
    {
      mail_summary (0, NULL);
    }
  else if (argc == 2)
    {
      /* switch folders */
      mu_url_t url, tmp_url;
      mu_mailbox_t newbox = NULL;
      int status;

      if (mail_expand_name (argv[1], &url))
	return 1;

      status = mu_mailbox_create_from_url (&newbox, url);
      if (status)
	{
	  mu_error(_("Cannot create mailbox %s: %s"),
		   mu_url_to_string (url),
		   mu_strerror (status));
	  mu_url_destroy (&url);
	  return 1;
	}
      mu_mailbox_attach_ticket (newbox);

      if ((status = mu_mailbox_open (newbox, MU_STREAM_RDWR)) != 0)
	{
	  mu_error(_("Cannot open mailbox %s: %s"),
		   mu_url_to_string (url), mu_strerror (status));
	  mu_mailbox_destroy (&newbox);
	  return 1;
	}

      page_invalidate (1); /* Invalidate current page map */

      mu_mailbox_get_url (mbox, &url);
      mu_url_dup (url, &tmp_url);

      if (mail_mbox_close ())
	{
	  mu_url_destroy (&tmp_url);
	  mu_mailbox_close (newbox);
	  mu_mailbox_destroy (&newbox);
	  return 1;
	}

      mu_url_destroy (&prev_url);
      prev_url = tmp_url;

      mbox = newbox;
      mu_mailbox_messages_count (mbox, &total);
      set_cursor (1);
      if (mailvar_is_true (mailvar_name_header))
	{
	  util_do_command ("summary");
	  util_do_command ("headers");
	}
      return 0;
    }
  else
    {
      mu_error (_("%s takes only one argument"), argv[0]);
    }
  return 1;
}
