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

#include <config.h>
#include <stdlib.h>
#include <mailutils/mailutils.h>
#include <muaux.h>
#include <sysexits.h>

int truncate_opt;
int from_filter;

static struct mu_option decodemail_options[] = 
{
  { "truncate", 't', NULL, MU_OPTION_DEFAULT,
    N_("truncate the output mailbox, if it exists"),
    mu_c_bool, &truncate_opt },
  MU_OPTION_END
}, *options[] = { decodemail_options, NULL };

struct mu_cli_setup cli = {
  options,
  NULL,
  N_("GNU decodemail -- decode messages."),
  NULL
};

static char *decodemail_capa[] = {
  "debug",
  "mailbox",
  "locking",
  NULL
};

char *charset;
char *content_type;

static void
define_charset (void)
{
  struct mu_lc_all lc_all = { .flags = 0 };
  char *ep = getenv ("LC_ALL");
  if (!ep)
    ep = getenv ("LANG");

  if (ep && mu_parse_lc_all (ep, &lc_all, MU_LC_CSET) == 0)
    {
      charset = mu_strdup (lc_all.charset);
      mu_lc_all_free (&lc_all);
    }
  else
    charset = mu_strdup ("us-ascii");

  mu_asprintf (&content_type, "text/plain; charset=%s", charset);
}

static mu_message_t message_decode (mu_message_t);
static void message_store_mbox (mu_message_t, mu_mailbox_t);
static void message_store_stdout (mu_message_t, mu_mailbox_t);

int
main (int argc, char **argv)
{
  int rc;
  mu_mailbox_t imbox, ombox = NULL;
  char *imbox_name = NULL, *ombox_name = NULL;
  void (*message_store) (mu_message_t, mu_mailbox_t);
  mu_iterator_t itr;
  unsigned long i;
  int err = 0;
  
  /* Native Language Support */
  MU_APP_INIT_NLS ();

  /* register the formats.  */
  mu_register_all_mbox_formats ();
  mu_register_extra_formats ();
  mu_auth_register_module (&mu_auth_tls_module);

  mu_cli (argc, argv, &cli, decodemail_capa, NULL, &argc, &argv);

  switch (argc)
    {
    case 2:
      ombox_name = argv[1];
    case 1:
      imbox_name = argv[0];
      break;
    case 0:
      break;
    default:
      mu_error (_("too many arguments; try %s --help for help"),
		mu_program_name);
      exit (EX_USAGE);
    }

  /* Open input mailbox */
  rc = mu_mailbox_create_default (&imbox, imbox_name);
  if (rc != 0)
    {
      if (imbox_name)
	mu_error (_("could not create mailbox `%s': %s"),
		  imbox_name,
		  mu_strerror (rc));
      else
	mu_error (_("could not create default mailbox: %s"),
		  mu_strerror (rc));
      exit (EX_OSERR);
    }

  rc = mu_mailbox_open (imbox, MU_STREAM_READ);
  if (rc)
    {
      mu_url_t url = NULL;

      mu_mailbox_get_url (imbox, &url);
      mu_error (_("could not open input mailbox `%s': %s"),
		mu_url_to_string (url), mu_strerror (rc));
      exit (EX_NOINPUT);
    }

  /* Create output mailbox */
  if (ombox_name)
    {
      mu_property_t prop;
      const char *type;
      
      rc = mu_mailbox_create_default (&ombox, ombox_name);
      if (rc != 0)
	{
	  mu_error (_("could not create output mailbox `%s': %s"),
		    ombox_name,
		    mu_strerror (rc));
	  exit (EX_OSERR);
	}
      rc = mu_mailbox_open (ombox, MU_STREAM_RDWR|MU_STREAM_CREAT);
      if (rc)
	{
	  mu_error (_("could not open mailbox `%s': %s"),
		    ombox_name, mu_strerror (rc));
	  exit (EX_CANTCREAT);
	}

      if (mu_mailbox_get_property (ombox, &prop) == 0 &&
	  mu_property_sget_value (prop, "TYPE", &type) == 0 &&
	  strcmp (type, "MBOX") == 0)
	from_filter = 1;
      
      if (truncate_opt)
	{
	  mu_mailbox_get_iterator (ombox, &itr);
	  for (mu_iterator_first (itr), i = 1; !mu_iterator_is_done (itr);
	       mu_iterator_next (itr), i++)
	    {
	      mu_message_t m;
	      mu_attribute_t a;
	      
	      rc = mu_iterator_current (itr, (void **)&m);
	      mu_message_get_attribute (m, &a);
	      mu_attribute_set_deleted (a);
	    }
	  mu_iterator_destroy (&itr);
	}
      message_store = message_store_mbox;
    }
  else
    {
      message_store = message_store_stdout;
      from_filter = 1;
    }
  
  define_charset ();
  
  rc = mu_mailbox_get_iterator (imbox, &itr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_mailbox_get_iterator", NULL, rc);
      exit (EX_SOFTWARE);
    }

  for (mu_iterator_first (itr), i = 1; !mu_iterator_is_done (itr);
       mu_iterator_next (itr), i++)
    {
      mu_message_t msg, newmsg;

      rc = mu_iterator_current (itr, (void **)&msg);
      if (rc)
	{
	  mu_error (_("cannot read message %lu: %s"),
		    i, mu_strerror (rc));
	  err = 1;
	  continue;
	}
      newmsg = message_decode (msg);
      message_store (newmsg, ombox);
      mu_message_unref (newmsg);
    }
  mu_mailbox_destroy (&imbox);
  (truncate_opt ? mu_mailbox_expunge : mu_mailbox_sync) (ombox);
  mu_mailbox_destroy (&ombox);

  if (err)
    exit (EX_UNAVAILABLE);
  exit (EX_OK);
}

static void
message_store_mbox (mu_message_t msg, mu_mailbox_t mbx)
{
  int rc = mu_mailbox_append_message (mbx, msg);
  if (rc)
    {
      mu_error (_("cannot store message: %s"), mu_strerror (rc));
      exit (EX_IOERR);
    }
}

static void
env_print (mu_message_t msg)
{
  mu_envelope_t env;
  char const *buf;
  size_t len;
  
  mu_message_get_envelope (msg, &env);
  if (mu_envelope_sget_sender (env, &buf))
    buf = "UNKNOWN";
  mu_printf ("From %s ", buf);
  
  if (mu_envelope_sget_date (env, &buf))
    { 
      char datebuf[MU_DATETIME_FROM_LENGTH+1];
      time_t t;
      struct tm *tm;

      t = time (NULL);
      tm = gmtime (&t);
      mu_strftime (datebuf, sizeof datebuf, MU_DATETIME_FROM, tm);
      buf = datebuf;
    }

  mu_printf ("%s", buf);
  len = strlen (buf);
  if (len > 1 && buf[len-1] != '\n')
    mu_printf ("\n");
}

static void
hdr_print (mu_message_t msg)
{
  mu_header_t hdr;
  mu_stream_t str;
  
  mu_message_get_header (msg, &hdr);
  mu_header_get_streamref (hdr, &str);
  mu_stream_copy (mu_strout, str, 0, NULL);
  mu_stream_destroy (&str);
}

static void
body_print (mu_message_t msg)
{
  int rc;
  mu_body_t body;
  mu_stream_t str;
  
  mu_message_get_body (msg, &body);
  rc = mu_body_get_streamref (body, &str);
  if (rc)
    {
      mu_error (_("cannot get body stream: %s"), mu_strerror (rc));
      exit (EX_OSERR);
    }
  mu_stream_copy (mu_strout, str, 0, NULL);
  mu_stream_destroy (&str);
}

static void
message_store_stdout (mu_message_t msg, mu_mailbox_t mbx)
{
  env_print (msg);
  hdr_print (msg);
  body_print (msg);
  mu_printf ("\n");
}

static mu_message_t
message_decode (mu_message_t msg)
{
  mu_message_t newmsg;
  int ismime;
  int rc;

  mu_message_is_multipart (msg, &ismime);
  if (!ismime)
    {
      mu_stream_t str;
      
      rc = message_body_stream (msg, from_filter, charset, &str);
      if (rc)
	{
	  newmsg = msg;
	  mu_message_ref (newmsg);
	}
      else
	{
	  mu_body_t body;
	  mu_stream_t bstr;
	  mu_header_t hdr, newhdr;
	  mu_iterator_t itr;
	  size_t i;
	  
	  mu_message_create (&newmsg, NULL);
	  mu_message_get_body (newmsg, &body);
	  mu_body_get_streamref (body, &bstr);
	  rc = mu_stream_copy (bstr, str, 0, NULL);
	  if (rc)
	    {
	      mu_diag_funcall (MU_DIAG_ERROR, "mu_steam_copy", NULL, rc);
	      exit (EX_IOERR);
	    }
	  mu_stream_unref (bstr);
	  mu_stream_unref (str);

	  mu_message_get_header (msg, &hdr);
	  mu_message_get_header (newmsg, &newhdr);
	  mu_header_get_iterator (hdr, &itr);
	  
	  for (mu_iterator_first (itr), i = 1; !mu_iterator_is_done (itr);
	       mu_iterator_next (itr), i++)
	    {
	      const char *name;
	      const char *value;
	      char *s;
	      
	      rc = mu_iterator_current_kv (itr, (void const **) &name,
					   (void**)&value);

	      if (mu_c_strcasecmp (name, MU_HEADER_CONTENT_TYPE) == 0 ||
		  mu_c_strcasecmp (name, MU_HEADER_CONTENT_TRANSFER_ENCODING) == 0 ||
		  mu_c_strcasecmp (name, MU_HEADER_CONTENT_DISPOSITION) == 0)
		continue;
	  
	      rc = mu_rfc2047_decode (charset, value, &s);
	      if (rc == 0)
		{
		  mu_header_append (newhdr, name, s);
		  free (s);
		}
	      else
		mu_header_append (newhdr, name, value);
	    }
	  mu_iterator_destroy (&itr);

	  mu_header_set_value (newhdr,
			       MU_HEADER_CONTENT_TRANSFER_ENCODING,
			       "8bit",
			       1);
	  mu_header_set_value (newhdr,
			       MU_HEADER_CONTENT_TYPE,
			       content_type,
			       1);
	  mu_header_set_value (newhdr,
			       MU_HEADER_CONTENT_DISPOSITION,
			       "inline",
			       1);
	}
    }
  else
    {
      size_t nparts, i;
      mu_mime_t mime;
      mu_header_t hdr, newhdr;
      mu_iterator_t itr;
      
      mu_mime_create (&mime, NULL, 0);
      mu_message_get_num_parts (msg, &nparts);

      for (i = 1; i <= nparts; i++)
	{
	  mu_message_t msgpart, msgdec;
	  
	  mu_message_get_part (msg, i, &msgpart);
	  msgdec = message_decode (msgpart);
	  mu_mime_add_part (mime, msgdec);
	  mu_message_unref (msgdec);
	}

      mu_mime_to_message (mime, &newmsg);
      mu_mime_unref (mime);

      // Add headers
      mu_message_get_header (msg, &hdr);
      mu_message_get_header (newmsg, &newhdr);
      mu_header_get_iterator (hdr, &itr);

      for (mu_iterator_first (itr), i = 1; !mu_iterator_is_done (itr);
	   mu_iterator_next (itr), i++)
	{
	  const char *name;
	  const char *value;
	  char *s;
      
	  rc = mu_iterator_current_kv (itr, (void const **) &name,
				       (void**)&value);

	  if (mu_c_strcasecmp (name, MU_HEADER_MIME_VERSION) == 0 ||
	      mu_c_strcasecmp (name, MU_HEADER_CONTENT_TYPE) == 0)
	    continue;
	  
	  rc = mu_rfc2047_decode (charset, value, &s);
	  if (rc == 0)
	    {
	      mu_header_append (newhdr, name, s);
	      free (s);
	    }
	  else
	    mu_header_append (newhdr, name, value);
	}
      mu_iterator_destroy (&itr);
    }
  
  return newmsg;
}

