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

#include "pop3d.h"
#include "mailutils/property.h"

mu_stream_t iostream;

void
pop3d_parse_command (char *cmd, char **pcmd, char **parg)
{
  char *p;
  
  cmd = mu_str_skip_class (cmd, MU_CTYPE_BLANK);
  *pcmd = cmd;
  p = mu_str_skip_class_comp (cmd, MU_CTYPE_SPACE);
  *p++ = 0;
  if (*p)
    {
      *parg = p;
      mu_rtrim_class (p, MU_CTYPE_SPACE);
    }
  else
    *parg = "";
}

/* This is called if GNU POP3 needs to quit without going to the UPDATE stage.
   This is used for conditions such as out of memory, a broken socket, or
   being killed on a signal */
int
pop3d_abquit (int reason)
{
  int code;
  
  /* Unlock spool */
  if (state != AUTHORIZATION)
    {
      mu_mailbox_flush (mbox, 0);
      mu_mailbox_close (mbox);
      manlock_unlock (mbox);
      mu_mailbox_destroy (&mbox);
    }

  switch (reason)
    {
    case ERR_NO_MEM:
      code = EX_SOFTWARE;
      pop3d_outf ("-ERR %s\n", pop3d_error_string (reason));
      mu_diag_output (MU_DIAG_ERROR, _("not enough memory"));
      break;

    case ERR_SIGNAL:
      code = EX_SOFTWARE;
      mu_diag_output (MU_DIAG_ERROR, _("quitting on signal"));
      break;

    case ERR_TERMINATE:
      code = EX_OK;
      mu_diag_output (MU_DIAG_NOTICE, _("terminating on request"));
      break;

    case ERR_TIMEOUT:
      code = EX_TEMPFAIL;
      mu_stream_clearerr (iostream);
      pop3d_outf ("-ERR %s\n", pop3d_error_string (reason));
      if (state == TRANSACTION)
	mu_diag_output (MU_DIAG_INFO, _("session timed out for user: %s"),
			username);
      else
	mu_diag_output (MU_DIAG_INFO, _("session timed out for no user"));
      break;

    case ERR_NO_IFILE:
      code = EX_NOINPUT;
      mu_diag_output (MU_DIAG_INFO, _("no input stream"));
      break;

    case ERR_NO_OFILE:
      code = EX_IOERR;
      mu_diag_output (MU_DIAG_INFO, _("no socket to send to"));
      break;

    case ERR_FILE:
      code = EX_IOERR;
      break;
      
    case ERR_PROTO:
      code = EX_PROTOCOL;
      mu_diag_output (MU_DIAG_INFO, _("remote protocol error"));
      break;

    case ERR_IO:
      code = EX_IOERR;
      mu_diag_output (MU_DIAG_INFO, _("I/O error"));
      break;

    case ERR_MBOX_SYNC:
      code = EX_OSERR; /* FIXME: This could be EX_SOFTWARE as well? */
      mu_diag_output (MU_DIAG_ERROR,
		      _("mailbox was updated by other party: %s"),
		      username);
      pop3d_outf ("-ERR [OUT-SYNC] Mailbox updated by other party or corrupted\n");
      break;

    default:
      code = EX_SOFTWARE;
      pop3d_outf ("-ERR Quitting: %s\n", pop3d_error_string (reason));
      mu_diag_output (MU_DIAG_ERROR, _("quitting (numeric reason %d)"),
		      reason);
      break;
    }

  closelog ();
  exit (code);
}

static void
log_cipher (mu_stream_t stream)
{
  mu_property_t prop;
  int rc = mu_stream_ioctl (stream, MU_IOCTL_TLSSTREAM,
			    MU_IOCTL_TLS_GET_CIPHER_INFO, &prop);
  if (rc)
    {
      mu_diag_output (MU_DIAG_INFO, _("TLS established"));
      mu_diag_output (MU_DIAG_ERROR, _("can't get TLS details: %s"),
		      mu_strerror (rc));
    }
  else
    {
      char const *cipher, *mac, *proto;
      if (mu_property_sget_value (prop, "cipher", &cipher))
	cipher = "UNKNOWN";	
      if (mu_property_sget_value (prop, "mac", &mac))
	mac = "UNKNOWN";
      if (mu_property_sget_value (prop, "protocol", &proto))
	proto = "UNKNOWN";
      
      mu_diag_output (MU_DIAG_INFO, _("TLS established using %s-%s (%s)"),
		      cipher, mac, proto);
      
      mu_property_destroy (&prop);
    }
}

void
pop3d_setio (int ifd, int ofd, struct mu_tls_config *tls_conf)
{
  mu_stream_t str, istream, ostream;
  int rc;
  
  if (ifd == -1)
    pop3d_abquit (ERR_NO_IFILE);
  if (ofd == -1)
    pop3d_abquit (ERR_NO_OFILE);

  if (tls_conf)
    {
      rc = mu_tlsfd_stream_create (&str, ifd, ofd, tls_conf, MU_TLS_SERVER, 0);
      if (rc)
	{
	  mu_error (_("failed to create TLS stream: %s"), mu_strerror (rc));
	  pop3d_abquit (ERR_FILE);
	}
      log_cipher (str);
    }
  else
    {
      if (mu_stdio_stream_create (&istream, ifd, MU_STREAM_READ))
	pop3d_abquit (ERR_FILE);
      mu_stream_set_buffer (istream, mu_buffer_line, 0);
  
      if (mu_stdio_stream_create (&ostream, ofd, MU_STREAM_WRITE))
	pop3d_abquit (ERR_FILE);
      mu_stream_set_buffer (ostream, mu_buffer_line, 0);
  
      /* Combine the two streams into an I/O one. */
      if (mu_iostream_create (&str, istream, ostream))
	pop3d_abquit (ERR_FILE);

      mu_stream_unref (istream);
      mu_stream_unref (ostream);
    }
  
  /* Convert all writes to CRLF form.
     There is no need to convert reads, as the code ignores extra \r anyway.
  */
  rc = mu_filter_create (&iostream, str, "CRLF", MU_FILTER_ENCODE,
			 MU_STREAM_WRITE | MU_STREAM_RDTHRU);
  mu_stream_unref (str);
  if (rc)
    pop3d_abquit (ERR_FILE);
  /* Change buffering scheme: filter streams are fully buffered by default. */
  mu_stream_set_buffer (iostream, mu_buffer_line, 0);
  
  if (pop3d_transcript)
    {
      mu_stream_t dstr, xstr;
      
      rc = mu_dbgstream_create (&dstr, MU_DIAG_DEBUG);
      if (rc)
	mu_error (_("cannot create debug stream; transcript disabled: %s"),
		  mu_strerror (rc));
      else
	{
	  rc = mu_xscript_stream_create (&xstr, iostream, dstr, NULL);
	  mu_stream_unref (dstr);
	  if (rc)
	    mu_error (_("cannot create transcript stream: %s"),
		      mu_strerror (rc));
	  else
	    {
	      mu_stream_unref (iostream);
	      iostream = xstr;
	    }
	}
    }
}

int
pop3d_init_tls_server (struct mu_tls_config *tls_conf)
{
  mu_stream_t tlsstream, stream[2], tstr, istr;
  mu_transport_t t[2];
  int ifd, ofd;
  int rc;

  rc = mu_stream_ioctl (iostream, MU_IOCTL_SUBSTREAM, MU_IOCTL_OP_GET, stream);
  if (rc)
    {
      mu_error (_("%s failed: %s"), "MU_IOCTL_SUBSTREAM",
		mu_stream_strerror (iostream, rc));
      return 1;
    }
  
  rc = mu_stream_ioctl (stream[MU_TRANSPORT_INPUT], MU_IOCTL_TRANSPORT,
			MU_IOCTL_OP_GET, t);
  if (rc)
    {
      mu_error (_("%s failed: %s"), "MU_IOCTL_TRANSPORT",
		mu_stream_strerror (iostream, rc));
      return 1;
    }
  ifd = (int) (intptr_t) t[0];

  rc = mu_stream_ioctl (stream[MU_TRANSPORT_OUTPUT], MU_IOCTL_TRANSPORT,
			MU_IOCTL_OP_GET, t);
  if (rc)
    {
      mu_error (_("%s failed: %s"), "MU_IOCTL_TRANSPORT",
		mu_stream_strerror (iostream, rc));
      return 1;
    }
  ofd = (int) (intptr_t) t[0];

  rc = mu_tlsfd_stream_create (&tlsstream, ifd, ofd,
			       tls_conf,
			       MU_TLS_SERVER,
			       0);

  if (rc)
    {
      mu_diag_output (MU_DIAG_ERROR, _("cannot open TLS stream: %s"),
		      mu_strerror (rc));
      return 1;
    }

  log_cipher (tlsstream);

  t[0] = (mu_transport_t) -1;
  mu_stream_ioctl (stream[MU_TRANSPORT_INPUT], MU_IOCTL_TRANSPORT,
		   MU_IOCTL_OP_SET, t);
  t[0] = (mu_transport_t) -1;
  mu_stream_ioctl (stream[MU_TRANSPORT_OUTPUT], MU_IOCTL_TRANSPORT,
		   MU_IOCTL_OP_SET, t);
  
  mu_stream_unref (stream[0]);
  mu_stream_unref (stream[1]);

  /*
   * Find the iostream and replace it with the TLS stream.
   * Unless transcript is enabled the iostream variable refers to a
   * CRLF filter, and its sub-stream is the iostream object.  If transcript
   * is enabled, the treanscript stream is added on top and iostream refers
   * to it.
   *
   * The loop below uses the fact that iostream is the only stream in
   * mailutils that returns *both* transport streams on MU_IOCTL_TOPSTREAM/
   * MU_IOCTL_OP_GET ioctl.  Rest of streams that support MU_IOCTL_TOPSTREAM,
   * return the transport stream in stream[0] and NULL in stream[1].
   */
  tstr = NULL;
  istr = iostream;
  while ((rc = mu_stream_ioctl (istr,
				MU_IOCTL_TOPSTREAM, MU_IOCTL_OP_GET,
				stream)) == 0
	 && stream[1] == NULL)
    {
      tstr = istr;
      istr = stream[0];
      mu_stream_unref (istr);
    }
  
  if (rc)
    {
      mu_error ("%s", _("INTERNAL ERROR: cannot locate iostream"));
      return 1;
    }

  mu_stream_unref (stream[0]);
  mu_stream_unref (stream[1]);
  
  stream[0] = tlsstream;
  stream[1] = NULL;
  rc = mu_stream_ioctl (tstr, MU_IOCTL_TOPSTREAM, MU_IOCTL_OP_SET, stream);
  if (rc)
    {
      mu_error (_("INTERNAL ERROR: failed to install TLS stream: %s"),
		mu_strerror (rc));
      return 1;
    }
  mu_stream_unref (tlsstream);
  
  return 0;
}

void
pop3d_bye ()
{
  mu_stream_close (iostream);
  mu_stream_destroy (&iostream);
}

void
pop3d_flush_output ()
{
  mu_stream_flush (iostream);
}

int
pop3d_is_master ()
{
  return iostream == NULL;
}

void
pop3d_outf (const char *fmt, ...)
{
  va_list ap;
  int rc;
  
  va_start (ap, fmt);
  rc = mu_stream_vprintf (iostream, fmt, ap);
  va_end (ap);
  if (rc)
    {
      mu_diag_output (MU_DIAG_ERROR, _("Write failed: %s"),
 		      mu_stream_strerror (iostream, rc));
      pop3d_abquit (ERR_IO);
    }
}

/* Gets a line of input from the client, caller should free() */
char *
pop3d_readline (char *buffer, size_t size)
{
  int rc;
  size_t nbytes;
  struct timeval tv, *to;

  if (idle_timeout)
    {
      tv.tv_sec = idle_timeout;
      tv.tv_usec = 0;
      to = &tv;
    }
  else
    to = NULL;
  
  rc = mu_stream_timed_readline (iostream, buffer, size, to, &nbytes);

  if (rc == MU_ERR_TIMEOUT)
    {
      mu_diag_output (MU_DIAG_ERROR, "%s", _("Read time out"));
      mu_stream_clearerr (iostream);
      pop3d_abquit (ERR_TIMEOUT);
    }
  else if (rc)
    {
      mu_diag_output (MU_DIAG_ERROR, _("Read failed: %s"),
 		      mu_stream_strerror (iostream, rc));
      pop3d_abquit (ERR_IO);
    }
  else if (nbytes == 0)
    {
      /* After a failed authorization attempt many clients simply disconnect
	 without issuing QUIT. We do not count this as a protocol error. */
      if (state == AUTHORIZATION)
	exit (EX_OK);

      mu_diag_output (MU_DIAG_ERROR, _("Unexpected eof on input"));
      pop3d_abquit (ERR_PROTO);
    }

  return buffer;
}


/* Handling of the deletion marks */

void
pop3d_mark_deleted (mu_attribute_t attr)
{
  mu_attribute_set_userflag (attr, POP3_ATTRIBUTE_DELE);
}

int
pop3d_is_deleted (mu_attribute_t attr)
{
  return mu_attribute_is_deleted (attr)
         || mu_attribute_is_userflag (attr, POP3_ATTRIBUTE_DELE);
}

void
pop3d_unset_deleted (mu_attribute_t attr)
{
  if (mu_attribute_is_userflag (attr, POP3_ATTRIBUTE_DELE))
    mu_attribute_unset_userflag (attr, POP3_ATTRIBUTE_DELE);
}

void
pop3d_undelete_all ()
{
  size_t i;
  size_t total = 0;

  mu_mailbox_messages_count (mbox, &total);

  for (i = 1; i <= total; i++)
    {
       mu_message_t msg = NULL;
       mu_attribute_t attr = NULL;
       mu_mailbox_get_message (mbox, i, &msg);
       mu_message_get_attribute (msg, &attr);
       mu_attribute_unset_deleted (attr);
    }
}

int
set_xscript_level (int xlev)
{
  if (pop3d_transcript)
    {
      if (xlev != MU_XSCRIPT_NORMAL)
	{
	  if (mu_debug_level_p (MU_DEBCAT_REMOTE,
	                        xlev == MU_XSCRIPT_SECURE ?
	                            MU_DEBUG_TRACE6 : MU_DEBUG_TRACE7))
	    return MU_XSCRIPT_NORMAL;
	}

      if (mu_stream_ioctl (iostream, MU_IOCTL_XSCRIPTSTREAM,
                           MU_IOCTL_XSCRIPTSTREAM_LEVEL, &xlev) == 0)
	return xlev;
    }
  return 0;
}
