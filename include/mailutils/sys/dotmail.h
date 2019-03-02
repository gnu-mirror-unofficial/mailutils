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

#ifndef _MAILUTILS_SYS_DOTMAIL_H
#define _MAILUTILS_SYS_DOTMAIL_H

# include <mailutils/types.h>

enum mu_dotmail_hdr
  {
    mu_dotmail_hdr_status,
    mu_dotmail_hdr_x_imapbase,
    mu_dotmail_hdr_x_uid,
    MU_DOTMAIL_HDR_MAX
  } status;

struct mu_dotmail_message
{
  /* Offsets in the mailbox */
  mu_off_t message_start; /* Start of message */
  mu_off_t body_start;    /* Start of body */
  mu_off_t message_end;   /* End of message */
  /* Additional info */
  size_t body_size;       /* Number of octets in unstuffed message body */
  size_t body_lines;      /* Number of lines in message body */
  unsigned long uid;      /* IMAP-style uid.  */
  char *hdr[MU_DOTMAIL_HDR_MAX]; /* Pre-scanned headers */
  unsigned body_dot_stuffed:1;   /* True if body is dot-stuffed */
  unsigned headers_scanned:1;    /* True if hdr is filled */
  unsigned attr_scanned:1;       /* True if attr_flags is initialized */
  unsigned body_lines_scanned:1; /* True if body_lines is initialized */
  int attr_flags;         /* Packed "Status:" attribute flags */
  mu_message_t message;   /* Pointer to the message object if any */
  /* Backlink to the mailbox */
  struct mu_dotmail_mailbox *mbox; /* Mailbox */
  size_t num;             /* Number of this message in the mailbox (0-based) */
};

struct mu_dotmail_message_ref
{
  size_t orig_num;        /* Original message index */
  mu_off_t message_start; /* Start of message */
  mu_off_t body_start;    /* Start of body */
  mu_off_t message_end;   /* End of message */
  int rescan;
};

struct mu_dotmail_mailbox
{
  char *name;                /* Disk file name */
  mu_mailbox_t mailbox;      /* Associated mailbox */

  mu_off_t size;             /* Size of the mailbox.  */
  unsigned long uidvalidity; /* Uidvalidity value */
  unsigned uidnext;          /* Expected next UID value */
  unsigned uidvalidity_scanned:1; /* True if uidvalidity is initialized */
  size_t scanned_uids_count; /* Number of messages for which UIDs have been
				scanned */
  
  struct mu_dotmail_message **mesg; /* Array of messages */
  size_t mesg_count;       /* Number of messages in mesgv */
  size_t mesg_max;         /* Actual capacity of mesg */
};

int mu_dotmail_mailbox_init (mu_mailbox_t mailbox);
void mu_dotmail_message_free (struct mu_dotmail_message *dmsg);
int mu_dotmail_message_get (struct mu_dotmail_message *dmsg, mu_message_t *mptr);
int mu_dotmail_message_headers_prescan (struct mu_dotmail_message *dmsg);
int mu_dotmail_message_attr_load (struct mu_dotmail_message *dmsg);
int mu_dotmail_mailbox_scan_uids (mu_mailbox_t mailbox, size_t msgno);
int mu_dotmail_message_reconstruct (mu_stream_t dest,
				    struct mu_dotmail_message *dmsg,
				    struct mu_dotmail_message_ref *ref);

#endif  
