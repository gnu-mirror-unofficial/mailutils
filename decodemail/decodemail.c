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
#include <sys/time.h>
#include <sys/resource.h>

int truncate_opt;
int from_filter;
int recode_charset;
char *charset;
int fd_err;

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

static mu_message_t message_decode (mu_message_t, mu_coord_t *, size_t);

static void message_store_mbox (mu_message_t, mu_mailbox_t);
static void message_store_stdout (mu_message_t, mu_mailbox_t);

static void
enable_log_prefix (int on)
{
  int mode;
  
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
		   MU_IOCTL_LOGSTREAM_GET_MODE, &mode);
  if (on)
    mode |= MU_LOGMODE_PREFIX;
  else
    mode &= ~MU_LOGMODE_PREFIX;
  
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
		   MU_IOCTL_LOGSTREAM_SET_MODE, &mode);
}

static void
set_log_prefix (mu_coord_t crd, size_t dim)
{
  char *prefix = mu_coord_part_string (crd, dim);
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
		   MU_IOCTL_LOGSTREAM_SET_PREFIX, prefix);
  free (prefix);
}

void
abend (int code)
{
  if (fd_err)
    {
      struct rlimit rlim;
      
      getrlimit (RLIMIT_NOFILE, &rlim);
      rlim.rlim_cur += fd_err;
      mu_error (_("at least %lu file descriptors are needed to process this message"),
		(unsigned long) rlim.rlim_cur);
    }
  exit (code);
}

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
  mu_coord_t crd;
  
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
      abend (EX_OSERR);
    }

  rc = mu_mailbox_open (imbox, MU_STREAM_READ);
  if (rc)
    {
      mu_url_t url = NULL;

      mu_mailbox_get_url (imbox, &url);
      mu_error (_("could not open input mailbox `%s': %s"),
		mu_url_to_string (url), mu_strerror (rc));
      abend (EX_NOINPUT);
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
	  abend (EX_OSERR);
	}
      rc = mu_mailbox_open (ombox, MU_STREAM_RDWR|MU_STREAM_CREAT);
      if (rc)
	{
	  mu_error (_("could not open mailbox `%s': %s"),
		    ombox_name, mu_strerror (rc));
	  abend (EX_CANTCREAT);
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
      abend (EX_SOFTWARE);
    }

  rc = mu_coord_alloc (&crd, 1);
  if (rc)
    mu_alloc_die ();

  enable_log_prefix (1);
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
      crd[1] = i;
      fd_err = 0;
      newmsg = message_decode (msg, &crd, 1);
      message_store (newmsg, ombox);
      mu_message_unref (newmsg);
      mu_message_unref (msg);
    }
  enable_log_prefix (0);
  
  mu_mailbox_destroy (&imbox);
  (truncate_opt ? mu_mailbox_expunge : mu_mailbox_sync) (ombox);
  mu_mailbox_destroy (&ombox);

  if (err)
    abend (EX_UNAVAILABLE);
  exit (EX_OK);
}

static void
message_store_mbox (mu_message_t msg, mu_mailbox_t mbx)
{
  int rc = mu_mailbox_append_message (mbx, msg);
  if (rc)
    {
      mu_error (_("cannot store message: %s"), mu_strerror (rc));
      switch (rc)
	{
	case MU_ERR_INVALID_EMAIL:
	case MU_ERR_EMPTY_ADDRESS:
	  break;

	case EMFILE:
	  fd_err++;
	  /* FALLTHROUGH */
	  
	default:
	  abend (EX_IOERR);
	}
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
message_store_stdout (mu_message_t msg, mu_mailbox_t mbx)
{
  mu_stream_t str;

  env_print (msg);
  mu_message_get_streamref (msg, &str);
  mu_stream_copy_nl (mu_strout, str, 0, NULL);
  mu_stream_destroy (&str);  
  mu_printf ("\n");
}

static inline int
is_address_header (char const *name)
{
  return !mu_c_strcasecmp (name, MU_HEADER_FROM) ||
    !mu_c_strcasecmp (name, MU_HEADER_TO) ||
    !mu_c_strcasecmp (name, MU_HEADER_CC) ||
    !mu_c_strcasecmp (name, MU_HEADER_BCC);
}

static int
qstring_needed (char const *s)
{
  for (; *s; s++)
    {
      if (mu_isascii (*s) && !mu_istspec (*s))
	continue;
      return 1;
    }
  return 0;
}

static void
qstring_format (mu_stream_t stream, char const *s)
{
  if (!s)
    return;
  if (qstring_needed (s))
    {
      char const *cp;
  
      mu_stream_write (stream, "\"", 1, NULL);
      while (*(cp = mu_str_skip_cset_comp (s, "\\\"")))
	{
	  mu_stream_write (stream, s, cp - s, NULL);
	  mu_stream_write (stream, "\\", 1, NULL);
	  mu_stream_write (stream, cp, 1, NULL);
	  s = cp + 1;
	}
      if (*s)
	mu_stream_write (stream, s, strlen (s), NULL);
      mu_stream_write (stream, "\"", 1, NULL);
    }
  else
    mu_stream_write (stream, s, strlen (s), NULL);
}
  
static int
address_decode (char const *name, char const *value, char const *charset,
		mu_header_t newhdr)
{
  int rc;
  mu_address_t addr;
  mu_stream_t mstr;
  mu_transport_t trans[2];
  
  rc = mu_memory_stream_create (&mstr, MU_STREAM_RDWR);
  if (rc)
    return rc;
  
  rc = mu_address_create (&addr, value);
  if (rc == 0)
    {
      mu_address_t cur;
      for (cur = addr; cur; cur = cur->next)
	{
	  char *s;
	  
	  rc = mu_rfc2047_decode (charset, cur->personal, &s);
	  if (rc == 0)
	    {
	      qstring_format (mstr, s);
	      free (s);
	    }
	  else
	    qstring_format (mstr, cur->personal);
	  mu_stream_printf (mstr, " <%s>", cur->email);
	  if (cur->next)
	    mu_stream_write (mstr, ", ", 2, NULL);
	}
      mu_stream_write (mstr, "", 1, NULL);
      rc = mu_stream_err (mstr);
      if (rc == 0)
	{
	  mu_stream_ioctl (mstr, MU_IOCTL_TRANSPORT,
			   MU_IOCTL_OP_GET,
			   trans);
	  mu_header_append (newhdr, name, (char*)trans[0]);
	}
      mu_stream_destroy (&mstr);
      mu_address_destroy (&addr);
    }
  return rc;
}

/*
 * Decode a single message or message part.
 *
 * Arguments:
 *   msg  -  Message or message part.
 *   crd  -  Pointer to mu_coord_t object that keeps its location.
 *   dim  -  Number of significant positions in crd.  If it is 1,
 *           msg is the message.  If it is greater than 1, msg is
 *           part of a MIME message.
 *
 * The function can reallocate crd to increase its actual dimension.
 * It can modify the coordinate positions starting from dim+1 (inclusive).
 */
static mu_message_t
message_decode_nomime (mu_message_t msg)
{
  mu_message_t newmsg;
  int rc;
  mu_stream_t str;
  mu_body_t body;
  mu_stream_t bstr;
  mu_header_t hdr, newhdr;
  mu_iterator_t itr;
  size_t i;
  char *content_type = NULL;
  mu_stream_stat_buffer stat;
      
  rc = message_body_stream (msg, from_filter, charset, &str);
  if (rc)
    return NULL;
  
  rc = mu_message_create (&newmsg, NULL);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_create", NULL, rc);
      abend (EX_OSERR);
    }
	  
  rc = mu_message_get_body (newmsg, &body);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_body", NULL, rc);
      goto end;
    }
	  
  rc = mu_body_get_streamref (body, &bstr);
  if (rc)
    {
      if (rc == EMFILE)
	fd_err++;
      mu_diag_funcall (MU_DIAG_ERROR, "mu_body_get_streamref", NULL, rc);
      goto end;
    }
	      
  mu_stream_set_stat (bstr,
		      MU_STREAM_STAT_MASK (MU_STREAM_STAT_IN8BIT),
		      stat);
  rc = mu_stream_copy (bstr, str, 0, NULL);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_copy", NULL, rc);
      if (mu_stream_err (bstr))
	{
	  abend (EX_IOERR);
	}
      else
	{
	  mu_stream_printf (bstr,
			    "\n[decodemail: content decoding failed: %s]\n",
			    mu_strerror (rc));
	}
    }
  mu_stream_unref (bstr);
  mu_stream_unref (str);

  rc = mu_message_get_header (msg, &hdr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_header", "msg", rc);
      goto end;
    }

  rc = mu_message_get_header (newmsg, &newhdr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_header", "newmsg", rc);
      goto end;
    }
      
  rc = mu_header_get_iterator (hdr, &itr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_header_get_iterator", NULL, rc);
      goto end;
    }

  for (mu_iterator_first (itr), i = 1; !mu_iterator_is_done (itr);
       mu_iterator_next (itr), i++)
    {
      const char *name;
      const char *value;
      char *s;
      
      rc = mu_iterator_current_kv (itr, (void const **) &name,
				   (void**)&value);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_iterator_current_kv", NULL, rc);
	  continue;
	}
      
      if (!mu_c_strcasecmp (name, MU_HEADER_CONTENT_TYPE))
	{
	  if (charset)
	    {
	      mu_content_type_t ct;
	      struct mu_mime_param **pparam;
	      char *vc = mu_strdup (value);
	      size_t len;
	      mu_string_unfold (vc, &len);
	      rc = mu_content_type_parse_ext (vc, NULL,
					      MU_CONTENT_TYPE_RELAXED |
					      MU_CONTENT_TYPE_PARAM,
					      &ct);
	      if (rc)
		{
		  mu_diag_funcall (MU_DIAG_ERROR, 
				   "mu_content_type_parse_ext",
				   vc, rc);
		  free (vc);
		  continue;
		}
	      free (vc);
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
				   "mu_assoc_install_ref",
				   NULL, rc);
		  abend (EX_IOERR);
		}
	      (*pparam)->value = mu_strdup (charset);
	      mu_content_type_format (ct, &content_type);
	      mu_content_type_destroy (&ct);
	      continue;
	    }
	}
      else if (!mu_c_strcasecmp (name, MU_HEADER_CONTENT_TRANSFER_ENCODING))
	continue;
      else if (is_address_header (name))
	{
	  if (address_decode (name, value, charset, newhdr))
	    mu_header_append (newhdr, name, value);
	  continue;
	}
	      
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
  rc = 0;
  
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
 end:
  if (rc)
    {
      mu_message_unref (newmsg);
      newmsg = NULL;
    }
  return newmsg;
}

static mu_message_t
message_decode_mime (mu_message_t msg, mu_coord_t *crd, size_t dim)
{
  int rc;
  mu_message_t newmsg;
  size_t nparts, i;
  mu_mime_t mime;
  mu_header_t hdr, newhdr;
  mu_iterator_t itr;
  char *s;
  mu_content_type_t ct;
      
  /* FIXME: The following could be simplified if we could obtain
     a mime object from the message */
  rc = mu_message_get_header (msg, &hdr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_header", "msg", rc);
      return NULL;
    }
  
  rc = mu_header_aget_value_unfold (hdr, MU_HEADER_CONTENT_TYPE, &s);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_header_aget_value_unfold",
		       MU_HEADER_CONTENT_TYPE, rc);
      return NULL;
    }

  rc = mu_content_type_parse_ext (s, NULL,
				  MU_CONTENT_TYPE_RELAXED |
				      MU_CONTENT_TYPE_PARAM, &ct);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_content_type_parse_ext", s, rc);
      free (s);
      return NULL;
    }
  free (s);

  if (!ct->subtype)
    {
      mu_content_type_destroy (&ct);
      return NULL;
    }
      
  rc = mu_mime_create_multipart (&mime, ct->subtype, ct->param);
  mu_content_type_destroy (&ct);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_mime_create_multipart", NULL, rc);
      return NULL;
    }
  
  rc = mu_message_get_num_parts (msg, &nparts);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_num_parts",
		       NULL, rc);
      return NULL;
    }

  ++dim;
  if (dim > mu_coord_length (*crd))
    {
      rc = mu_coord_realloc (crd, dim);
      if (rc)
	mu_alloc_die ();
    }

  for (i = 1; i <= nparts; i++)
    {
      mu_message_t msgpart, msgdec;
	  
      (*crd)[dim] = i;
      rc = mu_message_get_part (msg, i, &msgpart);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_part",
			   NULL, rc);
	  mu_mime_unref (mime);
	  return NULL;
	}
      msgdec = message_decode (msgpart, crd, dim);
      rc = mu_mime_add_part (mime, msgdec);
      mu_message_unref (msgdec);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_mime_add_part", NULL, rc);
	  mu_mime_unref (mime);
	  return NULL;
	}	  
    }

  --dim;
      
  rc = mu_mime_to_message (mime, &newmsg);
  mu_mime_unref (mime);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_mime_to_message", NULL, rc);
      return NULL;
    }
      
  /* Copy headers */
  rc = mu_message_get_header (newmsg, &newhdr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_header", "newmsg", rc);
      goto end;
    }
  
  rc = mu_header_get_iterator (hdr, &itr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_header_get_iterator", NULL, rc);
      goto end;
    }
      
  for (mu_iterator_first (itr), i = 1; !mu_iterator_is_done (itr);
       mu_iterator_next (itr), i++)
    {
      const char *name;
      const char *value;
      char *s;
      
      rc = mu_iterator_current_kv (itr, (void const **) &name,
				   (void**)&value);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_iterator_current_kv", NULL, rc);
	  continue;
	}

      if (mu_c_strcasecmp (name, MU_HEADER_MIME_VERSION) == 0 ||
	  mu_c_strcasecmp (name, MU_HEADER_CONTENT_TYPE) == 0)
	continue;
      else if (is_address_header (name))
	{
	  if (address_decode (name, value, charset, newhdr))
	    mu_header_append (newhdr, name, value);
	  continue;
	}
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
  rc = 0;
  
 end:
  if (rc)
    {
      mu_message_unref (newmsg);
      newmsg = NULL;
    }
  return newmsg;  
}

static mu_message_t
message_decode (mu_message_t msg, mu_coord_t *crd, size_t dim)
{
  mu_message_t newmsg;
  int ismime = 0;
  int rc;

  set_log_prefix (*crd, dim);
  
  rc = mu_message_is_multipart (msg, &ismime);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_is_multipart", NULL, rc);
      newmsg = NULL;
    }
  else if (!ismime)
    {
      newmsg = message_decode_nomime (msg);
    }
  else
    {
      newmsg = message_decode_mime (msg, crd, dim);
    }
  
  if (!newmsg)
    {
      mu_message_ref (msg);
      return msg;
    }

  set_log_prefix (*crd, dim);

  if (dim == 1)
    {
      /* Copy envelope */
      mu_envelope_t env, newenv;

      rc = mu_message_get_envelope (msg, &env);
      if (rc == 0)
	{
	  char *sender = NULL, *date = NULL;
	  if ((rc = mu_envelope_aget_sender (env, &sender)) != 0)
	    {
	      mu_diag_funcall (MU_DIAG_ERROR, "mu_envelope_aget_sender",
			      NULL, rc);
	    }
	  else if ((rc = mu_envelope_aget_date (env, &date)) != 0)
	    {
	      free (sender);
	      sender = NULL;
	      mu_diag_funcall (MU_DIAG_ERROR, "mu_envelope_aget_date",
			       NULL, rc);
	    }
	  
	  if (sender)
	    {
	      if ((rc = mu_envelope_create (&newenv, newmsg)) == 0)
		{
		  newenv->sender = sender;
		  newenv->date = date;
		  mu_message_set_envelope (newmsg, newenv,
					   mu_message_get_owner (newmsg));
		}
	      else
		{
		  free (sender);
		  free (date);
		  mu_diag_funcall (MU_DIAG_ERROR, "mu_envelope_create",
				   NULL, rc);
		}
	    }
	}
      else
	mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_envelope",
			 NULL, rc);
    }
  
  return newmsg;
}

