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
#include <mailutils/sys/envelope.h>
#include <muaux.h>
#include <sysexits.h>

int truncate_opt;
int from_filter;
int recode_charset;
char *charset;

static struct mu_option decodemail_options[] = 
{
  { "truncate", 't', NULL, MU_OPTION_DEFAULT,
    N_("truncate the output mailbox, if it exists"),
    mu_c_bool, &truncate_opt },
  { "charset", 'c', N_("CHARSET"), MU_OPTION_DEFAULT,
    N_("recode output to this charset"),
    mu_c_string, &charset },
  { "recode", 'R', NULL, MU_OPTION_DEFAULT,
    N_("recode text parts to the current charset"),
    mu_c_bool, &recode_charset },
  MU_OPTION_END
}, *options[] = { decodemail_options, NULL };

struct mu_cli_setup cli = {
  .optv = options,
  .prog_doc = N_("GNU decodemail -- decode messages."),
  .prog_args = N_("[INBOX] [OUTBOX]")
};

static char *decodemail_capa[] = {
  "debug",
  "mailbox",
  "locking",
  "mime",
  NULL
};

char *charset;

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
}

static mu_message_t message_decode (mu_message_t, int);
/* Values for the message_decode second parameter */
enum { MSG_PART, MSG_TOP };

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
  mu_cli_capa_register (&mu_cli_capa_mime);
  
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

  if (!charset && recode_charset)
    define_charset ();
  
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
      newmsg = message_decode (msg, MSG_TOP);
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
message_decode (mu_message_t msg, int what)
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
	  char *content_type = NULL;
	  mu_stream_stat_buffer stat;
	  
	  mu_message_create (&newmsg, NULL);
	  mu_message_get_body (newmsg, &body);
	  mu_body_get_streamref (body, &bstr);
	  mu_stream_set_stat (bstr,
			      MU_STREAM_STAT_MASK (MU_STREAM_STAT_IN8BIT),
			      stat);
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

	      if (!mu_c_strcasecmp (name, MU_HEADER_CONTENT_TYPE))
		{
		  if (charset)
		    {
		      mu_content_type_t ct;
		      struct mu_mime_param **pparam;
		      char *vc = mu_strdup (value);
		      size_t len;
		      mu_string_unfold (vc, &len);
		      rc = mu_content_type_parse (vc, NULL, &ct);
		      free (vc);
		      if (rc)
			{
			  mu_diag_funcall (MU_DIAG_ERROR,
					   "mu_content_type_parse", NULL, rc);
			  continue;
			}
		      rc = mu_assoc_install_ref (ct->param, "charset", &pparam);
		      switch (rc)
			{
			case 0:
			  *pparam = mu_zalloc (sizeof **pparam);
			  break;

			case MU_ERR_EXISTS:
			  free ((*pparam)->value);
			  break;

			default:
			  mu_diag_funcall (MU_DIAG_ERROR,
					   "mu_assoc_install_ref", NULL, rc);
			  exit (EX_IOERR);
			}
		      (*pparam)->value = mu_strdup (charset);
		      mu_content_type_format (ct, &content_type);
		      mu_content_type_destroy (&ct);
		      continue;
		    }
		}
	      else if (!mu_c_strcasecmp (name,
					 MU_HEADER_CONTENT_TRANSFER_ENCODING))
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
			       stat[MU_STREAM_STAT_IN8BIT] ? "8bit" : "7bit",
			       1);
	  if (charset)
	    {
	      if (!content_type)
		mu_asprintf (&content_type, "text/plain; charset=%s", charset);
	      mu_header_set_value (newhdr,
				   MU_HEADER_CONTENT_TYPE,
				   content_type,
				   1);
	      free (content_type);
	    }
	}
    }
  else
    {
      size_t nparts, i;
      mu_mime_t mime;
      mu_header_t hdr, newhdr;
      mu_iterator_t itr;
      char *content_type;
      char *subtype;

      /* FIXME: The following could be simplified if we could obtain
	 a mime object from the message */
      mu_message_get_header (msg, &hdr);
      rc = mu_header_aget_value_unfold (hdr, MU_HEADER_CONTENT_TYPE,
					&content_type);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_header_aget_value_unfold",
			   MU_HEADER_CONTENT_TYPE, rc);
	  exit (EX_SOFTWARE);
	}
      subtype = strchr (content_type, '/');
      if (subtype)
	{
	  subtype++;
	  subtype[strcspn (subtype, ";")] = 0;
	}
      else
	subtype = "multipart";
      
      mu_mime_create_multipart (&mime, subtype);
      free (content_type);
      mu_message_get_num_parts (msg, &nparts);

      for (i = 1; i <= nparts; i++)
	{
	  mu_message_t msgpart, msgdec;
	  
	  mu_message_get_part (msg, i, &msgpart);
	  msgdec = message_decode (msgpart, MSG_PART);
	  mu_mime_add_part (mime, msgdec);
	  mu_message_unref (msgdec);
	}

      mu_mime_to_message (mime, &newmsg);
      mu_mime_unref (mime);

      if (what == MSG_TOP)
	{
	  /* Copy envelope */
	  mu_envelope_t env, newenv;

	  rc = mu_message_get_envelope (msg, &env);
	  if (rc == 0)
	    {
	      rc = mu_envelope_create (&newenv, newmsg);
	      if (rc == 0)
		{
		  if ((rc = mu_envelope_aget_sender (env, &newenv->sender)) ||
		      (rc = mu_envelope_aget_date (env, &newenv->date)))
		    {
		      mu_error ("%s", _("can't copy envelope"));
		      exit (EX_UNAVAILABLE);
		    }
		  mu_message_set_envelope (newmsg, newenv,
					   mu_message_get_owner (newmsg));
		}
	      else
		mu_diag_funcall (MU_DIAG_ERROR, "mu_envelope_create",
				   NULL, rc);
	    }
	  else
	    mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_envelope",
			     NULL, rc);
	}
      
      /* Copy headers */
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

