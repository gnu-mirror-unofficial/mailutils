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
#include <unistd.h>
#ifdef WITH_PTHREAD
# include <pthread.h>
#endif
#include <sys/stat.h>
#include <signal.h>
#include <mailutils/sys/mboxrb.h>
#include <mailutils/sys/mailbox.h>
#include <mailutils/sys/message.h>
#include <mailutils/diag.h>
#include <mailutils/errno.h>
#include <mailutils/url.h>
#include <mailutils/property.h>
#include <mailutils/io.h>
#include <mailutils/observer.h>
#include <mailutils/filter.h>
#include <mailutils/stream.h>
#include <mailutils/locker.h>
#include <mailutils/nls.h>
#include <mailutils/header.h>
#include <mailutils/attribute.h>
#include <mailutils/envelope.h>
#include <mailutils/util.h>
#include <mailutils/cctype.h>
#include <mailutils/sys/folder.h>
#include <mailutils/sys/registrar.h>

static void
mboxrb_destroy (mu_mailbox_t mailbox)
{
  size_t i;
  struct mu_mboxrb_mailbox *dmp = mailbox->data;

  if (!dmp)
    return;

  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_TRACE1,
	    ("%s (%s)", __func__, dmp->name));
  mu_monitor_wrlock (mailbox->monitor);
  for (i = 0; i < dmp->mesg_count; i++)
    {
      mu_mboxrb_message_free (dmp->mesg[i]);
    }
  free (dmp->mesg);
  free (dmp->name);
  free (dmp);
  mailbox->data = NULL;
  mu_monitor_unlock (mailbox->monitor);
}

static int
mboxrb_mailbox_init_stream (struct mu_mboxrb_mailbox *dmp)
{
  int rc;
  mu_mailbox_t mailbox = dmp->mailbox;

  /*
   * Initialize stream flags.  If append mode is requested, convert it to
   * read-write, so that mboxrb_flush_unlocked be able to update the
   * X-IMAPbase header in the first message, if necessary.
   */
  dmp->stream_flags = mailbox->flags;
  if (dmp->stream_flags & MU_STREAM_APPEND)
    dmp->stream_flags = (dmp->stream_flags & ~MU_STREAM_APPEND) | MU_STREAM_RDWR;
  else if (dmp->stream_flags & MU_STREAM_WRITE)
    dmp->stream_flags |= MU_STREAM_READ;
  
  rc = mu_mapfile_stream_create (&mailbox->stream, dmp->name, dmp->stream_flags);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s:%s (%s): %s",
		 __func__, "mu_mapfile_stream_create", dmp->name,
		 mu_strerror (rc)));

      /* Fallback to regular file stream */
      rc = mu_file_stream_create (&mailbox->stream, dmp->name, dmp->stream_flags);
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s:%s (%s): %s",
		 __func__, "mu_file_stream_create", dmp->name,
		 mu_strerror (rc)));

      if (rc)
	return rc;
    }

  mu_stream_set_buffer (mailbox->stream, mu_buffer_full, 0);
  return 0;
}

static int
mboxrb_open (mu_mailbox_t mailbox, int flags)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  int rc;

  if (!dmp)
    return EINVAL;

  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_TRACE1,
	    ("%s(%s, 0x%x)", __func__, dmp->name, mailbox->flags));

  mailbox->flags = flags;

  rc = mboxrb_mailbox_init_stream (dmp);

  if (rc == 0
      && mailbox->locker == NULL
      && (flags & (MU_STREAM_WRITE | MU_STREAM_APPEND | MU_STREAM_CREAT)))
    {
      rc = mu_locker_create (&mailbox->locker, dmp->name, 0);
      if (rc)
	mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		  ("%s:%s (%s): %s",
		   __func__, "mu_locker_create", dmp->name,
		   mu_strerror (rc)));
    }

  return rc;
}

enum
  {
    FLUSH_SYNC,
    FLUSH_EXPUNGE, /* implies SYNC */
    FLUSH_UIDVALIDITY
  };

static int mboxrb_flush (struct mu_mboxrb_mailbox *dmp, int flag);

static int
mboxrb_close (mu_mailbox_t mailbox)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  size_t i;

  if (!dmp)
    return EINVAL;

  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_TRACE1,
	    ("%s (%s)", __func__, dmp->name));

  if (dmp->uidvalidity_changed && (dmp->stream_flags & MU_STREAM_WRITE))
    mboxrb_flush (dmp, FLUSH_UIDVALIDITY);
  
  mu_locker_unlock (mailbox->locker);
  mu_monitor_wrlock (mailbox->monitor);
  for (i = 0; i < dmp->mesg_count; i++)
    {
      mu_mboxrb_message_free (dmp->mesg[i]);
    }
  free (dmp->mesg);
  dmp->mesg = NULL;
  dmp->mesg_count = dmp->mesg_max = 0;
  dmp->size = 0;
  dmp->uidvalidity = 0;
  dmp->uidnext = 1;
  mu_monitor_unlock (mailbox->monitor);
  mu_stream_destroy (&mailbox->stream);
  return 0;
}

static int
mboxrb_remove (mu_mailbox_t mailbox)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;

  if (!dmp)
    return EINVAL;
  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_TRACE1,
	    ("%s (%s)", __func__, dmp->name));
  if (unlink (dmp->name))
    return errno;
  return 0;
}

static int
mboxrb_is_updated (mu_mailbox_t mailbox)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  mu_off_t size = 0;

  if (!dmp)
    return 0;

  if (mu_stream_size (mailbox->stream, &size) != 0)
    return 1;
  if (size < dmp->size)
    {
      mu_observable_notify (mailbox->observable, MU_EVT_MAILBOX_CORRUPT,
			    mailbox);
      mu_diag_output (MU_DIAG_EMERG, _("mailbox corrupted, shrank in size"));
      return 0;
    }
  return (dmp->size == size);
}

#ifdef WITH_PTHREAD
void
mboxrb_cleanup (void *arg)
{
  mu_mailbox_t mailbox = arg;
  mu_monitor_unlock (mailbox->monitor);
  mu_locker_unlock (mailbox->locker);
}
#endif

static int
mboxrb_alloc_message (struct mu_mboxrb_mailbox *dmp,
		       struct mu_mboxrb_message **dmsg_ptr)
{
  struct mu_mboxrb_message *dmsg;

  if (dmp->mesg_count == dmp->mesg_max)
    {
      size_t n = dmp->mesg_max;
      void *p;

      if (n == 0)
	n = 64;
      else
	{
	  if ((size_t) -1 / 3 * 2 / sizeof (dmp->mesg[0]) <= n)
	    return ENOMEM;
	  n += (n + 1) / 2;
	}
      p = realloc (dmp->mesg, n * sizeof (dmp->mesg[0]));
      if (!p)
	return ENOMEM;
      dmp->mesg = p;
      dmp->mesg_max = n;
    }
  dmsg = calloc (1, sizeof (*dmsg));
  if (!dmsg)
    return ENOMEM;
  dmsg->mbox = dmp;
  dmsg->num = dmp->mesg_count;
  dmp->mesg[dmp->mesg_count++] = dmsg;
  *dmsg_ptr = dmsg;
  return 0;
}

static int
mboxrb_dispatch (mu_mailbox_t mailbox, int evt, void *data)
{
  if (!mailbox->observable)
    return 0;

  mu_monitor_unlock (mailbox->monitor);
  if (mu_observable_notify (mailbox->observable, evt, data))
    {
      if (mailbox->locker)
	mu_locker_unlock (mailbox->locker);
      return EINTR;
    }
  mu_monitor_wrlock (mailbox->monitor);
  return 0;
}

/* Notes on the UID subsystem

   1. The values of uidvalidity and uidnext are stored in the
      X-IMAPbase header in the first message.
   2. Message UID is stored in the X-UID header in that message.
   3. To minimize unwanted modifications to the mailbox, the
      UID subsystem is initialized only in the following cases:

      3a. Upon mailbox scanning, if the first message contains a
	  valid X-IMAPbase header. In this case, the
	  mboxrb_rescan_unlocked function initializes each
	  message's uid value from the X-UID header. The first
	  message that lacks X-UID or with an X-UID that cannot
	  be parsed, gets assigned new UID. The subsequent
	  messages are assigned new UIDs no matter whether they
	  have X-UID headers. In this case, the uidvalidity value
	  is reset to the current timestamp, to indicate that all
	  UIDs might have changed.

      3b. When any of the following functions are called for
	  the first time: mboxrb_uidvalidity, mboxrb_uidnext,
	  mboxrb_message_uid. This means that the caller used
	  mu_mailbox_uidvalidity, mu_mailbox_uidnext, or
	  mu_message_get_uid.
	  In this case, each message is assigned a UID equal to
	  its ordinal number (1-based) in the mailbox.
	  This is done by the mu_mboxrb_mailbox_uid_setup function.

   4. When a message is appended to the mailbox, any existing
      X-IMAPbase and X-UID headers are removed from it. If the
      UID subsystem is initialized, the message is assigned a new
      UID.
   5. Assigning new UID to a message does not change its attributes.
      Instead, its uid_modified flag is set.
*/

/* Allocate next available UID for the mailbox.
   The caller must ensure that the UID subsystem is initialized.
*/
static unsigned long
mboxrb_alloc_next_uid (struct mu_mboxrb_mailbox *mbox)
{
  mbox->uidvalidity_changed = 1;
  return mbox->uidnext++;
}

static void
mboxrb_message_alloc_uid (struct mu_mboxrb_message *dmsg)
{
  dmsg->uid = mboxrb_alloc_next_uid (dmsg->mbox);
  dmsg->uid_modified = 1;
}

/* Width of the decimal representation of the maximum value of the unsigned
 * type t.  146/485 is the closest approximation of log10(2):
 *
 *  log10(2) = .301030
 *  146/485  = .301031
*/
#define UINT_STRWIDTH(t) ((int)((sizeof(t) * 8 * 146 + 484) / 485))

/*
 * The format for the X-IMAPbase header is:
 *
 *    X-IMAPbase: <V> <N>
 *
 * where <V> and <N> are current values of the uidvalidity and uidnext
 * parameters, correspondingly.
 *
 * The header is stored in the first message.  To avoid rewriting entire
 * mailbox when one of the parameters chages, the values of <V> and <N>
 * are left-padded with spaces to the maximum width of their data types.
 *
 * Offset of the header in the mailbox and its length (without the
 * trailing newline) are stored in x_imapbase_off and x_imapbase_len
 * members of struct mu_mboxrb_mailbox.
 *
 * The X_IMAPBASE_MAX macro returns maximum size of the buffer necessary
 * for formatting the X-IMAPbase header.  In fact, it is 2 bytes wider
 * than necessary (due to the two '0' in the sample string below).
 */
#define X_IMAPBASE_MAX(d)		   \
  (sizeof (MU_HEADER_X_IMAPBASE ": 0 0") + \
   UINT_STRWIDTH ((d)->uidvalidity) +	   \
   UINT_STRWIDTH ((d)->uidnext))

/*
 * A modified version of Marc Crispin VALID macro.
 *
 * This function handles all existing flavors of the From_ line, most
 * of which are antiquated and fallen out of use:
 *
 *              From user Wed Dec  2 05:53 1992
 * BSD          From user Wed Dec  2 05:53:22 1992
 * SysV         From user Wed Dec  2 05:53 PST 1992
 * rn           From user Wed Dec  2 05:53:22 PST 1992
 *              From user Wed Dec  2 05:53 -0700 1992
 *              From user Wed Dec  2 05:53:22 -0700 1992
 *              From user Wed Dec  2 05:53 1992 PST
 *              From user Wed Dec  2 05:53:22 1992 PST
 *              From user Wed Dec  2 05:53 1992 -0700
 * Solaris      From user Wed Dec  2 05:53:22 1992 -0700
 *
 * (plus all of them followed by " remote from xxx").  Moreover, the
 * user part is allowed to have whitespaces in it (although mailutils
 * email address functions won't tolerate it).
 * 
 * Input: S - line read from the mailbox (with \n terminator).
 * Output: ZP - points to the space character preceding the time zone
 * information in S.  If there is no TZ, points to the terminating \n.
 *
 * Return value: If S is a valid From_ line, a pointer to the time
 * information in S.  Otherwise, NULL.
 */
static inline char *
parse_from_line (char const *s, char **zp)
{
  int ti = 0;
  int zn;
  // Validate if the string begins with 'From '
  if ((*s == 'F') && (s[1] == 'r') && (s[2] == 'o') && (s[3] == 'm') && 
      (s[4] == ' '))
    {
      char *x = strchr (s, '\n');
      if (x)
	{
	  if (x - s >= 41)
	    {
	      static char suf[] = " remote from ";
#             define suflen (sizeof(suf)-1)

	      for (zn = -1; x + zn > s && x[zn] != ' '; (zn)--);
	      if (memcmp (x + zn - suflen + 1, suf, suflen) == 0)
		x += zn - suflen + 1;
	    }
	  if (x - s >= 27)
	    {
	      if (x[-5] == ' ')
		{
		  if (x[-8] == ':')
		    {
		      zn = 0;
		      ti = -5;
		    }
		  else if (x[-9] == ' ')
		    ti = zn = -9;
		  else if ((x[-11] == ' ')
			   && ((x[-10] == '+') || (x[-10] == '-')))	      
		    ti = zn = -11;
		}							      
	      else if (x[-4] == ' ')                          
		{                                                 
		  if (x[-9] == ' ')
		    {
		      zn = -4;
		      ti = -9;
		    }
		}
	      else if (x[-6] == ' ')                                        
		{                                                           
		  if ((x[-11] == ' ') && ((x[-5] == '+') || (x[-5] == '-')))
		    {
		      zn = -6;
		      ti = -11;
		    }
		}
	      if (ti && !((x[ti - 3] == ':') &&
			  (x[ti -= ((x[ti - 6] == ':') ? 9 : 6)] == ' ') &&
			  (x[ti - 3] == ' ') && (x[ti - 7] == ' ') &&
			  (x[ti - 11] == ' ')))
		ti = 0;
	    }
	}
      if (ti)
	{
	  *zp = x + zn;
	  return (char*) x + ti;
	}
    }
  return NULL;
}

/* Finalize current message */
static inline int
scan_message_finalize (struct mu_mboxrb_mailbox *dmp,
		       struct mu_mboxrb_message *dmsg, mu_stream_t stream,
		       size_t n, int *force_init_uids)
{
  int rc;
  size_t count;
  
  rc = mu_stream_seek (stream, 0, MU_SEEK_CUR, &dmsg->message_end);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s:%s (%s): %s",
		 __func__, "mu_stream_seek", dmp->name,
		 mu_strerror (rc)));
      return -1;
    }
  dmsg->message_end -= n + 1;
  if (dmsg->uid == 0)
    *force_init_uids = 1;
  if (*force_init_uids)
    mboxrb_message_alloc_uid (dmsg);

  /* Every 100 mesgs update the lock, it should be every minute.  */
  if (dmp->mailbox->locker && (dmp->mesg_count % 100) == 0)
    mu_locker_touchlock (dmp->mailbox->locker);

  count = dmp->mesg_count;
  mboxrb_dispatch (dmp->mailbox, MU_EVT_MESSAGE_ADD, &count);
  return 0;
}

static inline struct mu_mboxrb_message *
scan_message_begin (struct mu_mboxrb_mailbox *dmp, mu_stream_t stream,
		    char *buf, size_t n, char *ti, char *zn)
{
  int rc;
  struct mu_mboxrb_message *dmsg;
  
  /* Create new message */
  rc = mboxrb_alloc_message (dmp, &dmsg);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s:%s (%s): %s",
		 __func__, "mboxrb_alloc_message", dmp->name,
		 mu_strerror (rc)));
      return NULL;
    }
  rc = mu_stream_seek (stream, 0, MU_SEEK_CUR, &dmsg->message_start);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s:%s (%s): %s",
		 __func__, "mu_stream_seek", dmp->name,
		 mu_strerror (rc)));
      return NULL;
    }
  dmsg->message_start -= n;
  dmsg->from_length = n;
  dmsg->env_sender_len = ti - buf - 10;
  while (dmsg->env_sender_len > 6 && buf[dmsg->env_sender_len-1] == ' ')
    dmsg->env_sender_len--;
  dmsg->env_sender_len -= 5;

  if (zn[0] != '\n')
    {
      /*
       * Ideally, From_ line should not contain time zone info.  If it does,
       * zn points to the first space before the TZ.  The latter can be an
       * abbreviated time zone or a numeric offset from UTC (see the comment
       * to parse_from_line above).  Parse the date line and convert it to
       * a normalized form (ctime(3)).
       */
      struct tm tm;
      char const *fmt;
      struct mu_timezone tz;
      char *te;
      time_t t;
      
      int numeric_zone = zn[1] == '+' || zn[1] == '-' || mu_isdigit (zn[1]);
      if (zn[-3] == ':')
	{
	  if (zn[-6] == ':')
	    {
	      if (numeric_zone)
		fmt = "%a %b %e %H:%M:%S %z %Y";
	      else
		fmt = "%a %b %e %H:%M:%S %Z %Y";
	    }
	  else
	    {
	      if (numeric_zone)
		fmt = "%a %b %e %H:%M %z %Y";
	      else
		fmt = "%a %b %e %H:%M %Z %Y";
	    }
	}
      else
	{
	  if (zn[-11] == ':')
	    {
	      if (numeric_zone)
		fmt = "%a %b %e %H:%M:%S %Y %z";
	      else
		fmt = "%a %b %e %H:%M:%S %Y %Z";
	    }
	  else
	    {
	      if (numeric_zone)
		fmt = "%a %b %e %H:%M %Y %z";
	      else
		fmt = "%a %b %e %H:%M %Y %Z";
	    }
	}
	  
      if (mu_scan_datetime (ti - 10, fmt, &tm, &tz, &te) == 0)
	t = mu_datetime_to_utc (&tm, &tz);
      else
	t = time (NULL);
      gmtime_r (&t, &tm);
      mu_strftime (dmsg->date, sizeof (dmsg->date), MU_DATETIME_FROM, &tm);
    }
  else
    {
      /*
       * No zone information in the timestamp.  Copy the timestamp to
       * the date buffer.  The timestamp may or may not contain seconds.
       * In the latter case, assume '0' seconds.
       */
      if (ti[6] == ':')
	memcpy (dmsg->date, ti - 10, MU_DATETIME_FROM_LENGTH);
      else
	{
	  memcpy (dmsg->date, ti - 10, 16);
	  memcpy (dmsg->date + 16, ":00", 3);
	  dmsg->date[19] = ' ';
	  memcpy (dmsg->date + 20, ti + 7, 4);
	}
      dmsg->date[24] = 0;
    }
  
  return dmsg;
}

/* Scan the mailbox starting from the given offset.
 *
 * Notes on the mailbox format:
 *
 *  1. A mailbox consists of a series of messages.
 * 
 *  2. Each message is preceded by a From_ line and followed by a blank line.
 *     A From_ line is a line that begins with the five characters 'F', 'r',
 *     'o', 'm', and ' ', followed by sender email and delivery date.  The
 *     From_ line parser is able to handle various From_ line formats
 *     (differing mainly in date/time format), most of which are encountered
 *     only in ancient mailboxes.  Nevertheless, this makes escaping of the
 *     From_ lines less crucial and ensures optimal robustness in handling
 *     different mailbox formats (mboxo, mboxrd and mboxcl (mboxcl2) are
 *     all handled properly.
 *
 *  3. The From_ lines and the blank lines bracket messages, fore and aft.
 *     They do not comprise a message divider.  This means, in particular,
 *     that both bracketing lines should be included in the message octet
 *     and line counts.  However, counting the From_ line goes against
 *     the mailutils approach of logically dividing envelope and the rest
 *     of the message and would create useless differences compared to
 *     another mailbox formats.  For this reason, the From_ line is not
 *     reflected in returns from the mu_message_size and mu_message_lines
 *     functions.  The terminating blank line, on the contrary, is assumed
 *     to be part of the message body and is counted in body and message
 *     size and line count computations.
 *
 *  4. A mailbox that contains zero messages contains no lines.
 *
 *  5. The first message in the mailbox is not preceded by a blank line.
 *     The last message in the mailbox is not followed by a From_ line.
 * 
 *  6. If a non-empty file does not have valid From_ construct in its
 *     first physical line, it will be rejected by the parser.
 *
 *  7. A message may contain blank lines.  It should not, however, contain
 *     lines beginning with the sequence 'F', 'r', 'o', 'm', ' '.
 *
 *  8. When incorporating a message into the mailbox, any line in the message
 *     body that begins with zero or more '>' characters immediately followed
 *     by the sequence 'F', 'r', 'o', 'm', ' ', is escaped by prepending it
 *     with a '>' character.  Thus, "From " becomes ">From ", ">From "
 *     becomes ">>From ", and so on.  When reading the message body, a
 *     reverse operation is performed.
 *
 *  9. Last message in the mailbox is allowed to end with a partial last line
 *     (i.e. the one whose final characters are not two newlines).  The message
 *     will be handled as usual.  When a new message is incorporated to the
 *     mailbox, the missing newlines will be added to the end of the last
 *     message.
 */
static int
mboxrb_rescan_unlocked (mu_mailbox_t mailbox, mu_off_t offset)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  int rc;
  mu_stream_t stream;
  char *buf = NULL;
  size_t bufsize = 0;
  size_t n;
  enum mboxrb_scan_state
  {
    mboxrb_scan_init,  /* At the beginning of the file */
    mboxrb_scan_header,/* Scanning message header */
    mboxrb_scan_body,  /* Scanning message body */
    mboxrb_scan_empty_line /* At the empty line within or at the end of body */
  } state = mboxrb_scan_init;
  struct mu_mboxrb_message *dmsg = NULL;
  char *zn, *ti;
  int force_init_uids = 0;
  size_t numlines = 0;
  
# define IS_HEADER(h,b,n)			\
  ((n) > sizeof (h) - 1				\
   && strncasecmp (b, h, sizeof (h) - 1) == 0	\
   && b[sizeof (h) - 1] == ':')
  

  rc = mu_stream_size (mailbox->stream, &dmp->size);
  if (rc)
    return rc;
  if (offset == dmp->size)
    return 0;
  if (!(dmp->stream_flags & MU_STREAM_READ))
    return 0;

  rc = mu_streamref_create (&stream, mailbox->stream);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s:%s (%s): %s",
		 __func__, "mu_streamref_create", dmp->name,
		 mu_strerror (rc)));
      return rc;
    }

  rc = mu_stream_seek (stream, offset, MU_SEEK_SET, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s:%s (%s): %s",
		 __func__, "mu_stream_seek", dmp->name,
		 mu_strerror (rc)));
      return rc;
    }

  while ((rc = mu_stream_getline (stream, &buf, &bufsize, &n)) == 0
	 && n > 0)
    {
      switch (state)
	{
	case mboxrb_scan_init:
	  if ((ti = parse_from_line (buf, &zn)) == 0)
	    {
	      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			("%s does not start with a valid From_ line",
			 dmp->name));
	      rc = MU_ERR_PARSE;
	      goto err;
	    }
	  if ((dmsg = scan_message_begin (dmp, stream, buf, n, ti, zn)) == NULL)
	    goto err;
	  state = mboxrb_scan_header;
	  break;

	case mboxrb_scan_header:
	  if (n == 1 && buf[0] == '\n')
	    {
	      rc = mu_stream_seek (stream, 0, MU_SEEK_CUR, &dmsg->body_start);
	      if (rc)
		{
		  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			    ("%s:%s (%s): %s",
			     __func__, "mu_stream_seek", dmp->name,
			     mu_strerror (rc)));
		  goto err;
		}
	      state = mboxrb_scan_body;
	    }
	  else if (mu_isspace (buf[0]))
	    continue;
	  else if (!dmp->uidvalidity_scanned
		   && IS_HEADER (MU_HEADER_X_IMAPBASE, buf, n))
	    {
	      if (sscanf (buf + sizeof (MU_HEADER_X_IMAPBASE),
			  "%lu %lu",
			  &dmp->uidvalidity, &dmp->uidnext) == 2)
		{
		  mu_off_t off;
		  
		  rc = mu_stream_seek (stream, 0, MU_SEEK_CUR, &off);
		  if (rc)
		    {
		      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
				("%s:%s (%s): %s",
				 __func__, "mu_stream_seek", dmp->name,
				 mu_strerror (rc)));
		      goto err;
		    }
		  dmp->x_imapbase_len = n - 1;
		  dmp->x_imapbase_off = off - n;
		  dmp->uidvalidity_scanned = 1;
		}
	    }
	  else if (!force_init_uids
		   && dmsg->uid == 0
		   && IS_HEADER (MU_HEADER_X_UID, buf, n))
	    {
	      if (!(sscanf (buf + sizeof (MU_HEADER_X_UID), "%lu", &dmsg->uid) == 1
		    && dmsg->uid < dmp->uidnext
		    && (dmsg->num == 0 || dmsg->uid > dmp->mesg[dmsg->num - 1]->uid)))
		{
		  force_init_uids = 1;
		}
	    }
	  else if (IS_HEADER (MU_HEADER_STATUS, buf, n))
	    {
	      mu_attribute_string_to_flags (buf + sizeof (MU_HEADER_STATUS),
					    &dmsg->attr_flags);
	    }
	  break;

	case mboxrb_scan_body:
	  if (n == 1 && buf[0] == '\n')
	    {
	      state = mboxrb_scan_empty_line;
	    }
	  break;

	case mboxrb_scan_empty_line:
	  if ((ti = parse_from_line (buf, &zn)) != 0)
	    {
	      if (scan_message_finalize (dmp, dmsg, stream, n, &force_init_uids))
		goto err;
	      if ((dmsg = scan_message_begin (dmp, stream, buf, n, ti, zn)) == NULL)
		goto err;
	      state = mboxrb_scan_header;
	    }
	  else if (n == 1 && buf[0] == '\n')
	    state = mboxrb_scan_empty_line;
	  else
	    state = mboxrb_scan_body;
	}
      if (++numlines % 1000 == 0)
	mboxrb_dispatch (mailbox, MU_EVT_MAILBOX_PROGRESS, NULL);
    }

  if (dmsg)
    {
      if (scan_message_finalize (dmp, dmsg, stream, 0, &force_init_uids))
	goto err;
    }
  
 err:
  if (rc)
    {
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s:%s (%s): %s",
		 __func__, "mu_stream_read", dmp->name,
		 mu_strerror (rc)));
    }
  mu_stream_unref (stream);

  if (force_init_uids)
    {
      dmp->uidvalidity = (unsigned long) time (NULL);
      dmp->uidvalidity_changed = 1;
      dmp->uidvalidity_scanned = 1;
    }      
  
  return rc;
}

/* Scan the mailbox starting from the given offset */
static int
mboxrb_rescan (mu_mailbox_t mailbox, mu_off_t offset)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  int rc;

  if (!dmp)
    return EINVAL;

  if (!(dmp->stream_flags & MU_STREAM_READ))
    return 0;
      
  mu_monitor_wrlock (mailbox->monitor);
#ifdef WITH_PTHREAD
  pthread_cleanup_push (mboxrb_cleanup, (void *)mailbox);
#endif

  if (mailbox->locker && (rc = mu_locker_lock (mailbox->locker)))
    {
      mu_monitor_unlock (mailbox->monitor);
      return rc;
    }

  rc = mboxrb_rescan_unlocked (mailbox, offset);

  if (mailbox->locker)
    mu_locker_unlock (mailbox->locker);
  mu_monitor_unlock (mailbox->monitor);

#ifdef WITH_PTHREAD
  pthread_cleanup_pop (0);
#endif

  return rc;
}

static int
mboxrb_refresh (mu_mailbox_t mailbox)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;

  if (mboxrb_is_updated (mailbox))
    return 0;
  return mboxrb_rescan (mailbox,
			 dmp->mesg_count == 0
			   ? 0
			   : dmp->mesg[dmp->mesg_count - 1]->message_end + 1);
}

static int
mboxrb_scan (mu_mailbox_t mailbox, size_t i, size_t *pcount)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;

  if (!dmp)
    return EINVAL;

  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_TRACE1,
	    ("%s (%s)", __func__, dmp->name));

  if (i == 0 || (dmp->mesg_count && i > dmp->mesg_count))
    return EINVAL;

  if (!mboxrb_is_updated (mailbox))
    {
      int rc;

      while (i < dmp->mesg_count)
	mu_mboxrb_message_free (dmp->mesg[dmp->mesg_count--]);

      rc = mboxrb_refresh (mailbox);
      if (rc)
	return rc;
    }
  else if (mailbox->observable)
    {
      for (; i <= dmp->mesg_count; i++)
	{
	  size_t tmp = i;
	  if (mu_observable_notify (mailbox->observable, MU_EVT_MESSAGE_ADD,
				    &tmp) != 0)
	    break;
	  /* FIXME: Hardcoded value! Must be configurable */
	  if (((i + 1) % 50) == 0)
	    mu_observable_notify (mailbox->observable, MU_EVT_MAILBOX_PROGRESS,
				  NULL);
	}
    }
  if (pcount)
    *pcount = dmp->mesg_count;
  return 0;
}

static int
mboxrb_messages_recent (mu_mailbox_t mailbox, size_t *pcount)
{
  size_t i;
  size_t count = 0;
  struct mu_mboxrb_mailbox *dmp = mailbox->data;

  int rc = mboxrb_refresh (mailbox);
  if (rc)
    return rc;

  for (i = 0; i < dmp->mesg_count; i++)
    {
      if (MU_ATTRIBUTE_IS_UNSEEN (dmp->mesg[i]->attr_flags))
	++count;
    }

  *pcount = count;

  return 0;
}

static int
mboxrb_message_unseen (mu_mailbox_t mailbox, size_t *pmsgno)
{
  size_t i;
  struct mu_mboxrb_mailbox *dmp = mailbox->data;

  int rc = mboxrb_refresh (mailbox);
  if (rc)
    return rc;

  for (i = 0; i < dmp->mesg_count; i++)
    {
      if (MU_ATTRIBUTE_IS_UNREAD (dmp->mesg[i]->attr_flags))
	{
	  *pmsgno = i + 1;
	  return 0;
	}
    }

  return MU_ERR_NOENT;
}

/* Initialize the mailbox UID subsystem. See the Notes above. */
int
mu_mboxrb_mailbox_uid_setup (struct mu_mboxrb_mailbox *dmp)
{
  if (!dmp->uidvalidity_scanned)
    {
      size_t i;
      int rc = mboxrb_refresh (dmp->mailbox);
      if (rc || dmp->uidvalidity_scanned)
	return rc;

      dmp->uidvalidity = (unsigned long)time (NULL);
      dmp->uidnext = 1;
      dmp->uidvalidity_scanned = 1;
      dmp->uidvalidity_changed = 1;
      
      for (i = 0; i < dmp->mesg_count; i++)
	mboxrb_message_alloc_uid (dmp->mesg[i]);
    }
  return 0;
}

static int
mboxrb_get_uidvalidity (mu_mailbox_t mailbox, unsigned long *puidvalidity)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  int rc = mu_mboxrb_mailbox_uid_setup (dmp);
  if (rc == 0)
    *puidvalidity = dmp->uidvalidity;
  return rc;
}

static int
mboxrb_set_uidvalidity (mu_mailbox_t mailbox, unsigned long uidvalidity)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  int rc = mu_mboxrb_mailbox_uid_setup (dmp);
  if (rc == 0)
    dmp->uidvalidity = uidvalidity;
  return rc;
}

static int
mboxrb_uidnext (mu_mailbox_t mailbox, size_t *puidnext)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  int rc = mu_mboxrb_mailbox_uid_setup (dmp);
  if (rc == 0)
    *puidnext = dmp->uidnext;
  return rc;
}

static int
mboxrb_get_message (mu_mailbox_t mailbox, size_t msgno, mu_message_t *pmsg)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  int rc;

  if (!dmp || msgno < 1)
    return EINVAL;
  if (pmsg == NULL)
    return MU_ERR_OUT_PTR_NULL;

  if (dmp->mesg_count == 0)
    {
      rc = mboxrb_scan (mailbox, 1, NULL);
      if (rc)
	return rc;
    }

  if (msgno > dmp->mesg_count)
    return MU_ERR_NOENT;

  return mu_mboxrb_message_get (dmp->mesg[msgno-1], pmsg);
}

static int
qid2off (mu_message_qid_t qid, mu_off_t *pret)
{
  mu_off_t ret = 0;
  for (;*qid; qid++)
    {
      if (!('0' <= *qid && *qid <= '9'))
	return 1;
      ret = ret * 10 + *qid - '0';
    }
  *pret = ret;
  return 0;
}

static int
mboxrb_quick_get_message (mu_mailbox_t mailbox, mu_message_qid_t qid,
			   mu_message_t *pmsg)
{
  int rc;
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  struct mu_mboxrb_message *dmsg;
  mu_off_t offset;

  if (mailbox == NULL || qid2off (qid, &offset)
      || !(mailbox->flags & MU_STREAM_QACCESS))
    return EINVAL;

  if (dmp->mesg_count == 0)
    {
      rc = mboxrb_rescan (mailbox, offset);
      if (rc)
	return rc;
      if (dmp->mesg_count == 0)
	return MU_ERR_NOENT;
    }

  dmsg = dmp->mesg[0];
  if (dmsg->message_start != offset)
    return MU_ERR_EXISTS;
  if (dmsg->message)
    {
      if (pmsg)
	*pmsg = dmsg->message;
      return 0;
    }
  return mu_mboxrb_message_get (dmsg, pmsg);
}

static int
mailbox_append_message (mu_mailbox_t mailbox, mu_message_t msg)
{
  int rc;
  mu_off_t size;
  mu_stream_t istr, flt;
  static char *exclude_headers[] = {
    MU_HEADER_X_IMAPBASE,
    MU_HEADER_X_UID,
    NULL
  };
  struct mu_mboxrb_mailbox *dmp = mailbox->data;

  if (dmp->mesg_count)
    {
      char nl[2];
      static char pad[] = { '\n', '\n' };
      int n;
      
      size = dmp->mesg[dmp->mesg_count-1]->message_end - 1;
      rc = mu_stream_seek (mailbox->stream, size, MU_SEEK_SET, NULL);
      if (rc)
	return rc;
      rc = mu_stream_read (mailbox->stream, nl, 2, NULL);
      if (rc)
	return rc;

      if (nl[1] != '\n')
	n = 2;
      else if (nl[0] != '\n')
	n = 1;
      else
	n = 0;

      if (n)
	{
	  mu_stream_write (mailbox->stream, pad, n, NULL);
	}
      size += n + 2;
    }
  else
    {
      size = 0;
      rc = mu_stream_seek (mailbox->stream, size, MU_SEEK_SET, NULL);
    }
  
  if (rc)
    return rc;

  rc = mu_message_get_streamref (msg, &istr);
  if (rc)
    return rc;

  do
    {
      mu_envelope_t env;
      char *date = NULL;
      char *sender = NULL;

      rc = mu_message_get_envelope (msg, &env);
      if (rc)
	break;

      rc = mu_envelope_aget_sender (env, &sender);
      if (rc == 0)
	{
	  rc = mu_envelope_aget_date (env, &date);
	  if (rc)
	    {
	      rc = mu_message_reconstruct_envelope (msg, &env);
	      if (rc == 0)
		{
		  rc = mu_envelope_aget_sender (env, &sender);
		  if (rc == 0)
		    {
		      rc = mu_envelope_aget_date (env, &date);
		    }

		  mu_envelope_destroy (&env, msg);
		}
	    }
	  
	  if (rc)
	    {
	      free (sender);
	      break;
	    }
	  
	  rc = mu_stream_printf (mailbox->stream, "From %s %s\n",
				 sender, date);
	  free (sender);
	  free (date);
	}

      if (rc)
	break;
      
      rc = mu_stream_header_copy (mailbox->stream, istr, exclude_headers);
      if (rc)
	break;

      /* Write UID-related data */
      if (dmp->uidvalidity_scanned)
	{
	  if (dmp->mesg_count == 0)
	    mu_stream_printf (mailbox->stream, "%s: %*lu %*lu\n",
			      MU_HEADER_X_IMAPBASE,
			      UINT_STRWIDTH (dmp->uidvalidity),
			      dmp->uidvalidity,
			      UINT_STRWIDTH (dmp->uidnext),
			      dmp->uidnext);
	  mu_stream_printf (mailbox->stream, "%s: %lu\n",
			    MU_HEADER_X_UID,
			    mboxrb_alloc_next_uid (dmp));
	}

      rc = mu_stream_write (mailbox->stream, "\n", 1, NULL);
      if (rc)
	break;

      rc = mu_filter_create (&flt, istr, "FROMRB",
			     MU_FILTER_ENCODE, MU_STREAM_READ);
      mu_stream_destroy (&istr);
      rc = mu_stream_copy_nl (mailbox->stream, flt, 0, NULL);
      mu_stream_unref (flt);
    }
  while (0);

  if (rc)
    {
      mu_stream_destroy (&istr);
      rc = mu_stream_truncate (mailbox->stream, size);
      if (rc)
	mu_error (_("cannot truncate stream after failed append: %s"),
		  mu_stream_strerror (mailbox->stream, rc));
      return rc;
    }

  /* Rescan the message */
  rc = mboxrb_rescan_unlocked (mailbox, size);
  if (rc)
    return rc;

  if (mailbox->observable)
    {
      char *buf = NULL;
      mu_asprintf (&buf, "%lu", (unsigned long) size);
      mu_observable_notify (mailbox->observable,
			    MU_EVT_MAILBOX_MESSAGE_APPEND, buf);
      free (buf);
    }

  return 0;
}

static int
mboxrb_append_message (mu_mailbox_t mailbox, mu_message_t msg)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  int rc;

  rc = mboxrb_refresh (mailbox);
  if (rc)
    return rc;
  
  mu_monitor_wrlock (mailbox->monitor);
  if (mailbox->locker && (rc = mu_locker_lock (mailbox->locker)) != 0)
    {
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("%s(%s):%s: %s",
		 __func__, dmp->name, "mu_locker_lock",
		 mu_strerror (rc)));
    }
  else
    {
      rc = mailbox_append_message (mailbox, msg);

      if (mailbox->locker)
	mu_locker_unlock (mailbox->locker);
    }
  mu_monitor_unlock (mailbox->monitor);
  return rc;
}

static int
mboxrb_messages_count (mu_mailbox_t mailbox, size_t *pcount)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  int rc;

  if (!dmp)
    return EINVAL;

  rc = mboxrb_refresh (mailbox);
  if (rc)
    return rc;

  if (pcount)
    *pcount = dmp->mesg_count;

  return 0;
}

static int
mboxrb_get_size (mu_mailbox_t mailbox, mu_off_t *psize)
{
  mu_off_t size;
  int rc;

  rc  = mu_stream_size (mailbox->stream, &size);
  if (rc != 0)
    return rc;
  if (psize)
    *psize = size;
  return 0;
}

static int
mboxrb_get_atime (mu_mailbox_t mailbox, time_t *return_time)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  struct stat st;

  if (dmp == NULL)
    return EINVAL;
  if (stat (dmp->name, &st))
    return errno;
  *return_time = st.st_atime;
  return 0;
}

struct mu_mboxrb_flush_tracker
{
  struct mu_mboxrb_mailbox *dmp;
  size_t *ref;
  size_t mesg_count;
};

static int
tracker_init (struct mu_mboxrb_flush_tracker *trk,
	      struct mu_mboxrb_mailbox *dmp)
{
  trk->ref = calloc (dmp->mesg_count, sizeof (trk->ref[0]));
  if (!trk->ref)
    return ENOMEM;
  trk->dmp = dmp;
  trk->mesg_count = 0;
  return 0;
}

static void
tracker_free (struct mu_mboxrb_flush_tracker *trk)
{
  free (trk->ref);
}

static struct mu_mboxrb_message *
tracker_next_ref (struct mu_mboxrb_flush_tracker *trk, size_t orig_num)
{
  trk->ref[trk->mesg_count++] = orig_num;
  return trk->dmp->mesg[orig_num];
}

static void
mboxrb_tracker_sync (struct mu_mboxrb_flush_tracker *trk)
{
  struct mu_mboxrb_mailbox *dmp = trk->dmp;
  size_t i;

  if (trk->mesg_count == 0)
    {
      for (i = 0; i < dmp->mesg_count; i++)
	mu_mboxrb_message_free (dmp->mesg[i]);      
      dmp->size = 0;
      dmp->uidvalidity_scanned = 0;
    }
  else
    {
      /* Mark */
      for (i = 0; i < trk->mesg_count; i++)
	dmp->mesg[trk->ref[i]]->mark = 1;
      /* Sweep */
      for (i = 0; i < dmp->mesg_count; i++)
	if (!dmp->mesg[i]->mark)
	  mu_mboxrb_message_free (dmp->mesg[i]);
      /* Reorder */
      for (i = 0; i < trk->mesg_count; i++)
	{
	  dmp->mesg[i] = dmp->mesg[trk->ref[i]];
	  dmp->mesg[i]->mark = 0;
	}
      dmp->mesg_count = trk->mesg_count;
      dmp->size = dmp->mesg[dmp->mesg_count - 1]->message_end + 1;
    }
  dmp->mesg_count = trk->mesg_count;
  /* FIXME: Check uidvalidity values?? */
}

/* Write to the output stream DEST messages in the range [from,to).
   Update TRK accordingly.
*/
static int
mboxrb_mailbox_copy_unchanged (struct mu_mboxrb_flush_tracker *trk,
				size_t from, size_t to,
				mu_stream_t dest)
{
  if (to > from)
    {
      struct mu_mboxrb_mailbox *dmp = trk->dmp;
      mu_off_t start;
      mu_off_t stop;
      size_t i;
      mu_off_t off;
      int rc;

      start = dmp->mesg[from]->message_start;
      if (to == dmp->mesg_count)
	stop = dmp->mesg[to-1]->message_end + 1;
      else
	stop = dmp->mesg[to]->message_start;

      rc = mu_stream_seek (dest, 0, MU_SEEK_CUR, &off);
      if (rc)
	return rc;
      off -= start;
      /* Fixup offsets */
      for (i = from; i < to; i++)
	{
	  struct mu_mboxrb_message *ref = tracker_next_ref (trk, i);
	  ref->message_start += off;
	  ref->body_start += off;
	  ref->message_end += off;
	}

      /* Copy data */
      rc = mu_stream_seek (dmp->mailbox->stream, start, MU_SEEK_SET, NULL);
      if (rc)
	return rc;
      return mu_stream_copy (dest, dmp->mailbox->stream, stop - start, NULL);
    }
  return 0;
}

/* Flush the mailbox described by the tracker TRK to the stream TEMPSTR.
   First modified message is I (0-based). EXPUNGE is 1 if the
   MU_ATTRIBUTE_DELETED attribute is to be honored.
*/
static int
mboxrb_flush_temp (struct mu_mboxrb_flush_tracker *trk,
		    size_t i,
		    mu_stream_t tempstr, int expunge)
{
  struct mu_mboxrb_mailbox *dmp = trk->dmp;
  size_t start = 0;
  size_t save_imapbase = 0;
  size_t expcount = 0;
  int rc;

  rc = mu_stream_seek (trk->dmp->mailbox->stream, 0, MU_SEEK_SET, NULL);
  if (rc)
    return rc;
  while (i < dmp->mesg_count)
    {
      struct mu_mboxrb_message *dmsg = dmp->mesg[i];

      if (expunge && (dmsg->attr_flags & MU_ATTRIBUTE_DELETED))
	{
	  size_t expevt[2] = { i + 1, expcount };

	  rc = mboxrb_mailbox_copy_unchanged (trk, start, i, tempstr);
	  if (rc)
	    return rc;
	  mu_observable_notify (dmp->mailbox->observable,
				MU_EVT_MAILBOX_MESSAGE_EXPUNGE,
				expevt);
	  expcount++;
	  mu_message_destroy (&dmsg->message, dmsg);

	  /* Make sure uidvalidity and next uid are preserved even if
	     the first message (where they are saved) is deleted */
	  if (i == save_imapbase)
	    {
	      save_imapbase = i + 1;
	      if (save_imapbase < dmp->mesg_count)
		dmp->mesg[save_imapbase]->attr_flags |= MU_ATTRIBUTE_MODIFIED;
	    }
	  i++;
	  start = i;
	  continue;
	}

      if (dmsg->uid_modified
	  || (dmsg->attr_flags & MU_ATTRIBUTE_MODIFIED)
	  || mu_message_is_modified (dmsg->message))
	{
	  char *x_imapbase = NULL;
	  
	  rc = mboxrb_mailbox_copy_unchanged (trk, start, i, tempstr);
	  if (rc)
	    return rc;
	  if (save_imapbase == i)
	    {
	      mu_asprintf (&x_imapbase, "%*lu %*lu",
			   UINT_STRWIDTH (dmp->uidvalidity),
			   dmp->uidvalidity,
			   UINT_STRWIDTH (dmp->uidnext),
			   dmp->uidnext);
	    }
	  rc = mu_mboxrb_message_reconstruct (tempstr, dmsg,
					      tracker_next_ref (trk, i),
					      x_imapbase);
	  free (x_imapbase);
	  if (rc)
	    return rc;
	  i++;
	  start = i;
	  continue;
	}

      i++;
    }
  rc = mboxrb_mailbox_copy_unchanged (trk, start, i, tempstr);
  if (rc)
    return rc;
  if (trk->mesg_count)
    {
      rc = mu_stream_truncate (tempstr,
			       trk->dmp->mesg[trk->ref[trk->mesg_count - 1]]->message_end + 1);
    }
  
  return mu_stream_flush (tempstr);
}

/* Flush the mailbox described by the tracker TRK to the stream TEMPSTR.
   EXPUNGE is 1 if the MU_ATTRIBUTE_DELETED attribute is to be honored.
   Assumes that simultaneous access to the mailbox has been blocked.
*/
static int
mboxrb_flush_unlocked (struct mu_mboxrb_flush_tracker *trk, int mode)
{
  struct mu_mboxrb_mailbox *dmp = trk->dmp;
  int rc;
  size_t dirty;
  mu_stream_t tempstr;
  struct mu_tempfile_hints hints;
  int tempfd;
  char *tempname;
  char *p;

  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_TRACE1,
	    ("%s (%s)", __func__, dmp->name));
  if (dmp->mesg_count == 0)
    return 0;
  if (mode == FLUSH_UIDVALIDITY && !dmp->uidvalidity_changed)
    return 0;
  
  rc = mboxrb_refresh (dmp->mailbox);
  if (rc)
    return rc;

  if (dmp->uidvalidity_changed)
    {
      size_t i;
      char buf[X_IMAPBASE_MAX (dmp)];
      int n;
      mu_stream_t stream = dmp->mailbox->stream;

      /*
       * Format the X-IMAPbase header and check if it will fit in place
       * of the existing one (if any).  If so, write it at once and return.
       */
      n = snprintf (buf, sizeof (buf), "%s: %*lu %*lu",
		    MU_HEADER_X_IMAPBASE,
		    UINT_STRWIDTH (dmp->uidvalidity),
		    dmp->uidvalidity,
		    UINT_STRWIDTH (dmp->uidnext),
		    dmp->uidnext);
      
      if (dmp->x_imapbase_len && dmp->x_imapbase_len >= n)
	{
	  rc = mu_stream_seek (stream, dmp->x_imapbase_off, MU_SEEK_SET, NULL);
	  if (rc)
	    {
	      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			("%s:%s (%s): %s",
			 __func__, "mu_stream_seek", dmp->name,
			 mu_strerror (rc)));
	      return rc;
	    }
	  rc = mu_stream_printf (stream, "%-*s",
				 (int) dmp->x_imapbase_len,
				 buf);
	  if (rc)
	    {
	      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			("%s:%s (%s): %s",
			 __func__, "mu_stream_printf", dmp->name,
			 mu_strerror (rc)));
	      return rc;
	    }
	}
      else
	{
	  /*
	   * There is no X-IMAPbase header yet or it is not wide enough to
	   * accept the current value.  Fall back to reformatting entire
	   * mailbox.  Clear any other changes that might have been done
	   * to its messages.
	   */
	  dmp->mesg[0]->uid_modified = 1;

	  if (mode == FLUSH_UIDVALIDITY)
	    {
	      for (i = 1; i < dmp->mesg_count; i++)
		{
		  struct mu_mboxrb_message *dmsg = dmp->mesg[i];
		  dmsg->attr_flags &= ~(MU_ATTRIBUTE_MODIFIED|MU_ATTRIBUTE_DELETED);
		}
	    }
	}
    }

  for (dirty = 0; dirty < dmp->mesg_count; dirty++)
    {
      struct mu_mboxrb_message *dmsg = dmp->mesg[dirty];
      if (dmsg->uid_modified)
	break;
      if ((dmsg->attr_flags & MU_ATTRIBUTE_MODIFIED)
	  || (dmsg->attr_flags & MU_ATTRIBUTE_DELETED)
	  || (dmsg->message && mu_message_is_modified (dmsg->message)))
	break;
    }

  rc = 0;
  if (dirty < dmp->mesg_count)
    {
      p = strrchr (dmp->name, '/');
      if (p)
	{
	  size_t l = p - dmp->name;
	  hints.tmpdir = malloc (l + 1);
	  if (!hints.tmpdir)
	    return ENOMEM;
	  memcpy (hints.tmpdir, dmp->name, l);
	  hints.tmpdir[l] = 0;
	}
      else
	{
	  hints.tmpdir = mu_getcwd ();
	  if (!hints.tmpdir)
	    return ENOMEM;
	}
      rc = mu_tempfile (&hints, MU_TEMPFILE_TMPDIR, &tempfd, &tempname);
      if (rc)
	{
	  free (hints.tmpdir);
	  return rc;
	}
      rc = mu_fd_stream_create (&tempstr, tempname, tempfd,
				MU_STREAM_RDWR|MU_STREAM_SEEK);
      if (rc)
	{
	  free (hints.tmpdir);
	  close (tempfd);
	  free (tempname);
	  return rc;
	}
      
      rc = mboxrb_flush_temp (trk, dirty, tempstr, mode == FLUSH_EXPUNGE);
      mu_stream_unref (tempstr);
      if (rc == 0)
	{
	  /* Rename mailbox to temporary copy */
	  char *backup = mu_tempname (hints.tmpdir);
	  rc = rename (dmp->name, backup);
	  if (rc)
	    {
	      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			("%s:%s: failed to rename to backup file %s: %s",
			 __func__, dmp->name, tempname,
			 mu_strerror (rc)));
	      unlink (backup);
	    }
	  else
	    {
	      rc = rename (tempname, dmp->name);
	      if (rc == 0)
		{
		  /* Success. Synchronize internal data with the counter. */
		  mboxrb_tracker_sync (trk);
		  mu_stream_destroy (&dmp->mailbox->stream);
		  rc = mboxrb_mailbox_init_stream (dmp);
		}
	      else
		{
		  int rc1;
		  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			    ("%s: failed to rename temporary file %s %s: %s",
			     __func__, tempname, dmp->name,
			     mu_strerror (rc)));
		  rc1 = rename (backup, dmp->name);
		  if (rc1)
		    {
		      mu_error (_("failed to restore %s from backup %s: %s"),
				dmp->name, backup, mu_strerror (rc1));
		      mu_error (_("backup left in %s"), backup);
		      free (backup);
		      backup = NULL;
		    }
		}
	    }
	  
	  if (backup)
	    {
	      unlink (backup);
	      free (backup);
	    }
	}
      unlink (tempname);
      free (tempname);
      free (hints.tmpdir);
    }
  
  dmp->uidvalidity_changed = 0;  
  
  return rc;
}

/* Flush the changes in the mailbox DMP to disk storage.
   EXPUNGE is 1 if the MU_ATTRIBUTE_DELETED attribute is to be honored.
   Block simultaneous access for the duration of the process.

   This is done by creating a temporary mailbox on the same device as
   DMP and by transferring all messages (whether changed or not) to
   it. If the process succeeds, old mailbox is removed and the temporary
   one is renamed to it. In case of failure, the temporary is removed and
   the original mailbox remains unchanged.
*/
static int
mboxrb_flush (struct mu_mboxrb_mailbox *dmp, int mode)
{
  int rc;
  sigset_t signalset;
#ifdef WITH_PTHREAD
  int state;
#endif
  struct mu_mboxrb_flush_tracker trk;

  /* Lock the mailbox */
  if (dmp->mailbox->locker
      && (rc = mu_locker_lock (dmp->mailbox->locker)) != 0)
    return rc;

#ifdef WITH_PTHREAD
  pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state);
#endif
  sigemptyset (&signalset);
  sigaddset (&signalset, SIGTERM);
  sigaddset (&signalset, SIGHUP);
  sigaddset (&signalset, SIGTSTP);
  sigaddset (&signalset, SIGINT);
  sigaddset (&signalset, SIGWINCH);
  sigprocmask (SIG_BLOCK, &signalset, 0);

  rc = tracker_init (&trk, dmp);
  if (rc == 0)
    {
      rc = mboxrb_flush_unlocked (&trk, mode);
      tracker_free (&trk);
    }

#ifdef WITH_PTHREAD
  pthread_setcancelstate (state, &state);
#endif
  sigprocmask (SIG_UNBLOCK, &signalset, 0);

  if (dmp->mailbox->locker)
    mu_locker_unlock (dmp->mailbox->locker);
  return rc;
}

static int
mboxrb_expunge (mu_mailbox_t mailbox)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  return mboxrb_flush (dmp, FLUSH_EXPUNGE);
}

static int
mboxrb_sync (mu_mailbox_t mailbox)
{
  struct mu_mboxrb_mailbox *dmp = mailbox->data;
  return mboxrb_flush (dmp, FLUSH_SYNC);
}

int
mu_mboxrb_mailbox_init (mu_mailbox_t mailbox)
{
  int status;
  struct mu_mboxrb_mailbox *dmp;
  mu_property_t property = NULL;

  if (mailbox == NULL)
    return EINVAL;

  /* Allocate specific mbox data.  */
  dmp = calloc (1, sizeof (*dmp));
  if (dmp == NULL)
    return ENOMEM;

  /* Back pointer.  */
  dmp->mailbox = mailbox;
  dmp->uidnext = 1;
  
  status = mu_url_aget_path (mailbox->url, &dmp->name);
  if (status)
    {
      free (dmp);
      return status;
    }

  mailbox->data = dmp;

  /* Overloading the defaults.  */
  mailbox->_destroy = mboxrb_destroy;
  mailbox->_open = mboxrb_open;
  mailbox->_close = mboxrb_close;
  mailbox->_remove = mboxrb_remove;
  mailbox->_scan = mboxrb_scan;
  mailbox->_is_updated = mboxrb_is_updated;

  mailbox->_get_message = mboxrb_get_message;
  mailbox->_quick_get_message = mboxrb_quick_get_message;
  mailbox->_messages_count = mboxrb_messages_count;
  mailbox->_messages_recent = mboxrb_messages_recent;
  mailbox->_message_unseen = mboxrb_message_unseen;

  mailbox->_append_message = mboxrb_append_message;

  mailbox->_expunge = mboxrb_expunge;
  mailbox->_sync = mboxrb_sync;

  mailbox->_get_uidvalidity = mboxrb_get_uidvalidity;
  mailbox->_set_uidvalidity = mboxrb_set_uidvalidity;
  mailbox->_uidnext = mboxrb_uidnext;
  mailbox->_get_size = mboxrb_get_size;
  mailbox->_get_atime = mboxrb_get_atime;

  mu_mailbox_get_property (mailbox, &property);
  mu_property_set_value (property, "TYPE", "MBOX", 1);

  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_TRACE1,
	    ("%s (%s)", __func__, dmp->name));
  return 0;
}

/* Folder support */

/* Return MU_FOLDER_ATTRIBUTE_FILE if NAME looks like a mboxrb
   mailbox.

   If MU_AUTODETECT_ACCURACY is 0 (i.e. autodetection is disabled),
   always returns MU_FOLDER_ATTRIBUTE_FILE.
   
   Otherwise, the function analyzes first 128 bytes from file. If they
   look like a message header start, i.e. match "^[A-Za-z_][A-Za-z0-9_-]*:",
   then the file is considered a mboxrb mailbox.

   Additionally, if MU_AUTODETECT_ACCURACY is greater than 1, the last
   3 characters of the file are considered. For valid mboxrb they must
   be "\n.\n".
*/
static int
mboxrb_detect (char const *name)
{
  int res = 0;
  
  if (mu_autodetect_accuracy () == 0)
    res = MU_FOLDER_ATTRIBUTE_FILE;
  else
    {
      int rc;
      mu_stream_t str = NULL;

      rc = mu_file_stream_create (&str, name, MU_STREAM_READ);
      if (rc == 0)
	{
	  char *buf = NULL;
	  size_t size = 0;
	  size_t n;
      
	  rc = mu_stream_getline (str, &buf, &size, &n);
	  if (rc == 0)
	    {
	      char *zn;
	      if (parse_from_line (buf, &zn))
		{
		  res = MU_FOLDER_ATTRIBUTE_FILE;
		}
	    }
	  free (buf);
	}
    }
  return res;
}

static int
mboxrb_is_scheme (mu_record_t record, mu_url_t url, int flags)
{
  int rc = 0;
  int scheme_matched = mu_url_is_scheme (url, record->scheme);
  if (scheme_matched || mu_scheme_autodetect_p (url))
    {
      struct stat st;
      const char *path;

      mu_url_sget_path (url, &path);
      if (stat (path, &st) < 0)
	{
	  if (errno == ENOENT)
	    {
	      if (scheme_matched)
		return MU_FOLDER_ATTRIBUTE_FILE & flags;
	    }
	  return 0;
	}

      if (S_ISREG (st.st_mode) || S_ISCHR (st.st_mode))
	{
	  if (st.st_size == 0)
	    {
	      rc |= MU_FOLDER_ATTRIBUTE_FILE;
	    }
	  else if (flags & MU_FOLDER_ATTRIBUTE_FILE)
	    {
	      rc |= mboxrb_detect (path);
	    }
	}

      if ((flags & MU_FOLDER_ATTRIBUTE_DIRECTORY)
	  && S_ISDIR (st.st_mode))
	rc |= MU_FOLDER_ATTRIBUTE_DIRECTORY;
    }
  return rc;
}

static struct _mu_record _mboxrb_record =
{
  MU_MBOX_PRIO,
  "mbox",
  MU_RECORD_LOCAL,
  MU_URL_SCHEME | MU_URL_PATH | MU_URL_PARAM,
  MU_URL_PATH,
  mu_url_expand_path, /* URL init. */
  mu_mboxrb_mailbox_init, /* Mailbox init.  */
  NULL, /* Mailer init.  */
  _mu_fsfolder_init, /* Folder init.  */
  NULL, /* No need for back pointer.  */
  mboxrb_is_scheme, /* _is_scheme method.  */
  NULL, /* _get_url method.  */
  NULL, /* _get_mailbox method.  */
  NULL, /* _get_mailer method.  */
  NULL  /* _get_folder method.  */
};
mu_record_t mu_mbox_record = &_mboxrb_record;
