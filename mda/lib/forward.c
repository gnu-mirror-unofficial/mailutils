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

/* ".forward" support for GNU MDA */

#include "libmda.h"

static char *forward_file;

#define FORWARD_FILE_PERM_CHECK (				\
			   MU_FILE_SAFETY_OWNER_MISMATCH |	\
			   MU_FILE_SAFETY_GROUP_WRITABLE |	\
			   MU_FILE_SAFETY_WORLD_WRITABLE |	\
			   MU_FILE_SAFETY_LINKED_WRDIR |	\
			   MU_FILE_SAFETY_DIR_IWGRP |		\
			   MU_FILE_SAFETY_DIR_IWOTH )

static int forward_file_checks = FORWARD_FILE_PERM_CHECK;


static int
cb2_forward_file_checks (const char *name, void *data)
{
  if (mu_file_safety_compose (data, name, FORWARD_FILE_PERM_CHECK))
    mu_error (_("unknown keyword: %s"), name);
  return 0;
}

static int
cb_forward_file_checks (void *data, mu_config_value_t *arg)
{
  return mu_cfg_string_value_cb (arg, cb2_forward_file_checks,
				 &forward_file_checks);
}

struct mu_cfg_param mda_forward_cfg[] = {
  { "file", mu_c_string, &forward_file, 0, NULL,
    N_("Process forward file.") },
  { "file-checks", mu_cfg_callback, NULL, 0, cb_forward_file_checks,
    N_("Configure safety checks for the forward file."),
    N_("arg: list") },
  { NULL }
};

/* Auxiliary functions */

/* Forward message MSG to given EMAIL, using MAILER and sender address FROM */
static int
forward_to_email (mu_message_t msg, mu_address_t from,
		  mu_mailer_t mailer, const char *email)
{
  mu_address_t to;
  int rc;
  
  rc = mu_address_create (&to, email);
  if (rc)
    {
      mu_error (_("%s: cannot create email: %s"), email, mu_strerror (rc));
      return 1;
    }

  rc = mu_mailer_send_message (mailer, msg, from, to);
  if (rc)
    mu_error (_("Sending message to `%s' failed: %s"),
	      email, mu_strerror (rc));
  mu_address_destroy (&to);
  return rc;
}

/* Create a mailer if it does not already exist.*/
static int
forward_mailer_create (mu_mailer_t *pmailer)
{
  int rc;

  if (*pmailer == NULL)
    {
      rc = mu_mailer_create (pmailer, NULL);
      if (rc)
	{
	  const char *url = NULL;
	  mu_mailer_get_url_default (&url);
	  mu_error (_("Creating mailer `%s' failed: %s"),
		    url, mu_strerror (rc));
	  return 1;
	}

      rc = mu_mailer_open (*pmailer, 0);
      if (rc)
	{
	  const char *url = NULL;
	  mu_mailer_get_url_default (&url);
	  mu_error (_("Opening mailer `%s' failed: %s"),
		    url, mu_strerror (rc));
	  mu_mailer_destroy (pmailer);
	  return 1;
	}
    }
  return 0;
}

/* Create *PFROM (if it is NULL), from the envelope sender address of MSG. */
static int
create_from_address (mu_message_t msg, mu_address_t *pfrom)
{
  if (!*pfrom)
    {
      mu_envelope_t envelope;
      const char *str;
      int status = mu_message_get_envelope (msg, &envelope);
      if (status)
	{
	  mu_error (_("cannot get envelope: %s"), mu_strerror (status));
	  return 1;
	}
      status = mu_envelope_sget_sender (envelope, &str);
      if (status)
	{
	  mu_error (_("cannot get envelope sender: %s"), mu_strerror (status));
	  return 1;
	}
      status = mu_address_create (pfrom, str);
      if (status)
	{
	  mu_error (_("%s: cannot create email: %s"), str,
		    mu_strerror (status));
	  return 1;
	}
    }
  return 0;
}       


/* Forward message MSG as requested by file FILENAME.
   MYNAME gives local user name. */
static enum mda_forward_result
process_forward (mu_message_t msg, char *filename, const char *myname)
{
  int rc;
  mu_stream_t file;
  size_t size = 0, n;
  char *buf = NULL;
  enum mda_forward_result result = mda_forward_ok;
  mu_mailer_t mailer = NULL;
  mu_address_t from = NULL;

  rc = mu_file_stream_create (&file, filename, MU_STREAM_READ);
  if (rc)
    {
      mu_error (_("%s: cannot open forward file: %s"),
		filename, mu_strerror (rc));
      return mda_forward_error;
    }

  while (mu_stream_getline (file, &buf, &size, &n) == 0 && n > 0)
    {
      char *p;

      mu_rtrim_class (buf, MU_CTYPE_SPACE);
      p = mu_str_skip_class (buf, MU_CTYPE_SPACE);

      if (*p && *p != '#')
	{
	  if (strchr (p, '@'))
	    {
	      if (create_from_address (msg, &from)
		  || forward_mailer_create (&mailer)
		  || forward_to_email (msg, from, mailer, p))
		result = mda_forward_error;
	    }
	  else 
	    {
	      if (*p == '\\')
		p++;
	      if (strcmp (p, myname) == 0)
		{
		  if (result == mda_forward_ok)
		    result = mda_forward_metoo;
		}
	      else if (mda_deliver_to_user (msg, p, NULL))
		result = mda_forward_error;
	    }
	}
    }

  mu_address_destroy (&from);
  if (mailer)
    {
      mu_mailer_close (mailer);
      mu_mailer_destroy (&mailer);
    }
  free (buf);
  mu_stream_destroy (&file);
  return result;
}


static mu_list_t idlist;

/* Check if the forward file FWFILE for user given by AUTH exists, and if
   so, use to to forward message MSG. */
enum mda_forward_result
mda_forward (mu_message_t msg, struct mu_auth_data *auth)
{
  struct stat st;
  char *filename;
  enum mda_forward_result result = mda_forward_none;
  int rc;

  if (!forward_file)
    return mda_forward_none;
  
  if (forward_file[0] != '/')
    {
      if (stat (auth->dir, &st))
	{
	  if (errno == ENOENT)
	    /* FIXME: a warning, maybe? */;
	  else if (!S_ISDIR (st.st_mode))
	    mu_error (_("%s: not a directory"), auth->dir);
	  else
	    mu_error (_("%s: cannot stat directory: %s"),
		      auth->dir, mu_strerror (errno));
	  return mda_forward_none;
	}
      filename = mu_make_file_name (auth->dir, forward_file);
    }
  else
    filename = strdup (forward_file);
  
  if (!filename)
    {
      mu_error ("%s", mu_strerror (errno));
      return mda_forward_error;
    }

  if (!idlist)
    mu_list_create (&idlist);

  rc = mu_file_safety_check (filename, forward_file_checks,
			     auth->uid, idlist);
  if (rc == 0)
    result = process_forward (msg, filename, auth->name);
  else if (rc == MU_ERR_EXISTS)
    mu_diag_output (MU_DIAG_NOTICE,
		    _("skipping forward file %s: already processed"),
		    filename);
  else
    mu_error (_("ignoring forward file %s: %s"),
	      filename, mu_strerror (rc));
  
  free (filename);
  return result;
}

