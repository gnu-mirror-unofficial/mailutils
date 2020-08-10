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

#include "libmda.h"

static char *default_domain;
int multiple_delivery;     /* Don't return errors when delivering to multiple
			      recipients */
static char *sender_address = NULL;       

static void
set_sender_address (struct mu_parseopt *po, struct mu_option *opt,
		    char const *arg)
{
  if (sender_address != NULL)
    {
      mu_parseopt_error (po, _("multiple --from options"));
      exit (po->po_exit_error);
    }
  else
    {
      char *errmsg;
      int rc = mu_str_to_c (arg, opt->opt_type, opt->opt_ptr, &errmsg);
      if (rc)
	{
	  mu_parseopt_error (po, _("can't set sender address: %s"),
			     errmsg ? errmsg : mu_strerror (rc));
	  exit (po->po_exit_error);
	}
    }
}

struct mu_cfg_param mda_deliver_cfg[] = {
  { "domain", mu_c_string, &default_domain, 0, NULL,
    N_("Default email domain") },
  { "exit-multiple-delivery-success", mu_c_bool, &multiple_delivery, 0, NULL,
    N_("In case of multiple delivery, exit with code 0 if at least one "
       "delivery succeeded.") },
  { NULL }
};

struct mu_option mda_deliver_options[] = {
  MU_OPTION_GROUP (N_("Delivery options")),
  { "from", 'f', N_("EMAIL"), MU_OPTION_DEFAULT,
    N_("specify the sender's name"),
    mu_c_string, &sender_address, set_sender_address },
  { NULL,   'r', NULL, MU_OPTION_ALIAS },
  MU_OPTION_END
};

static mu_message_t
make_tmp (const char *from)
{
  int rc;
  mu_stream_t in, out;
  char *buf = NULL;
  size_t size = 0, n;
  mu_message_t mesg;

  rc = mu_stdio_stream_create (&in, MU_STDIN_FD, MU_STREAM_READ);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stdio_stream_create",
                       "MU_STDIN_FD", rc);
      exit (EX_TEMPFAIL);
    }

  rc = mu_temp_stream_create (&out, 0);
  if (rc)
    {
      mda_error (_("unable to open temporary stream: %s"), mu_strerror (rc));
      exit (EX_TEMPFAIL);
    }

  rc = mu_stream_getline (in, &buf, &size, &n);
  if (rc)
    {
      mda_error (_("read error: %s"), mu_strerror (rc));
      mu_stream_destroy (&in);
      mu_stream_destroy (&out);
      exit (EX_TEMPFAIL);
    }
  if (n == 0)
    {
      mda_error (_("unexpected EOF on input"));
      mu_stream_destroy (&in);
      mu_stream_destroy (&out);
      exit (EX_TEMPFAIL);
    }

  if (n >= 5 && memcmp (buf, "From ", 5))
    {
      struct mu_auth_data *auth = NULL;
      if (!from)
        {
          auth = mu_get_auth_by_uid (getuid ());
          if (auth)
            from = auth->name;
        }
      if (from)
        {
          time_t t;
          struct tm *tm;

          time (&t);
          tm = gmtime (&t);
          mu_stream_printf (out, "From %s ", from);
          mu_c_streamftime (out, "%c%n", tm, NULL);
        }
      else
        {
          mda_error (_("cannot determine sender address"));
          mu_stream_destroy (&in);
          mu_stream_destroy (&out);
          exit (EX_TEMPFAIL);
        }
      if (auth)
        mu_auth_data_free (auth);
    }

  mu_stream_write (out, buf, n, NULL);
  free (buf);

  rc = mu_stream_copy (out, in, 0, NULL);
  mu_stream_destroy (&in);
  if (rc)
    {
      mda_error (_("copy error: %s"), mu_strerror (rc));
      mu_stream_destroy (&out);
      exit (EX_TEMPFAIL);
    }

  rc = mu_stream_to_message (out, &mesg);
  mu_stream_destroy (&out);
  if (rc)
    {
      mda_error (_("error creating temporary message: %s"),
                    mu_strerror (rc));
      exit (EX_TEMPFAIL);
    }

  return mesg;
}

int
mda_run_delivery (mda_delivery_fn delivery_fun, int argc, char **argv)
{
  mu_message_t mesg = make_tmp (sender_address);

  if (multiple_delivery)
    multiple_delivery = argc > 1;

  for (; *argv; argv++)
    {
      delivery_fun (mesg, *argv, NULL);
      if (multiple_delivery)
        exit_code = EX_OK;
    }
  return exit_code;
}

static int
deliver_to_mailbox (mu_mailbox_t mbox, mu_message_t msg,
		    struct mu_auth_data *auth,
		    char **errp)
{
  int status;
  char *path;
  mu_url_t url = NULL;
  mu_locker_t lock;
  int failed = 0;
  int exit_code = EX_OK;

  mu_mailbox_get_url (mbox, &url);
  path = (char*) mu_url_to_string (url);

  status = mu_mailbox_open (mbox, MU_STREAM_APPEND|MU_STREAM_CREAT);
  if (status != 0)
    {
      mda_error (_("cannot open mailbox %s: %s"), 
                    path, mu_strerror (status));
      return EX_TEMPFAIL;
    }

  /* FIXME: This is superfluous, as mu_mailbox_append_message takes care
     of locking anyway. But I leave it here for the time being. */
  mu_mailbox_get_locker (mbox, &lock);

  if (lock)
    {
      status = mu_locker_lock (lock);

      if (status)
	{
	  mda_error (_("cannot lock mailbox `%s': %s"), path,
		        mu_strerror (status));
	  return EX_TEMPFAIL;
	}
    }
  
#if defined(USE_MAILBOX_QUOTAS)
  if (auth)
    {
      mu_off_t n;
      size_t msg_size;
      mu_off_t mbsize;
      
      if ((status = mu_mailbox_get_size (mbox, &mbsize)))
	{
	  mda_error (_("cannot get size of mailbox %s: %s"),
			path, mu_strerror (status));
	  if (status == ENOSYS)
	    mbsize = 0; /* Try to continue anyway */
	  else
	    return EX_TEMPFAIL;
	}
    
      switch (mda_check_quota (auth, mbsize, &n))
	{
	case MQUOTA_EXCEEDED:
	  mda_error (_("%s: mailbox quota exceeded for this recipient"),
			auth->name);
	  if (errp)
	    mu_asprintf (errp, "%s: mailbox quota exceeded for this recipient",
		         auth->name);
	  exit_code = EX_QUOTA;
	  failed++;
	  break;
	  
	case MQUOTA_UNLIMITED:
	  break;
	  
	default:
	  if ((status = mu_message_size (msg, &msg_size)))
	    {
	      mda_error (_("cannot get message size (input message %s): %s"),
			    path, mu_strerror (status));
	      exit_code = EX_UNAVAILABLE;
	      failed++;
	    }
	  else if (msg_size > n)
	    {
	      mda_error (_("%s: message would exceed maximum mailbox size for "
			      "this recipient"),
			    auth->name);
	      if (errp)
		mu_asprintf (errp,
			     "%s: message would exceed maximum mailbox size "
			     "for this recipient",
			     auth->name);
	      exit_code = EX_QUOTA;
	      failed++;
	    }
	  break;
	}
    }
#endif
  
  if (!failed)
    {
      status = mu_mailbox_append_message (mbox, msg);
      if (status)
	{
	  mda_error (_("error writing to mailbox %s: %s"),
		        path, mu_strerror (status));
	  failed++;
	}
      else
	{
	  status = mu_mailbox_sync (mbox);
	  if (status)
	    {
	      mda_error (_("error flushing mailbox %s: %s"),
			    path, mu_strerror (status));
	      failed++;
	    }
	}
    }

  mu_mailbox_close (mbox);
  mu_locker_unlock (lock);
  return failed ? exit_code : 0;
}

static int
is_remote_url (mu_url_t url)
{
  int rc, res;

  if (!url)
    return 0;
  
  rc = mu_registrar_test_local_url (url, &res);
  return rc == 0 && res == 0;
}

static int
do_delivery (mu_url_t url, mu_message_t msg, const char *name, char **errp)
{
  struct mu_auth_data *auth = NULL;
  mu_mailbox_t mbox;
  int status;

  mu_set_user_email_domain (default_domain);
  
  if (name && !is_remote_url (url))
    {
      auth = mu_get_auth_by_name (name);
      if (!auth)
	{
	  mda_error (_("%s: no such user"), name);
	  if (errp)
	    mu_asprintf (errp, "%s: no such user", name);
	  exit_code = EX_NOUSER;
	  return EX_NOUSER;
	}

      status = mu_set_user_email (name);
      if (status)
	mu_error (_("%s: invalid email: %s"), name, mu_strerror (status));
      
      if (getuid ())
	auth->change_uid = 0;

      switch (mda_filter_message (msg, auth))
	{
	case MDA_FILTER_OK:
	  break;

	case MDA_FILTER_FILTERED:
	  exit_code = EX_OK;
	  mu_auth_data_free (auth);
	  return 0;

	case MDA_FILTER_FAILURE:
	  return exit_code = EX_TEMPFAIL;
	}
 
      switch (mda_forward (msg, auth))
	{
	case mda_forward_none:
	case mda_forward_metoo:
	  break;
	    
	case mda_forward_ok:
	  mu_auth_data_free (auth);
	  return 0;

	case mda_forward_error:
	  mu_auth_data_free (auth);
	  return exit_code = EX_TEMPFAIL;
	}
    }
  else
    mu_set_user_email (NULL);
  
  if (!url)
    {
      status = mu_url_create (&url, auth->mailbox);
      if (status)
	{
	  mda_error (_("cannot create URL for %s: %s"),
			auth->mailbox, mu_strerror (status));
	  return exit_code = EX_UNAVAILABLE;
	}
    }      

  status = mu_mailbox_create_from_url (&mbox, url);

  if (status)
    {
      mda_error (_("cannot open mailbox %s: %s"),
		    mu_url_to_string (url),
		    mu_strerror (status));
      mu_url_destroy (&url);
      mu_auth_data_free (auth);
      return EX_TEMPFAIL;
    }

  status = mu_mailbox_set_notify (mbox, name);
  if (status)
    mu_error (_("failed to set notification on %s: %s"),
	      mu_url_to_string (url),
	      mu_strerror (status));
  
  /* Actually open the mailbox. Switch to the user's euid to make
     sure the maildrop file will have right privileges, in case it
     will be created */
  if (mda_switch_user_id (auth, 1))
    return EX_TEMPFAIL;
  status = deliver_to_mailbox (mbox, msg, auth, errp);
  if (mda_switch_user_id (auth, 0))
    return EX_TEMPFAIL;

  mu_auth_data_free (auth);
  mu_mailbox_destroy (&mbox);

  return status;
}

int
mda_deliver_to_url (mu_message_t msg, char *dest_id, char **errp)
{
  int status;
  const char *name;
  mu_url_t url = NULL;
  
  status = mu_url_create (&url, dest_id);
  if (status)
    {
      mda_error (_("%s: cannot create url: %s"), dest_id,
		    mu_strerror (status));
      return EX_NOUSER;
    }
  status = mu_url_sget_user (url, &name);
  if (status == MU_ERR_NOENT)
    name = NULL;
  else if (status)
    {
      mda_error (_("%s: cannot get user name from url: %s"),
		    dest_id, mu_strerror (status));
      mu_url_destroy (&url);
      return EX_NOUSER;
    }
  return do_delivery (url, msg, name, errp);
}
  
int
mda_deliver_to_user (mu_message_t msg, char *dest_id, char **errp)
{
  return do_delivery (NULL, msg, dest_id, errp);
}
