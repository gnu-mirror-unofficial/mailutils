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
#include "mailcap.h"

/*
  FIXME:
 decode, this is temporary, until the API on how to present
 mime/attachements etc is less confusing.
 */

struct decode_closure
{
  int select_hdr;
};

static int print_stream (mu_stream_t, mu_stream_t);
static int display_message (mu_message_t, msgset_t *msgset, void *closure);
static int display_submessage (struct mime_descend_closure *closure,
			       void *data);
static void get_content_encoding (mu_header_t hdr, char **value);
static void run_metamail (const char *mailcap, mu_message_t mesg);

int
mail_decode (int argc, char **argv)
{
  msgset_t *msgset;
  struct decode_closure decode_closure;

  if (msgset_parse (argc, argv, MSG_NODELETED|MSG_SILENT, &msgset))
    return 1;

  decode_closure.select_hdr = mu_islower (argv[0][0]);

  util_msgset_iterate (msgset, display_message, (void *)&decode_closure);

  msgset_free (msgset);
  return 0;
}

int
display_message (mu_message_t mesg, msgset_t *msgset, void *arg)
{
  struct decode_closure *closure = arg;
  mu_attribute_t attr = NULL;
  struct mime_descend_closure mclos;
  
  mu_message_get_attribute (mesg, &attr);
  if (mu_attribute_is_deleted (attr))
    return 1;

  mclos.hints = closure->select_hdr ? MDHINT_SELECTED_HEADERS : 0;
  mclos.msgset = msgset;
  mclos.message = mesg;
  mclos.type = NULL;
  mclos.encoding = NULL;
  mclos.parent = NULL;

  mime_descend (&mclos, display_submessage, NULL);

  /* Mark enclosing message as read */
  if (mu_mailbox_get_message (mbox, msgset_msgno (msgset), &mesg) == 0)
    util_mark_read (mesg);

  return 0;
}

static void
display_headers (mu_stream_t out, mu_message_t mesg, 
                 const msgset_t *msgset MU_ARG_UNUSED,
		 int select_hdr)
{
  mu_header_t hdr = NULL;

  /* Print the selected headers only.  */
  if (select_hdr)
    {
      size_t num = 0;
      size_t i = 0;
      const char *sptr;

      mu_message_get_header (mesg, &hdr);
      mu_header_get_field_count (hdr, &num);
      for (i = 1; i <= num; i++)
	{
	  if (mu_header_sget_field_name (hdr, i, &sptr))
	    continue;
	  if (mail_header_is_visible (sptr))
	    {
	      mu_stream_printf (out, "%s: ", sptr);
	      if (mu_header_sget_field_value (hdr, i, &sptr))
		sptr = "";
	      mu_stream_printf (out, "%s\n", sptr);
	    }
	}
      mu_stream_printf (out, "\n");
    }
  else /* Print displays all headers.  */
    {
      mu_stream_t stream = NULL;
      if (mu_message_get_header (mesg, &hdr) == 0
	  && mu_header_get_streamref (hdr, &stream) == 0)
	{
	  print_stream (stream, out);
	  mu_stream_destroy (&stream);
	}
    }
}

static void
display_part_header (mu_stream_t str, const msgset_t *msgset,
		     const char *type, const char *encoding)
{
  int size = util_screen_columns () - 3;
  unsigned int i;
  char *msp;
  
  mu_stream_printf (str, "+");
  for (i = 0; (int)i <= size; i++)
    mu_stream_printf (str, "-");
  mu_stream_printf (str, "+");
  mu_stream_printf (str, "\n");
  msp = msgset_str (msgset);
  mu_stream_printf (str, _("| Message=%s"), msp);
  free (msp);
  mu_stream_printf (str, "\n");

  mu_stream_printf (str, _("| Type=%s\n"), type);
  mu_stream_printf (str, _("| Encoding=%s\n"), encoding);
  mu_stream_printf (str, "+");
  for (i = 0; i <= size; i++)
    mu_stream_printf (str, "-");
  mu_stream_printf (str, "+");
  mu_stream_printf (str, "\n");
}

int
mime_descend (struct mime_descend_closure *closure,
	      mime_descend_fn fun, void *data)
{
  int status = 0;
  mu_header_t hdr = NULL;
  char *type;
  char *encoding;
  int ismime = 0;
  struct mime_descend_closure subclosure;

  mu_message_get_header (closure->message, &hdr);
  util_get_hdr_value (hdr, MU_HEADER_CONTENT_TYPE, &type);
  if (!type)
    type = mu_strdup ("text/plain");
  get_content_encoding (hdr, &encoding);

  closure->type = type;
  closure->encoding = encoding;
  
  subclosure.hints = 0;
  subclosure.parent = closure;
  
  mu_message_is_multipart (closure->message, &ismime);
  if (ismime)
    {
      unsigned int j;
      size_t nparts;
      
      status = mu_message_get_num_parts (closure->message, &nparts);
      if (status)
	{
	  mu_diag_funcall (MU_DIAG_ERR, "mu_message_get_num_parts", NULL,
			   status);
	}
      else
	{
	  for (j = 1; j <= nparts; j++)
	    {
	      mu_message_t message = NULL;

	      if (mu_message_get_part (closure->message, j, &message) == 0)
		{
		  msgset_t *set = msgset_expand (msgset_dup (closure->msgset),
						 msgset_make_1 (j));
		  subclosure.msgset = set;
		  subclosure.message = message;
		  status = mime_descend (&subclosure, fun, data);
		  msgset_free (set);
		  if (status)
		    break;
		}
	    }
	}
    }
  else if (mu_c_strncasecmp (type, "message/rfc822", 14) == 0)
    {
      mu_message_t submsg = NULL;

      switch (mu_message_unencapsulate (closure->message, &submsg, NULL))
	{
	case 0:
	  subclosure.hints = MDHINT_SELECTED_HEADERS;
	  subclosure.msgset = closure->msgset;
	  subclosure.message = submsg;
	  status = mime_descend (&subclosure, fun, data);
	  break;

	case MU_ERR_INVALID_EMAIL:
	  /*
	   * The mu_message_unencapsulate function returns this code
	   * if it was unable to parse the message body as a RFC822
	   * message (this code is propagated from mu_stream_to_message,
	   * which does the actual work).
	   *
	   * If the enclosing messgae(part) is of message/digest type, it
	   * is possible that messages are packed into it without MIME
	   * headers.  That violates RFC 1341, but such digests are
	   * reported to exist (re. email conversation with Karl on
	   * 2020-08-07, <202008070138.0771cfHn003390@freefriends.org>).
	   *
	   * Try to see whether the part has one of the normal RFC822 headers
	   * and if so treat it as the message.  Otherwise, treat it as
	   * text/plain.
	   * 
	   * FIXME: Do all this only if the upper-level message is of
	   * message/digest type.
	   */
	  {
	    char *names[] = { "From", "To", "Subject" };
	    mu_header_t hdr;

	    if (mu_message_get_header (closure->message, &hdr) == 0 &&
		mu_header_sget_firstof (hdr, names, NULL, NULL) == 0)
	      {
		mu_stream_t str;
		if (mu_message_get_streamref (closure->message, &str) == 0)
		  {
		    status = mu_stream_to_message (str, &submsg);
		    mu_stream_unref (str);
		    if (status == 0)
		      {
			mu_header_t subhdr;
			if (mu_message_get_header (submsg, &subhdr) == 0)
			  {
			    mu_header_remove (subhdr,
					      MU_HEADER_CONTENT_TYPE, 1);
			    subclosure.hints = MDHINT_SELECTED_HEADERS;
			    subclosure.msgset = closure->msgset;
			    subclosure.message = submsg;
			    status = mime_descend (&subclosure, fun, data);
			    break;
			  }
		      }
		  }
	      }
	  }
	  /* FALLTHROUGH */
	  
	default:
	  /* Treat as text/plain */
	  status = fun (closure, data);
	}
    }
  else
    status = fun (closure, data);

  closure->type = NULL;
  closure->encoding = NULL;
  
  free (type);
  free (encoding);

  return status;
}

static int
display_submessage (struct mime_descend_closure *closure, void *data)
{
  char *tmp;
  
  if (mailvar_get (&tmp, mailvar_name_metamail, mailvar_type_string, 0) == 0)
    {
      /* If `metamail' is set to a string, treat it as command line
	 of external metamail program. */
      run_metamail (tmp, closure->message);
    }
  else
    {
      int builtin_display = 1;
      mu_body_t body = NULL;
      mu_stream_t b_stream = NULL;
      mu_stream_t d_stream = NULL;
      mu_stream_t stream = NULL;
      mu_header_t hdr = NULL;
      
      mu_message_get_body (closure->message, &body);
      mu_message_get_header (closure->message, &hdr);
      mu_body_get_streamref (body, &b_stream);

      /* Can we decode.  */
      if (mu_filter_create (&d_stream, b_stream, closure->encoding,
			    MU_FILTER_DECODE, 
			    MU_STREAM_READ) == 0)
        {
          mu_stream_unref (b_stream);
	  stream = d_stream;
        }
      else
	stream = b_stream;
      
      display_part_header (mu_strout,
			   closure->msgset,
			   closure->type, closure->encoding);
      
      /* If `metamail' is set to true, enable internal mailcap
	 support */
      if (mailvar_is_true (mailvar_name_metamail))
	{
	  char *no_ask = NULL;
	  
	  mailvar_get (&no_ask, mailvar_name_mimenoask, mailvar_type_string, 0);
	  builtin_display = display_stream_mailcap (NULL, stream, hdr, no_ask,
						    interactive, 0,
						    MU_DEBCAT_APP);
	}
      
      if (builtin_display)
	{
	  size_t lines = 0;
	  mu_stream_t str;
	  
	  mu_message_lines (closure->message, &lines);
	  str = open_pager (lines);
	  
	  display_headers (str, closure->message, closure->msgset,
			   closure->hints & MDHINT_SELECTED_HEADERS);
	  
	  print_stream (stream, str);

	  mu_stream_unref (str);
	}
      mu_stream_destroy (&stream);
    }
  
  return 0;
}

/* FIXME: Try to use mu_stream_copy instead */
static int
print_stream (mu_stream_t stream, mu_stream_t out)
{
  char buffer[512];
  size_t n = 0;
  
  while (mu_stream_read (stream, buffer, sizeof (buffer) - 1, &n) == 0
	 && n != 0)
    {
      if (ml_got_interrupt())
	{
	  mu_error(_("\nInterrupt"));
	  break;
	}
      buffer[n] = '\0';
      mu_stream_printf (out, "%s", buffer);
    }
  return 0;
}

static void
get_content_encoding (mu_header_t hdr, char **value)
{
  char *encoding;
  util_get_hdr_value (hdr, MU_HEADER_CONTENT_TRANSFER_ENCODING, &encoding);
  if (encoding == NULL || *encoding == '\0')
    {
      if (encoding)
	free (encoding);
      encoding = mu_strdup ("7bit"); /* Default.  */
    }
  *value = encoding;
}

/* Run `metamail' program MAILCAP_CMD on the message MESG */
static void
run_metamail (const char *mailcap_cmd, mu_message_t mesg)
{
  pid_t pid;
  struct sigaction ignore;
  struct sigaction saveintr;
  struct sigaction savequit;
  sigset_t chldmask;
  sigset_t savemask;
  
  ignore.sa_handler = SIG_IGN;	/* ignore SIGINT and SIGQUIT */
  ignore.sa_flags = 0;
  sigemptyset (&ignore.sa_mask);
  if (sigaction (SIGINT, &ignore, &saveintr) < 0)
    {
      mu_error ("sigaction: %s", strerror (errno));
      return;
    }      
  if (sigaction (SIGQUIT, &ignore, &savequit) < 0)
    {
      mu_error ("sigaction: %s", strerror (errno));
      sigaction (SIGINT, &saveintr, NULL);
      return;
    }      
      
  sigemptyset (&chldmask);	/* now block SIGCHLD */
  sigaddset (&chldmask, SIGCHLD);

  if (sigprocmask (SIG_BLOCK, &chldmask, &savemask) < 0)
    {
      sigaction (SIGINT, &saveintr, NULL);
      sigaction (SIGQUIT, &savequit, NULL);
      return;
    }
  
  pid = fork ();
  if (pid < 0)
    {
      mu_error ("fork: %s", strerror (errno));
    }
  else if (pid == 0)
    {
      /* Child process */
      int status;
      mu_stream_t stream = NULL;

      do /* Fake loop to avoid gotos */
	{
	  mu_stream_t pstr;
	  char *no_ask;
	  
	  setenv ("METAMAIL_PAGER", getenv ("PAGER"), 0);
	  if (mailvar_get (&no_ask, mailvar_name_mimenoask,
			   mailvar_type_string, 0))
	    setenv ("MM_NOASK", no_ask, 1);
	  
	  status = mu_message_get_streamref (mesg, &stream);
	  if (status)
	    {
	      mu_error ("mu_message_get_streamref: %s", mu_strerror (status));
	      break;
	    }

	  status = mu_command_stream_create (&pstr, mailcap_cmd,
					     MU_STREAM_WRITE);
	  if (status)
	    {
	      mu_error ("mu_command_stream_create: %s", mu_strerror (status));
	      break;
	    }

	  mu_stream_copy (pstr, stream, 0, NULL);
	  mu_stream_close (pstr);
	  mu_stream_destroy (&pstr);
	  exit (0);
	}
      while (0);
      
      abort ();
    }
  else
    {
      int status;
      /* Master process */
      
      while (waitpid (pid, &status, 0) < 0)
	if (errno != EINTR)
	  break;
    }

  /* Restore the signal handlers */
  sigaction (SIGINT, &saveintr, NULL);
  sigaction (SIGQUIT, &savequit, NULL);
  sigprocmask (SIG_SETMASK, &savemask, NULL);
}

/* print_message_body(msg, out, stat)
 *
 * Prints the body of the message MSG to the output stream OUT.  If
 * the Content-Transfer-Encoding header is set, the body is decoded.
 * If stat is not NULL, it must point to an array of two size_t.
 * Upon return stat[0] will contain the number of bytes and stat[1]
 * the number of newline characters written to the output.
 */
int
print_message_body (mu_message_t msg, mu_stream_t out, size_t *stat)
{
  int rc;
  mu_header_t hdr;
  mu_body_t body;
  mu_stream_t d_stream;
  mu_stream_t stream;
  char *encoding = NULL;
  mu_stream_stat_buffer sb;
  
  rc = mu_message_get_header (msg, &hdr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_header",
		       NULL, rc);
      return rc;
    }
      
  rc = mu_message_get_body (msg, &body);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_body",
		       NULL, rc);
      return rc;
    }
  
  rc = mu_body_get_streamref (body, &stream);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_body_get_streamref",
		       NULL, rc);
      return rc;
    }

  util_get_hdr_value (hdr, MU_HEADER_CONTENT_TRANSFER_ENCODING, &encoding);
  if (encoding == NULL || *encoding == '\0')
    /* No need to filter */;
  else if ((rc = mu_filter_create (&d_stream, stream, encoding,
				   MU_FILTER_DECODE, MU_STREAM_READ)) == 0)
    {
      mu_stream_unref (stream);
      stream = d_stream;
    }
  else
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_body",
		       encoding, rc);
      /* FIXME: continue anyway? */
    }

  if (stat)
    {
      mu_stream_set_stat (stream,
			  MU_STREAM_STAT_MASK (MU_STREAM_STAT_IN) |
			  MU_STREAM_STAT_MASK (MU_STREAM_STAT_INLN),
			  sb);
    }
  rc = mu_stream_copy (out, stream, 0, NULL);

  mu_stream_destroy (&stream);
  free (encoding);

  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_copy",
		       encoding, rc);
    }
  else if (stat)
    {
      stat[0] = sb[MU_STREAM_STAT_IN];
      stat[1] = sb[MU_STREAM_STAT_INLN];
    }
  
  return rc;
}
