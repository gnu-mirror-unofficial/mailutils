/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019 Free Software Foundation, Inc.

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

/* Status, UID, uidnext and uidvalidity */
static char *expect[] = {
  "status:    ",
  "x-imapbase:",
  "x-uid:     ",
};

static char *canon_name[] = {
  MU_HEADER_STATUS,
  MU_HEADER_X_IMAPBASE,
  MU_HEADER_X_UID
};

int
mu_dotmail_message_headers_prescan (struct mu_dotmail_message *dmsg)
{
  mu_stream_t stream;
  int rc;
  enum
  {
    prescan_state_init,
    prescan_state_expect,
    prescan_state_skip,
    prescan_state_stop
  } state = prescan_state_init;

  int i = 0;
  int j = 0;

  char cur;
  size_t n;

  if (dmsg->headers_scanned)
    return 0;

  rc = mu_streamref_create_abridged (&stream,
				     dmsg->mbox->mailbox->stream,
				     dmsg->message_start,
				     dmsg->body_start - 1);
  if (rc)
    return rc;

  rc = mu_stream_seek (stream, 0, MU_SEEK_SET, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s:%s (%s): %s",
		 __func__, "mu_stream_seek", dmsg->mbox->name,
		 mu_strerror (rc)));
      return rc;
    }

  while (state != prescan_state_stop
	 && (rc = mu_stream_read (stream, &cur, 1, &n)) == 0
	 && n == 1)
    {
      switch (state)
	{
	case prescan_state_init:
	  j = 0;
	  state = prescan_state_stop;
	  for (i = 0; i < MU_DOTMAIL_HDR_MAX; i++)
	    {
	      if (dmsg->hdr[i] == NULL)
		{
		  state = prescan_state_skip;
		  if (expect[i][j] == mu_tolower (cur))
		    {
		      j++;
		      state = prescan_state_expect;
		      break;
		    }
		}
	    }
	  break;

	case prescan_state_expect:
	  {
	    int c = mu_tolower (cur);
	    if (expect[i][j] != c)
	      {
		if (++i == MU_DOTMAIL_HDR_MAX
		    || memcmp (expect[i-1], expect[i], j)
		    || expect[i][j] != c)
		  {
		    state = prescan_state_skip;
		    break;
		  }
	      }

	    if (c == ':')
	      {
		char *buf = NULL;
		size_t size = 0;
		size_t n;

		rc = mu_stream_getline (stream, &buf, &size, &n);
		if (rc)
		  {
		    mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			      ("%s:%s (%s): %s",
			       __func__, "mu_stream_getline",
			       dmsg->mbox->name,
			       mu_strerror (rc)));
		    return rc;
		  }
		if (n > 0)
		  {
		    buf[n-1] = 0;
		    dmsg->hdr[i] = buf;
		  }
		else
		  free (buf);
		state = prescan_state_init;
	      }
	    else
	      {
		j++;
		if (expect[i][j] == 0)
		  state = prescan_state_skip;
	      }
	  }
	  break;

	case prescan_state_skip:
	  if (cur == '\n')
	    state = prescan_state_init;
	  break;

	default:
	  break; /* Should not happen */
	}
    }

  if (rc)
    {
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s:%s (%s): %s",
		 __func__, "mu_stream_read", dmsg->mbox->name,
		 mu_strerror (rc)));
      /* Try to get on with what we've got this far */
    }
  dmsg->headers_scanned = 1;
  return 0;
}

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
    *psize = dmsg->message_end - dmsg->body_start;
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
  mu_stream_t stream;
  int rc;

  rc = mu_body_create (&body, msg);
  if (rc)
    return rc;
  rc = mu_streamref_create_abridged (&stream,
				     dmsg->mbox->mailbox->stream,
				     dmsg->body_start,
				     dmsg->message_end - 1);
  if (rc)
    mu_body_destroy (&body, msg);
  else
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
      int rc = mu_dotmail_message_headers_prescan (dmsg);
      if (rc)
	return rc;
      if (dmsg->hdr[mu_dotmail_hdr_status])
	mu_string_to_flags (dmsg->hdr[mu_dotmail_hdr_status], &dmsg->attr_flags);
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
  int rc = mu_dotmail_mailbox_scan_uids (dmsg->mbox->mailbox, dmsg->num + 1);
  if (rc == 0 && puid)
    *puid = dmsg->uid;
  return rc;
}

static int
dotmail_message_qid (mu_message_t msg, mu_message_qid_t *pqid)
{
  struct mu_dotmail_message *dmsg = mu_message_get_owner (msg);
  return mu_asprintf (pqid, "%lu", (unsigned long) dmsg->message_start);
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
msg_header_to_stream (mu_stream_t dest, mu_stream_t src,
		      struct mu_dotmail_message const *dmsg)
{
  int rc;
#define LA_MAX (sizeof (expect[0]) - 1)
  char lookahead[LA_MAX];
  int la_idx = 0;
  enum
  {
    save_state_init,
    save_state_expect,
    save_state_skip,
    save_state_copy,
    save_state_stop
  } state = save_state_init;
  int i = 0;
  int j = 0;
  int hdr_saved[MU_DOTMAIL_HDR_MAX];

  memset (&hdr_saved, 0, sizeof hdr_saved);
  while (1)
    {
      char c;
      size_t n;

      rc = mu_stream_read (src, &c, 1, &n);
      if (rc)
	return rc;
      if (n == 0)
	{
	  if (la_idx)
	    {
	      rc = mu_stream_write (dest, lookahead, la_idx, NULL);
	      if (rc)
		return rc;
	    }
	  break;
	}

      if (state == save_state_init || state == save_state_expect)
	{
	  if (la_idx == LA_MAX)
	    state = save_state_copy;
	  else
	    {
	      lookahead[la_idx++] = c;
	      c = mu_tolower (c);
	    }
	}

      switch (state)
	{
	case save_state_init:
	  if (c == '\n')
	    {
	      /* End of headers. */
	      for (i = 0; i < MU_DOTMAIL_HDR_MAX; i++)
		{
		  if (!hdr_saved[i] && dmsg->hdr[i])
		    {
		      mu_stream_printf (dest, "%s: %s\n",
					canon_name[i], dmsg->hdr[i]);
		      if (mu_stream_err (dest))
			return mu_stream_last_error (dest);
		    }
		}
	      state = save_state_stop;
	      break;
	    }

	  j = 0;
	  state = save_state_copy;
	  for (i = 0; i < MU_DOTMAIL_HDR_MAX; i++)
	    {
	      if (!hdr_saved[i] && expect[i][j] == c)
		{
		  j++;
		  state = save_state_expect;
		  break;
		}
	    }
	  break;

	case save_state_expect:
	  if (expect[i][j] != c)
	    {
	      if (++i == MU_DOTMAIL_HDR_MAX
		  || memcmp (expect[i-1], expect[i], j)
		  || expect[i][j] != c)
		{
		  state = save_state_copy;
		  break;
		}
	    }

	  if (c == ':')
	    {
	      if (dmsg->hdr[i])
		{
		  rc = mu_stream_write (dest, lookahead, la_idx, NULL);
		  if (rc)
		    return rc;
		  rc = mu_stream_write (dest, dmsg->hdr[i],
					strlen (dmsg->hdr[i]), NULL);
		  if (rc)
		    return rc;
		  rc = mu_stream_write (dest, "\n", 1, NULL);
		  if (rc)
		    return rc;
		}
	      hdr_saved[i] = 1;
	      la_idx = 0;
	      state = save_state_skip;
	    }
	  else
	    {
	      j++;
	      if (expect[i][j] == 0)
		state = save_state_copy;
	    }
	  break;

	case save_state_copy:
	  if (la_idx > 0)
	    {
	      rc = mu_stream_write (dest, lookahead, la_idx, NULL);
	      if (rc)
		return rc;
	      la_idx = 0;
	    }
	  rc = mu_stream_write (dest, &c, 1, NULL);
	  if (c == '\n')
	    state = save_state_init;
	  break;

	case save_state_skip:
	  if (c == '\n')
	    state = save_state_init;
	  break;

	default:
	  abort (); /* Should not happen */
	}
    }

  return 0;
}

int
mu_dotmail_message_reconstruct (mu_stream_t dest,
				struct mu_dotmail_message *dmsg,
				struct mu_dotmail_message_ref *ref)
{
  int rc;
  mu_header_t hdr;
  mu_body_t body;
  mu_stream_t str, flt;
  mu_attribute_t attr;

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

  rc = mu_stream_seek (dest, 0, MU_SEEK_CUR, &ref->message_start);
  if (rc)
    return rc;

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
      ref->rescan = 1;
    }

  return rc;
}
