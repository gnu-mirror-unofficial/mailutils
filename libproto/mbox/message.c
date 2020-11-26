/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019-2020 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. */

#include <config.h>
#include <stdlib.h>
#include <mailutils/sys/mboxrb.h>
#include <mailutils/sys/mailbox.h>
#include <mailutils/sys/message.h>
#include <mailutils/diag.h>
#include <mailutils/errno.h>
#include <mailutils/cctype.h>
#include <mailutils/stream.h>
#include <mailutils/header.h>
#include <mailutils/body.h>
#include <mailutils/filter.h>
#include <mailutils/attribute.h>
#include <mailutils/envelope.h>
#include <mailutils/io.h>
#include <mailutils/util.h>

void
mu_mboxrb_message_free (struct mu_mboxrb_message *dmsg)
{
  if (dmsg)
    {
      mu_message_destroy (&dmsg->message, dmsg);
      free (dmsg);
    }
}

static int
_msg_stream_setup (mu_message_t msg, struct mu_mboxrb_message *dmsg)
{
  mu_stream_t stream;
  int rc;

  rc = mu_streamref_create_abridged (&stream,
				     dmsg->mbox->mailbox->stream,
				     dmsg->message_start + dmsg->from_length,
				     dmsg->message_end);
  if (rc == 0)
    rc = mu_message_set_stream (msg, stream, dmsg);
  return rc;
}

  /*

      Transitions:
      
         \  alpha
      state

            >   F   r   o   m   ' ' A  \n
	 0| 0   0   0   0   0   0   0  1
	 1| 2   0   0   0   0   0   0  1
	 2| 2   3   0   0   0   0   0  1       
	 3| 0   0   4   0   0   0   0  1
	 4| 0   0   0   5   0   0   0  1
	 5| 0   0   0   0   6   0   0  1
	 6| 0   0   0   0   0   7   0  1
	 7|

      accept: 1 7
   */
static short transitions[][256] = {
  { ['\n'] = 1 },
  { ['>'] = 2, ['\n'] = 1 },
  { ['>'] = 2, ['F'] = 3, ['\n'] = 1 },
  { ['r'] = 4, ['\n'] = 1 },
  { ['o'] = 5, ['\n'] = 1 },
  { ['m'] = 6, ['\n'] = 1 },
  { [' '] = 7, ['\n'] = 1 },
  { ['\n'] = 1 }
};

#define STATE_NL  1
#define STATE_ESC 7

static int
mboxrb_message_body_scan (struct mu_mboxrb_message *dmsg)
{
  int rc;
  mu_stream_t stream;
  char c;
  size_t n;
  size_t esc_count = 0;
  size_t body_lines = 0;
  int state = 1;

  if (dmsg->body_lines_scanned)
    return 0;
  
  rc = mu_streamref_create_abridged (&stream,
				     dmsg->mbox->mailbox->stream,
				     dmsg->body_start,
				     dmsg->message_end);
  if (rc)
    return rc;

  while ((rc = mu_stream_read (stream, &c, 1, &n)) == 0 && n == 1)
    {
      state = transitions[state][(unsigned int) (unsigned char) c];
      switch (state)
	{
	case STATE_NL:
	  body_lines++;
	  break;

	case STATE_ESC:
	  esc_count++;
	  break;
	}
    }
  mu_stream_unref (stream);
  if (rc)
    return rc;

  dmsg->body_lines = body_lines;
  dmsg->body_size = dmsg->message_end - dmsg->body_start - esc_count + 1;
  dmsg->body_from_escaped = esc_count != 0;
  
  dmsg->body_lines_scanned = 1;
  return 0;
}

static int
mboxrb_body_size (mu_body_t body, size_t *psize)
{
  int rc;
  mu_message_t msg = mu_body_get_owner (body);
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);
  if (!dmsg)
    return EINVAL;
  if ((rc = mboxrb_message_body_scan (dmsg)) != 0)
    return rc;
  if (psize)
    *psize = dmsg->body_size;
  return 0;
}

static int
mboxrb_body_lines (mu_body_t body, size_t *plines)
{
  int rc;
  mu_message_t msg = mu_body_get_owner (body);
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);
  if (!dmsg)
    return EINVAL;
  if ((rc = mboxrb_message_body_scan (dmsg)) != 0)
    return rc;
  if (plines)
    *plines = dmsg->body_lines;
  return 0;
}

static int
mboxrb_envelope_sender (mu_envelope_t env, char *buf, size_t len,
			size_t *pnwrite)
{
  int rc = 0;
  mu_message_t msg = mu_envelope_get_owner (env);
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);
  if (!dmsg)
    return EINVAL;

  if (!buf || len <= 1)
    {
      len = dmsg->env_sender_len;
    }
  else
    {
      mu_stream_t stream;
      
      rc = mu_streamref_create_abridged (&stream,
					 dmsg->mbox->mailbox->stream,
					 dmsg->message_start + 5,
					 dmsg->message_start + dmsg->from_length);
      if (rc)
	return rc;
      len--;
      if (len > dmsg->env_sender_len)
	len = dmsg->env_sender_len;
      rc = mu_stream_read (stream, buf, len, &len);
      if (rc == 0)
	{
	  buf[len] = 0;
	}
      mu_stream_destroy (&stream);
    }
  if (rc == 0)
    {
      if (pnwrite)
	*pnwrite = len;
    }
  return rc;
}	

static int
mboxrb_envelope_date (mu_envelope_t env, char *buf, size_t len,
		      size_t *pnwrite)
{
  mu_message_t msg = mu_envelope_get_owner (env);
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);

  if (!buf || len <= 1)
    {
      len = MU_DATETIME_FROM_LENGTH;
    }
  else
    {
      len = mu_cpystr (buf, dmsg->date, len);
    }
  if (pnwrite)
    *pnwrite = len;
  return 0;
}	

static int
_msg_body_setup (mu_message_t msg, struct mu_mboxrb_message *dmsg)
{
  mu_body_t body;
  mu_stream_t stream, flt;
  int rc;

  if ((rc = mboxrb_message_body_scan (dmsg)) != 0)
    return rc;

  rc = mu_streamref_create_abridged (&stream,
				     dmsg->mbox->mailbox->stream,
				     dmsg->body_start,
				     dmsg->message_end);
  if (rc)
    return rc;
  
  if (dmsg->body_from_escaped)
    {
      rc = mu_filter_create (&flt, stream, "FROMRB",
			     MU_FILTER_DECODE, MU_STREAM_READ);
      mu_stream_unref (stream);
      if (rc)
	return rc;

      rc = mu_rdcache_stream_create (&stream, flt,
				     MU_STREAM_READ|MU_STREAM_SEEK);
      mu_stream_unref (flt);

      if (rc)
	return rc;
    }
  
  rc = mu_body_create (&body, msg);
  if (rc == 0)
    {
      mu_body_set_stream (body, stream, msg);
      mu_body_set_size (body, mboxrb_body_size, msg);
      mu_body_set_lines (body, mboxrb_body_lines, msg);
      mu_body_clear_modified (body);
      mu_message_set_body (msg, body, dmsg);
    }
  return rc;
}

static int
mboxrb_get_attr_flags (mu_attribute_t attr, int *pflags)
{
  mu_message_t msg = mu_attribute_get_owner (attr);
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);

  if (dmsg == NULL)
    return EINVAL;
  if (pflags)
    *pflags = dmsg->attr_flags;
  return 0;
}

static int
mboxrb_set_attr_flags (mu_attribute_t attr, int flags)
{
  mu_message_t msg = mu_attribute_get_owner (attr);
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);

  dmsg->attr_flags |= flags;
  return 0;
}

static int
mboxrb_unset_attr_flags (mu_attribute_t attr, int flags)
{
  mu_message_t msg = mu_attribute_get_owner (attr);
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);
  dmsg->attr_flags &= ~flags;
  return 0;
}

static int
_msg_attribute_setup (mu_message_t msg, struct mu_mboxrb_message *dmsg)
{
  mu_attribute_t attribute;
  int rc = mu_attribute_create (&attribute, msg);
  if (rc)
    return rc;

  mu_attribute_set_get_flags (attribute, mboxrb_get_attr_flags, msg);
  mu_attribute_set_set_flags (attribute, mboxrb_set_attr_flags, msg);
  mu_attribute_set_unset_flags (attribute, mboxrb_unset_attr_flags, msg);
  mu_message_set_attribute (msg, attribute, dmsg);

  return 0;
}

static int
mboxrb_message_uid (mu_message_t msg, size_t *puid)
{
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);
  int rc = mu_mboxrb_mailbox_uid_setup (dmsg->mbox);
  if (rc == 0)
    *puid = dmsg->uid;
  return rc;
}

static int
mboxrb_message_qid (mu_message_t msg, mu_message_qid_t *pqid)
{
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);
  return mu_asprintf (pqid, "%lu", (unsigned long) dmsg->message_start);
}

static void
mboxrb_message_detach (mu_message_t msg)
{
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);
  dmsg->message = NULL;
}  

static int
mboxrb_message_setup (mu_message_t msg)
{
  struct mu_mboxrb_message *dmsg = mu_message_get_owner (msg);
  int rc;

  rc = _msg_stream_setup (msg, dmsg);
  if (rc)
    return rc;

  rc = _msg_body_setup (msg, dmsg);
  if (rc)
    return rc;

  rc = _msg_attribute_setup (msg, dmsg);
  if (rc)
    return rc;

  return 0;
}

int
mu_mboxrb_message_get (struct mu_mboxrb_message *dmsg, mu_message_t *mptr)
{
  if (!dmsg->message)
    {
      mu_message_t msg;
      mu_envelope_t env;
      
      int rc = mu_message_create (&msg, dmsg);
      if (rc)
	return rc;

      rc = mboxrb_message_setup (msg);
      if (rc)
	{
	  mu_message_destroy (&msg, dmsg);
	  return rc;
	}
      msg->_detach = mboxrb_message_detach;

      rc = mu_envelope_create (&env, msg);
      if (rc)
	{
	  mu_message_destroy (&msg, dmsg);
	  return rc;
	}
      mu_envelope_set_sender (env, mboxrb_envelope_sender, msg);
      mu_envelope_set_date (env, mboxrb_envelope_date, msg);
      mu_message_set_envelope (msg, env, dmsg);      
      
      /* Set the UID.  */
      mu_message_set_uid (msg, mboxrb_message_uid, dmsg);
      mu_message_set_qid (msg, mboxrb_message_qid, dmsg);

      /* Attach the message to the mailbox mbox data.  */
      dmsg->message = msg;
      mu_message_set_mailbox (msg, dmsg->mbox->mailbox, dmsg);
      mu_message_clear_modified (msg);

      dmsg->message = msg;
    }
  if (mptr)
    *mptr = dmsg->message;
  return 0;
}

static int
mboxrb_message_uid_save (mu_stream_t dst,
			 struct mu_mboxrb_message const *dmsg,
			 char const *x_imapbase)
{
  struct mu_mboxrb_mailbox *dmp = dmsg->mbox;
  if (dmp->uidvalidity_scanned)
    {
      if (x_imapbase)
	mu_stream_printf (dst, "%s: %s\n",
			  MU_HEADER_X_IMAPBASE,
			  x_imapbase);
      mu_stream_printf (dst, "%s: %lu\n",
			MU_HEADER_X_UID,
			dmsg->uid);
      return mu_stream_err (dst) ? mu_stream_last_error (dst) : 0;
    }
  return 0;
}

/* Copy message DMSG to DST replacing the UID-related information.
   The message is unchanged otherwise.
*/
int
mboxrb_message_copy_with_uid (mu_stream_t dst,
			      struct mu_mboxrb_message const *dmsg,
			      struct mu_mboxrb_message *ref,
			      char const *x_imapbase)
{
  int rc;
  mu_stream_t src;
  static char *exclude_headers[] = {
    MU_HEADER_X_IMAPBASE,
    MU_HEADER_X_UID,
    NULL
  };

  src = dmsg->mbox->mailbox->stream;

  rc = mu_stream_seek (src, dmsg->message_start, MU_SEEK_SET, NULL);
  if (rc)
    return rc;

  rc = mu_stream_copy (dst, src, dmsg->from_length, NULL);
  if (rc)
    return rc;

  rc = mu_stream_header_copy (dst, src, exclude_headers);
  if (rc)
    return rc;

  rc = mboxrb_message_uid_save (dst, dmsg, x_imapbase);
  if (rc)
    return rc;

  rc = mu_stream_write (dst, "\n", 1, NULL);
  if (rc)
    return rc;

  rc = mu_stream_seek (dst, 0, MU_SEEK_CUR, &ref->body_start);
  if (rc)
    return rc;

  rc = mu_stream_copy_nl (dst, src,
			  dmsg->message_end - dmsg->body_start + 1,
			  NULL);
  if (rc)
    return rc;

  rc = mu_stream_seek (dst, 0, MU_SEEK_CUR, &ref->message_end);
  if (rc == 0)
    ref->message_end--;
  return rc;
}

static int
msg_header_to_stream (mu_stream_t dst, mu_stream_t src,
		      struct mu_mboxrb_message const *dmsg,
		      char const *x_imapbase)
{
  static char *exclude_headers[] = {
    MU_HEADER_STATUS,
    MU_HEADER_X_IMAPBASE,
    MU_HEADER_X_UID,
    NULL
  };
  mu_attribute_t attr;
  int rc;
  char statbuf[MU_STATUS_BUF_SIZE];
    
  rc = mu_stream_header_copy (dst, src, exclude_headers);
  if (rc)
    return rc;

  rc = mboxrb_message_uid_save (dst, dmsg, x_imapbase);
  if (rc)
    return rc;

  rc = mu_message_get_attribute (dmsg->message, &attr);
  if (rc)
    return rc;

  rc = mu_attribute_to_string (attr, statbuf, MU_STATUS_BUF_SIZE, NULL);
  if (rc)
    return rc;

  if (statbuf[0])
    mu_stream_printf (dst, "%s: %s\n", MU_HEADER_STATUS, statbuf);

  return mu_stream_write (dst, "\n", 1, NULL);
}

static int
env_to_stream (struct mu_mboxrb_message const *dmsg,
	       struct mu_mboxrb_message *ref,
	       mu_envelope_t env, mu_stream_t dst)
{
  char const *sender, *date;
  int rc;

  if ((rc = mu_envelope_sget_sender (env, &sender)) == 0 &&
      (rc = mu_envelope_sget_date (env, &date)) == 0)
    {//FIXME
      mu_off_t off;
      rc = mu_stream_printf (dst, "From %s %s\n", sender, date);
      mu_stream_seek (dst, 0, MU_SEEK_CUR, &off);
      ref->from_length = off - ref->message_start;
      ref->env_sender_len = strlen (sender);
      strncpy (ref->date, date, sizeof (ref->date));
    }
  
  return rc;
}

/* Write to DEST a reconstructed copy of the message DMSG. Update
   the tracking reference REF.
*/
int
mu_mboxrb_message_reconstruct (mu_stream_t dest,
			       struct mu_mboxrb_message *dmsg,
			       struct mu_mboxrb_message *ref,
			       char const *x_imapbase)
{
  int rc;
  mu_envelope_t env;
  mu_header_t hdr;
  mu_body_t body;
  mu_stream_t str, flt;
  struct mu_mboxrb_message tmp;
  int same_ref;
  
  if ((same_ref = (ref == dmsg)) != 0)
    {
      /* Operate on temporary copy of dmsg */
      tmp = *ref;
      ref = &tmp;
    }
  rc = mu_stream_seek (dest, 0, MU_SEEK_CUR, &ref->message_start);
  if (rc)
    return rc;

  if (!dmsg->message)
    rc = mboxrb_message_copy_with_uid (dest, dmsg, ref, x_imapbase);
  else
    {
      rc = mu_message_get_envelope (dmsg->message, &env);
      if (rc)
	return rc;
      rc = env_to_stream (dmsg, ref, env, dest);
      if (rc)
	return rc;
  
      rc = mu_message_get_header (dmsg->message, &hdr);
      if (rc)
	return rc;
      rc = mu_header_get_streamref (hdr, &str);
      if (rc)
	return rc;
      rc = msg_header_to_stream (dest, str, dmsg, x_imapbase);
      mu_stream_unref (str);
      if (rc)
	return rc;

      rc = mu_stream_seek (dest, 0, MU_SEEK_CUR, &ref->body_start);
      if (rc)
	return rc;

      /* Copy body */
      rc = mu_message_get_body (dmsg->message, &body);
      if (rc)
	return rc;
      rc = mu_body_get_streamref (body, &str);
      if (rc)
	return rc;
      rc = mu_filter_create (&flt, str, "FROMRB",
			     MU_FILTER_ENCODE, MU_STREAM_READ);
      mu_stream_unref (str);
      if (rc)
	return rc;

      rc = mu_stream_copy_nl (dest, flt, 0, NULL);
      mu_stream_unref (flt);
      if (rc == 0)
	{
	  rc = mu_stream_seek (dest, 0, MU_SEEK_CUR, &ref->message_end);
	  if (rc)
	    return rc;
	  ref->message_end--;
	}
    }
  
  if (same_ref)
    *(struct mu_mboxrb_message *)dmsg = tmp;
  
  return rc;
}
