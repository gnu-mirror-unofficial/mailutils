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

#ifndef _MAILUTILS_SYS_MBOX2_H
#define _MAILUTILS_SYS_MBOX2_H

# include <mailutils/types.h>
# include <mailutils/datetime.h>

struct mu_mboxrb_message
{
  /* Offsets in the mailbox */
  mu_off_t message_start; /* Start of message */
  size_t from_length;     /* Length of the From_ line */
  int env_sender_len;     /* Length of the sender email in the From_ line */
  mu_off_t body_start;    /* Start of body */
  mu_off_t message_end;   /* Offset of the last byte of the message */
  /* Additional info */
  unsigned long uid;      /* IMAP-style uid.  */

  char date[MU_DATETIME_FROM_LENGTH+1]; /* Envelope date in normalized form */
  
  unsigned body_lines_scanned:1; /* True if body_lines is initialized */
  unsigned body_from_escaped:1;  /* True if body is from-escaped (valid if
				    body_lines_scanned is true) */
  unsigned uid_modified:1;/* UID|uidvalidity|uidnext has been modified */
  unsigned mark:1;
  
  int attr_flags;         /* Packed "Status:" attribute flags */

  /* The following two are set only if body_lines_scanned is true */
  size_t body_size;       /* Number of octets in message body
			     (after >From unescape) */
  size_t body_lines;      /* Number of lines in message body */
  mu_message_t message;   /* Pointer to the message object if any */
  /* Backlink to the mailbox */
  struct mu_mboxrb_mailbox *mbox; /* Mailbox */
  size_t num;             /* Number of this message in the mailbox (0-based) */  
};

struct mu_mboxrb_mailbox
{
  char *name;                /* Disk file name */
  mu_mailbox_t mailbox;      /* Associated mailbox */
  int stream_flags;          /* Flags used to create the mailbox stream */
  
  mu_off_t size;             /* Size of the mailbox.  */
  unsigned long uidvalidity; /* Uidvalidity value */
  unsigned long uidnext;     /* Expected next UID value */
  unsigned uidvalidity_scanned:1; /* True if uidvalidity is initialized */
  unsigned uidvalidity_changed:1; /* True if uidvalidity or uidnext has changed */

  size_t x_imapbase_off;   /* Offset of the X-IMAPbase header */ 
  size_t x_imapbase_len;   /* Length if the header without trailing \n */
  
  struct mu_mboxrb_message **mesg; /* Array of messages */
  size_t mesg_count;       /* Number of messages in mesgv */
  size_t mesg_max;         /* Actual capacity of mesg */
};

int mu_mboxrb_mailbox_init (mu_mailbox_t mailbox);
void mu_mboxrb_message_free (struct mu_mboxrb_message *dmsg);
int mu_mboxrb_message_get (struct mu_mboxrb_message *dmsg, mu_message_t *mptr);
int mu_mboxrb_message_attr_load (struct mu_mboxrb_message *dmsg);
int mu_mboxrb_mailbox_uid_setup (struct mu_mboxrb_mailbox *dmp);
int mu_mboxrb_message_reconstruct (mu_stream_t dest,
				   struct mu_mboxrb_message *dmsg,
				   struct mu_mboxrb_message *ref,
				   char const *x_imapbase);

#endif  
