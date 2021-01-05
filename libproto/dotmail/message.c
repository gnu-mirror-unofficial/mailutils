/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019-2021 Free Software Foundation, Inc.

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
#include <mailutils/sys/dotmail.h>
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
#include <mailutils/io.h>

void
mu_dotmail_message_free (struct mu_dotmail_message *dmsg)
{
  if (dmsg)
    {
      int i;
      for (i = 0; i < MU_DOTMAIL_HDR_MAX; i++)
	free (dmsg->hdr[i]);
      mu_message_destroy (&dmsg->message, dmsg);
      free (dmsg);
    }
}

static int
_msg_stream_setup (mu_message_t msg, struct mu_dotmail_message *dmsg)
{
  mu_stream_t stream;
  int rc;

  rc = mu_streamref_create_abridged (&stream,
				     dmsg->mbox->mailbox->stream,
				     dmsg->message_start,
				     dmsg->message_end - 1);
  if (rc == 0)
    rc = mu_message_set_stream (msg, stream, dmsg);
  return rc;
}

static int
dotmail_body_size (mu_body_t body, size_t *psize)
{
  mu_message_t msg = mu_body_get_owner (body);
  struct mu_dotmail_message *dmsg = mu_message_get_owner (msg);
  if (!dmsg)
    return EINVAL;
  if (psize)
    *psize = dmsg->body_size;
  return 0;
}

static int
dotmail_body_lines (mu_body_t body, size_t *plines)
{
  mu_message_t msg = mu_body_get_owner (body);
  struct mu_dotmail_message *dmsg = mu_message_get_owner (msg);
  if (!dmsg)
    return EINVAL;
  if (!dmsg->body_lines_scanned)
    {
      int rc;
      mu_stream_t str;
      char c;
      size_t n;

      rc = mu_body_get_streamref (body, &str);
      if (rc)
	return rc;
      dmsg->body_lines = 0;
      while ((rc = mu_stream_read (str, &c, 1, &n)) == 0 && n == 1)
	{
	  if (c == '\n')
	    dmsg->body_lines++;
	}
      mu_stream_unref (str);
      if (rc)
	return rc;
      dmsg->body_lines_scanned = 1;
    }
  if (plines)
    *plines = dmsg->body_lines;
  return 0;
}

static int
_msg_body_setup (mu_message_t msg, struct mu_dotmail_message *dmsg)
{
  mu_body_t body;
  mu_stream_t stream, flt;
  int rc;

  if (dmsg->body_dot_stuffed)
    {
      rc = mu_streamref_create_abridged (&stream,
					 dmsg->mbox->mailbox->stream,
					 dmsg->body_start,
					 dmsg->message_end + 1);
      if (rc)
	return rc;

      rc = mu_filter_create (&flt, stream, "DOT",
			     MU_FILTER_DECODE, MU_STREAM_READ);
      mu_stream_unref (stream);
      if (rc)
	return rc;

      rc = mu_rdcache_stream_create (&stream, flt,
				     MU_STREAM_READ|MU_STREAM_SEEK);
      mu_stream_unref (flt);
    }
  else
    {
      rc = mu_streamref_create_abridged (&stream,
					 dmsg->mbox->mailbox->stream,
					 dmsg->body_start,
					 dmsg->message_end - 1);
    }
  if (rc)
    return rc;

  rc = mu_body_create (&body, msg);
  if (rc == 0)
    {
      mu_body_set_stream (body, stream, msg);
      mu_body_set_size (body, dotmail_body_size, msg);
      mu_body_set_lines (body, dotmail_body_lines, msg);
      mu_body_clear_modified (body);
      mu_message_set_body (msg, body, dmsg);
    }
  return rc;
}

int
mu_dotmail_message_attr_load (struct mu_dotmail_message *dmsg)
{
  if (!dmsg->attr_scanned)
    {
      if (dmsg->hdr[mu_dotmail_hdr_status])
	mu_attribute_string_to_flags (dmsg->hdr[mu_dotmail_hdr_status], 
                                      &dmsg->attr_flags);
      else
	dmsg->attr_flags = 0;
      dmsg->attr_scanned = 1;
    }
  return 0;
}

static int
dotmail_get_attr_flags (mu_attribute_t attr, int *pflags)
{
  mu_message_t msg = mu_attribute_get_owner (attr);
  struct mu_dotmail_message *dmsg = mu_message_get_owner (msg);

  if (dmsg == NULL)
    return EINVAL;
  mu_dotmail_message_attr_load (dmsg);
  if (pflags)
    *pflags = dmsg->attr_flags;
  return 0;
}

static int
dotmail_set_attr_flags (mu_attribute_t attr, int flags)
{
  mu_message_t msg = mu_attribute_get_owner (attr);
  struct mu_dotmail_message *dmsg = mu_message_get_owner (msg);

  mu_dotmail_message_attr_load (dmsg);
  dmsg->attr_flags |= flags;
  return 0;
}

static int
dotmail_unset_attr_flags (mu_attribute_t attr, int flags)
{
  mu_message_t msg = mu_attribute_get_owner (attr);
  struct mu_dotmail_message *dmsg = mu_message_get_owner (msg);

  mu_dotmail_message_attr_load (dmsg);
  dmsg->attr_flags &= ~flags;
  return 0;
}

static int
_msg_attribute_setup (mu_message_t msg, struct mu_dotmail_message *dmsg)
{
  mu_attribute_t attribute;
  int rc = mu_attribute_create (&attribute, msg);
  if (rc)
    return rc;

  mu_attribute_set_get_flags (attribute, dotmail_get_attr_flags, msg);
  mu_attribute_set_set_flags (attribute, dotmail_set_attr_flags, msg);
  mu_attribute_set_unset_flags (attribute, dotmail_unset_attr_flags, msg);
  mu_message_set_attribute (msg, attribute, dmsg);

  return 0;
}

static int
dotmail_message_uid (mu_message_t msg, size_t *puid)
{
  struct mu_dotmail_message *dmsg = mu_message_get_owner (msg);
  int rc = mu_dotmail_mailbox_uid_setup (dmsg->mbox);
  if (rc == 0)
    *puid = dmsg->uid;
  return rc;
}

static int
dotmail_message_qid (mu_message_t msg, mu_message_qid_t *pqid)
{
  struct mu_dotmail_message *dmsg = mu_message_get_owner (msg);
  return mu_asprintf (pqid, "%lu", (unsigned long) dmsg->message_start);
}

static void
dotmail_message_detach (mu_message_t msg)
{
  struct mu_dotmail_message *dmsg = mu_message_get_owner (msg);
  dmsg->message = NULL;
}  

static int
dotmail_message_setup (mu_message_t msg)
{
  struct mu_dotmail_message *dmsg = mu_message_get_owner (msg);
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
mu_dotmail_message_get (struct mu_dotmail_message *dmsg, mu_message_t *mptr)
{
  if (!dmsg->message)
    {
      mu_message_t msg;

      int rc = mu_message_create (&msg, dmsg);
      if (rc)
	return rc;

      rc = dotmail_message_setup (msg);
      if (rc)
	{
	  mu_message_destroy (&msg, dmsg);
	  return rc;
	}
      msg->_detach = dotmail_message_detach;
      
      /* Set the UID.  */
      mu_message_set_uid (msg, dotmail_message_uid, dmsg);
      mu_message_set_qid (msg, dotmail_message_qid, dmsg);

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
dotmail_message_uid_save (mu_stream_t dst,
			  struct mu_dotmail_message const *dmsg)
{
  struct mu_dotmail_mailbox *dmp = dmsg->mbox;
  if (dmp->uidvalidity_scanned)
    {
      if (dmsg->hdr[mu_dotmail_hdr_x_imapbase])
	mu_stream_printf (dst, "%s: %s\n",
			  MU_HEADER_X_IMAPBASE,
			  dmsg->hdr[mu_dotmail_hdr_x_imapbase]);
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
dotmail_message_copy_with_uid (mu_stream_t dst,
			       struct mu_dotmail_message const *dmsg,
			       struct mu_dotmail_message *ref)
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

  rc = mu_stream_header_copy (dst, src, exclude_headers);
  if (rc)
    return rc;

  rc = dotmail_message_uid_save (dst, dmsg);
  if (rc)
    return rc;

  rc = mu_stream_write (dst, "\n", 1, NULL);
  if (rc)
    return rc;

  rc = mu_stream_seek (dst, 0, MU_SEEK_CUR, &ref->body_start);
  if (rc)
    return rc;

  rc = mu_stream_copy (dst, src,
		       dmsg->message_end - dmsg->body_start + 2,
		       NULL);
  if (rc)
    return rc;

  return mu_stream_seek (dst, 0, MU_SEEK_CUR, &ref->message_end);
}

static int
msg_header_to_stream (mu_stream_t dst, mu_stream_t src,
		      struct mu_dotmail_message *dmsg)
{
  static char *exclude_headers[] = {
    MU_HEADER_STATUS,
    MU_HEADER_X_IMAPBASE,
    MU_HEADER_X_UID,
    NULL
  };
  mu_attribute_t attr;
  int rc;

  rc = mu_stream_header_copy (dst, src, exclude_headers);
  if (rc)
    return rc;

  rc = dotmail_message_uid_save (dst, dmsg);
  if (rc)
    return rc;

  rc = mu_message_get_attribute (dmsg->message, &attr);
  if (rc)
    return rc;

  free (dmsg->hdr[mu_dotmail_hdr_status]);
  dmsg->hdr[mu_dotmail_hdr_status] = malloc (MU_STATUS_BUF_SIZE);
  if (!dmsg->hdr[mu_dotmail_hdr_status])
    return ENOMEM;
  rc = mu_attribute_to_string (attr, dmsg->hdr[mu_dotmail_hdr_status],
			       MU_STATUS_BUF_SIZE, NULL);
  if (rc)
    return rc;

  mu_stream_printf (dst, "%s: %s\n",
		    MU_HEADER_STATUS,
		    dmsg->hdr[mu_dotmail_hdr_status]);

  return mu_stream_write (dst, "\n", 1, NULL);
}

/* Write to DEST a reconstructed copy of the message DMSG. Update
   the tracking reference REF.
*/
int
mu_dotmail_message_reconstruct (mu_stream_t dest,
				struct mu_dotmail_message *dmsg,
				struct mu_dotmail_message *ref)
{
  int rc;
  mu_header_t hdr;
  mu_body_t body;
  mu_stream_t str, flt;
  struct mu_dotmail_message tmp;
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
    rc = dotmail_message_copy_with_uid (dest, dmsg, ref);
  else
    {
      rc = mu_message_get_header (dmsg->message, &hdr);
      if (rc)
	return rc;
      rc = mu_header_get_streamref (hdr, &str);
      if (rc)
	return rc;
      rc = msg_header_to_stream (dest, str, dmsg);
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
      rc = mu_filter_create (&flt, str, "DOT",
			     MU_FILTER_ENCODE, MU_STREAM_READ);
      mu_stream_unref (str);
      if (rc)
	return rc;

      rc = mu_stream_copy (dest, flt, 0, NULL);
      mu_stream_unref (flt);
      if (rc == 0)
	{
	  rc = mu_stream_seek (dest, 0, MU_SEEK_CUR, &ref->message_end);
	  if (rc)
	    return rc;

	  ref->message_end -= 2;
	}
    }

  if (same_ref)
    *dmsg = tmp;
  
  return rc;
}
