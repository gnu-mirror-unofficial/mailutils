/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2020 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library.  If not, see
   <http://www.gnu.org/licenses/>. */

/* First draft by Sergey Poznyakoff */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <limits.h>

#ifdef WITH_PTHREAD
# ifdef HAVE_PTHREAD_H
#  ifndef _XOPEN_SOURCE
#   define _XOPEN_SOURCE  500
#  endif
#  include <pthread.h>
# endif
#endif

#include <string.h>
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <mailutils/attribute.h>
#include <mailutils/body.h>
#include <mailutils/debug.h>
#include <mailutils/envelope.h>
#include <mailutils/error.h>
#include <mailutils/header.h>
#include <mailutils/message.h>
#include <mailutils/util.h>
#include <mailutils/property.h>
#include <mailutils/stream.h>
#include <mailutils/url.h>
#include <mailutils/observer.h>
#include <mailutils/errno.h>
#include <mailutils/locker.h>
#include <mailutils/sys/mailbox.h>
#include <mailutils/sys/registrar.h>
#include <mailutils/sys/amd.h>
#include <mailutils/io.h>
#include <mailutils/cstr.h>
#include <maildir.h>

#ifndef PATH_MAX 
# define PATH_MAX _POSIX_PATH_MAX
#endif

/* The maildir mailbox */
struct _maildir_data
{
  struct _amd_data amd;
  int folder_fd;         /* Descriptor of the top-level maildir directory */
  /* Additional data used during scanning: */
  int needs_attribute_fixup; /* The mailbox is created by mailutils 3.10
				or earlier and needs the attribute fixup
				(see "Maildir attribute fixup"" below) */
  int needs_uid_fixup;       /* Messages in the mailbox need to be assigned
				new UIDS.  Consequently, the uidvalidity
				value must be updated too. */
  unsigned long next_uid;    /* Next UID value. */
};
  
struct _maildir_message
{
  struct _amd_message amd_message;
  int subdir;
  char *file_name;  /* File name */
  size_t uniq_len;  /* Length of the unique file name prefix. */
  unsigned long uid;
};

static char *subdir_name[] = { "cur", "new", "tmp" };

char const *
mu_maildir_subdir_name (int subdir)
{
  if (subdir < 0 || subdir >= MU_ARRAY_SIZE (subdir_name))
    {
      errno = EINVAL;
      return NULL;
    }
  return subdir_name[subdir];
}

int
mu_maildir_reserved_name (char const *name)
{
  return strcmp (name, subdir_name[SUB_TMP]) == 0
         || strcmp (name, subdir_name[SUB_CUR]) == 0
         || strcmp (name, subdir_name[SUB_NEW]) == 0
	 || (strlen (name) > 3
             && (memcmp (name, ".mh", 3) == 0 || memcmp (name, ".mu", 3) == 0));
}

/*
 * Attribute handling.
 */

static struct info_map {
  char letter;
  int flag;
} info_map[] = {
  /* Draft: the user considers this message a draft; toggled at user
     discretion.  */
  { 'D', MU_ATTRIBUTE_DRAFT },
  /* Flagged: user-defined flag; toggled at user discretion. */
  { 'F', MU_ATTRIBUTE_FLAGGED },
  /* Passed: the user has resent/forwarded/bounced this message to
     someone else. */
  { 'P', MU_ATTRIBUTE_FORWARDED }, 
  /* Replied: the user has replied to this message. */
  { 'R', MU_ATTRIBUTE_ANSWERED },
  /* Seen: the user has viewed this message, though perhaps he didn't
     read all the way through it.
     This corresponds to the MU_ATTRIBUTE_READ attribute. */
  { 'S', MU_ATTRIBUTE_READ },
  /* Trashed: the user has moved this message to the trash; the trash
     will be emptied by a later user action. */
  { 'T', MU_ATTRIBUTE_DELETED },
  
  /* Mailutils versions prior to 3.10.90 marked replied messages with the
     'a' flag.  The discrepancy with the de-facto standard was reported
     in http://savannah.gnu.org/bugs/?56428.

     This entry is preserved for a while, for backward compatibility.

     Keep it at the end, so the 'R' letter is used when converting
     attributes to maildir info letters.

     Info flags are fixed in maildir_scan_unlocked.
  */
  { 'a', MU_ATTRIBUTE_ANSWERED },
};
#define info_map_size (MU_ARRAY_SIZE (info_map))

/* NOTE: BUF must be at least info_map_size bytes long */
static int
flags_to_info (int flags, char *buf)
{
  struct info_map *p;
  
  for (p = info_map; p < info_map + info_map_size; p++)
    {
      if (p->flag & flags)
	*buf++ = p->letter;
      /* Avoid handling the same flag again.  See the description of
	 'a' in info_map above and "Maildir attribute fixup" below. */
      flags &= ~p->flag;
    }
  *buf = 0;
  return 0;
}

static int
info_to_flags (char const *buf)
{
  int flags = 0;
  struct info_map *p;

  for (p = info_map; p < info_map + info_map_size; p++)
    if (strchr (buf, p->letter))
      flags |= p->flag;
  return flags;
}

/*
 * Maildir message file handling.
 * ==============================
 *
 * The format of the maildir message file name is described in:
 * http://cr.yp.to/proto/maildir.html.
 *
 * It consists of:
 *
 *   uniq [ ',' attrs ] ':2,' info
 *
 * where:
 *   uniq  - unique name prefix;
 *   info  - message flags in alphabetical order;
 *   attrs - implementation-defined attributes.
 *
 * The specification states that a "unique name can be anything that
 * doesn't contain a colon (or slash) and doesn't start with a dot."
 * This makes it possible to add to the unique prefix a list of comma-
 * separated attributes.  Each attribute consists of the attribute name,
 * immediately followed by '=' and attribute value.  Neither part can
 * contain '=' or ','.
 *
 * Mailutils implementation defines the following attributes:
 *
 *  u  -  UID of the message.
 *
 */

/*
 * Each attribute is represented by the following structure:
 */  
struct attrib
{
  char *name;             /* Attribute name */
  char *value;            /* Attribute value */
  struct attrib *next;    /* Pointer to the next attribute in list. */
};

/*
 * Both name and value point to the memory allocated in one block with
 * the attrib structure.  Attributes form a singly-linled list with the
 * list head pointing to the first attribute defined in the file name.
 *
 * Functions for accessing attribute lists:
 */

/* Push new attribute at the head of the existing attribute list.

   head - Pointer to the current list head;
   name - Attribute name;
   nlen - Length of the name;
   val  - Value;
   vlen - Length of the value.

   On success, updates head and returns the new head attribute.
   On memory allocation error, returne NULL and does not modify head.
*/
static inline struct attrib *
attrib_push (struct attrib **head, char const *name, size_t nlen,
	     char const *val, size_t vlen)
{
  struct attrib *attr;

  attr = malloc (sizeof (*attr) + nlen + vlen + 2);
  attr->next = *head;
  attr->name = (char*)(attr + 1);
  memcpy(attr->name, name, nlen);
  attr->name[nlen] = 0;
  attr->value = attr->name + nlen + 1;
  memcpy(attr->value, val, vlen);
  attr->value[vlen] = 0;
  *head = attr;
  return attr;
}

/* Free the attribute list */
static void
attrib_free (struct attrib *alist)
{
  while (alist)
    {
      struct attrib *next = alist->next;
      free (alist);
      alist = next;
    }
}

/* If the attribute name is present in the list alist, return its value.
   Otherwise, return NULL. */
static char const *
attrib_lookup (struct attrib *alist, char const *name)
{
  while (alist)
    {
      if (strcmp (alist->name, name) == 0)
	return alist->value;
      alist = alist->next;
    }
  return NULL;
}

/* Auxiliary function.  Returns 1 if the string STR of length LEN is
   encountered in the NULL-terminated string list STRLIST, and 0 otherwise.
*/
static inline int
is_member_of (char const *str, size_t len, char **strlist)
{
  char const *p;
  
  if (!strlist)
    return 1;
  while ((p = *strlist) != NULL)
    {
      if (strlen (p) == len && memcmp (p, str, len) == 0)
	return 1;
      strlist++;
    }
  return 0;
}

/*
 * Message name parser.
 *
 * Alphabet:
 *      0   ANY
 *      1   ','
 *      2   '1'
 *      3   '2'
 *      4   ':'
 *      5   '='
 *
 * Transitions:
 *
 *      \      input
 *      state
 *      
 *      \  0 1 2 3 4 5
 *      -+ -----------
 *      0| 0 1 0 0 0 0
 *      1| 0 0 2 3 0 0
 *      2| 0 0 0 0 4 0 
 *      3| 0 0 0 0 5 0
 *      4| 6 8 6 6 6 8   experimental semantics: any except ,=
 *      5| 6 8 6 6 6 8   flags : any except ,=
 *      6| 6 8 6 6 6 7   consume value; ends at =
 *      7| 7 6 7 7 7 8   consume keyword; ends at ,
 *      8|<stop>
 */

/* Convert input character to alphabet symbol code. */
static inline int
alpha (int input)
{
  switch (input)
    {
    case ',':
      return 1;

    case '1':
      return 2;

    case '2':
      return 3;

    case ':':
      return 4;

    case '=':
      return 5;
    }

  return 0;
}

/* Extract information from the maildir message name NAME.
   
   Input:
     name      - Message file name.
     attrnames - Pointer to the array of attribute names we are interested in.
                 NULL means all attributes, if attrs is not NULL.

   Output:
     flags     - Parsed out flags.  Can be NULL.
     attrs     - Linked list of extracted attributes.  

   If attrs is NULL, no attributes are returned and attrnames is not referenced.
     
   Returns length of the unique name prefix, or (size_t)-1 if memory
   allocation fails.
*/
static size_t
maildir_message_name_parse (char const *name, char **attrnames,
			    int *flags, struct attrib **attrs)
{
  char const *p = name + strlen (name);
  static int transition[][6] = {
    { 0, 1, 0, 0, 0, 0 },
    { 0, 0, 2, 3, 0, 0 },
    { 0, 0, 0, 0, 4, 0 }, 
    { 0, 0, 0, 0, 5, 0 },
    { 6, 8, 6, 6, 6, 8 },
    { 6, 8, 6, 6, 6, 8 },
    { 6, 8, 6, 6, 6, 7 },
    { 7, 6, 7, 7, 7, 8 },
  };
  int state = 0, oldstate;
  char const *endp = p;
  char const *startval;
  char const *endval;
  struct attrib *athead = NULL;
  size_t len;
  int f = 0;
  
  while (p > name)
    {
      p--;
      oldstate = state;
      state = transition[state][alpha(*p)];
      switch (state)
	{
	case 4:
	  endp = p;
	  endval = p;
	  f = 0;
	  break;

	case 5:
	  endp = p;
	  endval = p;
	  f = info_to_flags (p + 3);
	  break;

	case 6:
	  if (oldstate == 7)
	    {
	      len = startval - p - 2;
	      if (attrs && is_member_of (p + 1, len, attrnames))
		{
		  if (!attrib_push (&athead, p + 1, len,
				    startval, endval - startval))
		    {
		      attrib_free (athead);
		      return (size_t)-1;
		    }
		}
	      endp = p;
	      endval = p;
	    }
	  else if (oldstate != state)
	    endval = p + 1;
	  break;

	case 7:
	  if (oldstate != state)
	    startval = p + 1;
	  break;

	case 8:
	  /* error */
	  if (endval)
	    endp = endval;
	  else
	    endp = p + 2;
	  goto end;
	}
    }
 end:
  if (flags)
    *flags = f;
  if (attrs)
    *attrs = athead;
  return endp - name;
}

static int
maildir_open (struct _maildir_data *md)
{
  if (md->folder_fd == -1)
    {
      int fd = open (md->amd.name, O_RDONLY | O_NONBLOCK | O_DIRECTORY);
      if (fd == -1)
	{
	  int rc = errno;
	  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		    ("can't open directory %s: %s",
		     md->amd.name, mu_strerror (rc)));
	  return rc;
	}
      else
	md->folder_fd = fd;
    }
  return 0;
}

static void
maildir_close (struct _maildir_data *md)
{
  if (md->folder_fd == -1)
    {
      close (md->folder_fd);
      md->folder_fd = -1;
    }
}

/* Open subdirectory SUBDIR in MD.
   Note: maildir must be open.
 */
static int
maildir_subdir_open (struct _maildir_data *md, int subdir,
		     DIR **pdir, int *pfd)
{
  int rc;
  int fd;
  DIR *dir;
  
  fd = openat (md->folder_fd, subdir_name[subdir],
	       O_RDONLY | O_NONBLOCK | O_DIRECTORY);
  if (fd == -1)
    {
      if (errno == ENOENT)
	{
	  int perms = PERMS | mu_stream_flags_to_mode (md->amd.mailbox->flags, 1);
	  if (mkdirat (md->folder_fd, subdir_name[subdir], perms) == 0)
	    {
	      fd = openat (md->folder_fd, subdir_name[subdir],
			   O_RDONLY | O_NONBLOCK | O_DIRECTORY);
	    }
	  else
	    {
	      rc = errno;
	      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			("can't create directory %s/%s: %s",
			 md->amd.name, subdir_name[subdir], mu_strerror (rc)));
	      return rc;
	      
	    }
	}

      if (fd == -1)
	{
	  rc = errno;
	  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		    ("can't open directory %s/%s: %s",
		     md->amd.name, subdir_name[subdir], mu_strerror (rc)));
	  return rc;
	}
    }

  if (pdir)
    {
      dir = fdopendir (fd);
      if (!dir)
	{
	  rc = errno;
	  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		    ("can't fdopen directory %s/%s: %s",
		     md->amd.name, subdir_name[subdir], mu_strerror (rc)));
	  close (fd);
	  return rc;
	}

      *pdir = dir;
    }
  *pfd = fd;
  
  return 0;
}

static int
maildir_message_alloc (struct _maildir_data *md, int subdir, char const *name,
		       struct _maildir_message **pmsg)
{
  struct _maildir_message *msg;
  size_t n;
  static char *attrnames[] = { "a", "u", NULL };
  struct attrib *attrs;
  char const *p;
  
  msg = calloc (1, sizeof (*msg));
  if (!msg)
    return errno;
  msg->subdir = subdir;
  msg->file_name = strdup (name);
  if (!msg->file_name)
    {
      free (msg);
      return ENOMEM;
    }
  
  n = maildir_message_name_parse (name, attrnames,
				  &msg->amd_message.attr_flags,
				  &attrs);
  if (n == (size_t)-1)
    {
      free (msg->file_name);
      free (msg);
      return ENOMEM;
    }
  msg->uniq_len = n;
  
  if ((p = attrib_lookup (attrs, "a")) != NULL)
    {
      /* NOTICE: mu_attribute_string_to_flags preserves existing bits
	 in its second argument. */
      mu_attribute_string_to_flags (p, &msg->amd_message.attr_flags);
    }
  
  if ((p = attrib_lookup (attrs, "u")) != NULL)
    {
      char *endp;
      unsigned long n = strtoul (p, &endp, 10);
      if ((n == ULONG_MAX && errno == ERANGE) || *endp)
	/* The uid remains set to 0 and will be fixed up later */;
      else
	msg->uid = n;
    }

  attrib_free (attrs);
  *pmsg = msg;
  return 0;
}

static void
maildir_message_free (struct _amd_message *amsg)
{
  struct _maildir_message *mp = (struct _maildir_message *) amsg;
  if (mp)
    free (mp->file_name);
}

/*
 * Maldir scanning
 */
int
maildir_tmp_flush (struct _maildir_data *md)
{
  int rc;
  int fd;
  DIR *dir;
  struct dirent *entry;
    
  if (!(md->amd.mailbox->flags & MU_STREAM_WRITE))
    return 0; /* Do nothing in read-only mode */

  rc = maildir_subdir_open (md, SUB_TMP, &dir, &fd);
  if (rc)
    return rc;

  while ((entry = readdir (dir)))
    {
      switch (entry->d_name[0])
	{
	case '.':
	  break;

	default:
	  unlinkat (fd, entry->d_name, 0);
	  break;
	}
    }

  closedir (dir);
  return 0;
}

static int
maildir_subdir_scan (struct _maildir_data *md, int subdir)
{
  int rc;
  int fd;
  DIR *dir;
  struct dirent *entry;

  rc = maildir_subdir_open (md, subdir, &dir, &fd);
  if (rc)
    return rc;

  while ((entry = readdir (dir)))
    {
      struct stat st;
      struct _maildir_message *msg;
      size_t index;
      
      if (entry->d_name[0] == '.')
	continue;

      if (fstatat (fd, entry->d_name, &st, 0))
	{
	  if (errno != ENOENT)
	    {
	      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			("can't stat %s/%s/%s: %s",
			 md->amd.name, subdir_name[subdir], entry->d_name,
			 mu_strerror (errno)));
	    }
	  continue;
	}

      if (!S_ISREG (st.st_mode))
	continue;

      msg = calloc (1, sizeof (*msg));
      if (!msg)
	{
	  rc = ENOMEM;
	  break;
	}

      rc = maildir_message_alloc (md, subdir, entry->d_name, &msg);

      if (!amd_msg_lookup (&md->amd, (struct _amd_message *) msg, &index))
	{
	  /* should not happen */
	  maildir_message_free ((struct _amd_message *) msg);
	  continue;
	}

      rc = _amd_message_append (&md->amd, (struct _amd_message *) msg);
      if (rc)
	{
	  maildir_message_free ((struct _amd_message *) msg);
	  break;
	}
    }
  
  closedir (dir);
  return 0;
}

/*
 * Maildir attribute fixup
 * =======================
 *
 * Messag info flags used by Mailutils versions prior to 3.10.90 differ
 * from those described in the reference maildir implementation
 * (http://cr.yp.to/proto/maildir.html)
 * This was reported in http://savannah.gnu.org/bugs/?56428.
 *
 * The difference is summarized in the table below:
 *
 *  Attribute              Reference  MU-3.10
 *  ---------------------  ---------  -------
 *  MU_ATTRIBUTE_READ          S         R
 *  MU_ATTRIBUTE_ANSWERED      R         a
 *  MU_ATTRIBUTE_SEEN          -         S
 *
 * The "Attribute" column lists the maiutils attribute flag, the
 * "Reference" column contains the letter that corresponds to that flag
 * in the reference implementation and the "MU-3.10" column shows the
 * letter in mailutils 3.10 and earloer.
 *
 * Starting from version 3.10.90 this discrepancy is fixed.  To make sure
 * both old and new message attributes are correctly recognized, we try 
 * to detect whether the mailbox was created by mailutils versions prior to
 * 3.10.90 and fix up the attributes if so.  This is done after the initial
 * mailbox scan.
 *
 * To detect whether attributes need to be fixed, the .mu-prop file is
 * examined.  If it does not exist, it is assumed that the mailbox was
 * not created by mailutils and no fixup is needed.  If it exists and
 * does not contain the "version" property, or if the value of that
 * property is less than or equal to 3.10, then the fixup is needed.
 * Otherwise, fixup is not needed.  This is done by the needs_fixup
 * function.
 *
 * To assist in diagnostics, new AMD "capability" MU_AMD_PROP has been
 * added.  It is set, if the .mu-prop file existed when the mailbox
 * was opened.  The capability member was chosen to avoid adding new
 * members to struct _amd_data.
 *
 * During fixup, the internal attribute flags are changed according to
 * the table above.  If the mailbox is writable, each message file is
 * renamed if its attributes changed.  Otherwise, changes remain
 * in memory.  This is done by the maildir_attribute_fixup function.
 *
 * After successfully opening the (writable) mailbox, the "version"
 * property in its .mu-prop file is updated so that subsequent accesses
 * avoid performing the fixup again.
 *
 * The code below (up to the form feed character) will live for a couple
 * of releases after 3.11 to make sure all existing mailboxes have been
 * fixed up.
 */

static int
needs_fixup (struct _amd_data *amd)
{
  char const *amd_version;
  int rc;

  if (!(amd->capabilities & MU_AMD_PROP))
    {
      /* Absence of the mu-prop file indicates that this mailbox was
	 not created by mailutils (unless the file has been deleted,
	 of course).  Let's assume the former.  This means that no
	 attribute fixup is needed. */
      return 0;
    }

  if (amd->prop)
    {
      switch (mu_property_sget_value (amd->prop, "version", &amd_version))
	{
	case 0:
	  break;
	  
	case MU_ERR_NOENT:
	  return 1;

	default:
	  return 0;
	}
      if (mu_version_string_cmp (amd_version, "3.10", 0, &rc) == 0)
	return rc <= 0;
    }
  return 0;
}

static void
maildir_message_fixup (struct _maildir_data *md, struct _maildir_message *msg)
{
  int update = 0;
  
  if (md->needs_attribute_fixup)
    {
      int flags = msg->amd_message.attr_flags;

      if (flags & MU_ATTRIBUTE_READ)
	{
	  flags &= ~MU_ATTRIBUTE_READ;
	  flags |= MU_ATTRIBUTE_SEEN;
	}
      
      if (flags & MU_ATTRIBUTE_ANSWERED)
	{
	  flags &= ~MU_ATTRIBUTE_ANSWERED;
	  flags |= MU_ATTRIBUTE_READ;
	}
	  
      if (msg->amd_message.attr_flags != flags)
	{
	  msg->amd_message.attr_flags = flags;
	  update = 1;
	}
    }

  if (msg->uid == 0 || msg->uid < md->next_uid)
    md->needs_uid_fixup = 1;

  if (md->needs_uid_fixup)
    {
      msg->uid = md->next_uid++;
      update = 1;
    }
  else
    md->next_uid = msg->uid + 1;

  if (update && md->amd.mailbox->flags & MU_STREAM_WRITE)
    {  
      md->amd.chattr_msg (&msg->amd_message, 0);
    }
}

static int
maildir_scan_unlocked (mu_mailbox_t mailbox, size_t *pcount, int do_notify)
{
  struct _maildir_data *md = mailbox->data;
  int rc;
  char const *s;
  struct stat st;
  size_t i;
  int has_new = 0;
  int save_prop = 0;
  
  rc = maildir_open (md);
  if (rc)
    return rc;
  md->needs_attribute_fixup = needs_fixup (&md->amd);
  md->needs_uid_fixup = 0;
  md->next_uid = 1;
  
  /* Flush tmp/ */
  rc = maildir_tmp_flush (md);
  if (rc)
    goto err;

  /* Scan cur/ */
  rc = maildir_subdir_scan (md, SUB_CUR);
  if (rc)
    goto err;

  /* Scan new/ */
  rc = maildir_subdir_scan (md, SUB_NEW);
  if (rc)
    goto err;
  
  /* Sort messages */
  amd_sort (&md->amd);

  /* Fix up messages and send out dispatch notifications, if necessary. */
  for (i = 1; i <= md->amd.msg_count; i++)
    {
      struct _maildir_message *msg = (struct _maildir_message *)
	_amd_get_message (&md->amd, i);
      if (msg->subdir == SUB_NEW && md->amd.mailbox->flags & MU_STREAM_WRITE)
	{
	  if (md->amd.chattr_msg (&msg->amd_message, 0) == 0)
	    has_new = 1;
	}
      else
	{
	  if (has_new) /* should not happen */
	    md->needs_uid_fixup = 1;
	  maildir_message_fixup (md, msg);
	}
      if (do_notify)
	DISPATCH_ADD_MSG (mailbox, &md->amd, i);
    }

  /* Reset uidvalidity if any of the UIDs changed */
  if (md->needs_uid_fixup)
    {
      amd_reset_uidvalidity (&md->amd);
      save_prop = 1;
    }  
  
  /* Update version marker in the property file */
  if ((md->amd.mailbox->flags & MU_STREAM_WRITE) &&
      (mu_property_sget_value (md->amd.prop, "version", &s) ||
       strcmp (s, PACKAGE_VERSION)))
    {
      rc = mu_property_set_value (md->amd.prop, "version", PACKAGE_VERSION, 1);
      if (rc)
	{
	  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		    ("maildir_scan_dir: mu_property_set_value failed during attribute fixup: %s",
		     mu_strerror (rc)));
	}
      save_prop = 1;
    }

  if (save_prop)
    {
      rc = mu_property_save (md->amd.prop);
      if (rc)
	{
	  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		    ("maildir_scan_dir: mu_property_save failed during attribute fixup: %s",
		     mu_strerror (rc)));
	}
      rc = 0;
    }

  if (stat (md->amd.name, &st) == 0)
    md->amd.mtime = st.st_mtime;
  else
    md->amd.mtime = time (NULL);
  if (rc == 0 && pcount)
    *pcount = md->amd.msg_count;

 err:
  maildir_close (md);

  return rc;
}

/*
 * Functions for generating the unique part of the filename.
 */

/* Auxiliary functions operate on a buffer of the following structure: */
struct string_buffer
{
  char *base;   /* Buffer pointer. */
  size_t size;  /* Number of bytes allocated for base. */
  size_t off;   /* Offset of the first uninitialized byte. */
};

#define STRING_BUFFER_INITIALIZER { NULL, 0, 0 }

static void
string_buffer_free (struct string_buffer *buf)
{
  free (buf->base);
}

/* Expand the buffer to approx. 3/2 of its current size. */
static int
string_buffer_expand (struct string_buffer *buf)
{
  size_t n;
  char *p;
  
  if (!buf->base)
    {
      n = 64;
    }
  else
    {
      n = buf->size;
      
      if ((size_t) -1 / 3 * 2 <= n)
	return ENOMEM;

      n += (n + 1) / 2;
    }
  p = realloc (buf->base, n);
  if (!p)
    return ENOMEM;
  buf->base = p;
  buf->size = n;
  return 0;
}

/* Append LEN bytes from S to the buffer. */
static int
string_buffer_append (struct string_buffer *buf, char const *s, size_t len)
{
  while (buf->off + len > buf->size)
    {
      if (string_buffer_expand (buf))
	return ENOMEM;
    }
  memcpy (buf->base + buf->off, s, len);
  buf->off += len;
  return 0;
}

/* Append nul-terminated string S. */
static inline int
string_buffer_appendz (struct string_buffer *buf, char const *s)
{
  return string_buffer_append (buf, s, strlen (s));
}

static char const digits[] = "0123456789ABCDEF";

/* Format the number N to the buffer.  BASE must be 10 or 16, no other
   value is allowed. */
static int
string_buffer_format_long (struct string_buffer *buf, unsigned long n,
			   int base)
{
  size_t start = buf->off;
  char *p, *q;
  do
    {
      if (string_buffer_append (buf, &digits[n % base], 1))
	return ENOMEM;
      n /= base;
    }
  while (n > 0);
  p = buf->base + start;
  q = buf->base + buf->off - 1;
  while (p < q)
    {
      int t = *q;
      *q = *p;
      *p = t;
      p++;
      q--;
    }
  return 0;
}

/* Format this host name to the buffer. */
static int
string_buffer_format_hostname (struct string_buffer *buf)
{
  size_t start = buf->off;
  size_t len, i;
  
  while (gethostname (buf->base + buf->off, buf->size - buf->off) != 0)
    {
      if (errno != 0 && errno != ENAMETOOLONG && errno != EINVAL
	  && errno != ENOMEM)
	return errno;
      if (string_buffer_expand (buf))
	return ENOMEM;
    }

  len = strlen (buf->base + buf->off);
  buf->off += len;
  
  /* encode '/', ':', '.', and ',' */
  for (i = start; i < buf->off; i++)
    {
      switch (buf->base[i])
	{
	case '/':
	case ':':
	case ',':
	  break;

	default:
	  continue;
	}

      while (buf->off + 3 > buf->size)
	{
	  if (string_buffer_expand (buf))
	    return ENOMEM;
	}
      memmove (buf->base + i + 4, buf->base + i + 1, buf->off - i - 1);
      buf->base[i+1] = digits[(buf->base[i] >> 6) & 7];
      buf->base[i+2] = digits[(buf->base[i] >> 3) & 7];
      buf->base[i+3] = digits[buf->base[i] & 7];
      buf->base[i] = '\\';
      i += 3;
      buf->off += 3;
    }

  return 0;
}

/* Format message flags to buffer. */
static int
string_buffer_format_flags (struct string_buffer *buf, int flags)
{
  int rc;
  char fbuf[info_map_size + 1];

  flags_to_info (flags, fbuf);
  if ((rc = string_buffer_append (buf, ":2,", 3)) != 0)
    return rc;
  return string_buffer_appendz (buf, fbuf);
}

/* Format mailutils-specific attribute flags to the string buffer.
   Mailutils-specific flags are the flags that have no corresponding
   info letter in the maildir memo.  These flags are encoded in the
   'a' attribute setting.
*/
static int
string_buffer_format_mu_flags (struct string_buffer *buf, int flags)
{
  int rc = 0;
  char fb[MU_STATUS_BUF_SIZE];
  size_t len;
  
  mu_attribute_flags_to_string (flags & (MU_ATTRIBUTE_FLAGGED|MU_ATTRIBUTE_SEEN),
				fb, sizeof (fb), &len);
  if (len > 0)
    {
      if ((rc = string_buffer_append (buf, ",a=", 3)) == 0)
	rc = string_buffer_append (buf, fb, len);
    }
  return rc;
}

static int
string_buffer_format_message_name (struct string_buffer *buf,
				   struct _maildir_message *msg,
				   int flags)
{
  int rc;
  
  if ((rc = string_buffer_append (buf, msg->file_name, msg->uniq_len)) == 0 &&
      (rc = string_buffer_format_mu_flags (buf, flags)) == 0 &&
      (rc = string_buffer_append (buf, ",u=", 3)) == 0 &&
      (rc = string_buffer_format_long (buf, msg->uid, 10)) == 0 &&
      (rc = string_buffer_format_flags (buf, flags)) == 0)
    ;
  return rc;
}

static int
read_random (void *buf, size_t size)
{
  int rc;
  int fd = open ("/dev/urandom", O_RDONLY);
  if (fd == -1)
    return -1;
  rc = read (fd, buf, size);
  close (fd);
  return rc != size;
}

/* Create unique part.  If FD is supplied, use its inode and dev numbers
   as part of the created string.  Return the allocated string or NULL on
   memory shortage.
 */
static char *
maildir_uniq_create (struct _amd_data *amd, int fd)
{
  struct string_buffer sb = STRING_BUFFER_INITIALIZER;
  struct timeval tv;
  unsigned long n;
  char *ret;
  int rc;
  struct stat st;
  
  gettimeofday (&tv, NULL);
  rc = string_buffer_format_long (&sb, (unsigned long) tv.tv_sec, 10);
  if (rc)
    goto err;

  rc = string_buffer_append (&sb, ".", 1);
  if (rc)
    goto err;
  
  if (read_random (&n, sizeof (unsigned long))) /* FIXME: 32 bits */
    {
      rc = string_buffer_append (&sb, "R", 1);
      if (rc)
	goto err;
      rc = string_buffer_format_long (&sb, n, 16);
      if (rc)
	goto err;
    }

  if (fd > 0 && fstat (fd, &st) == 0)
    {
      rc = string_buffer_append (&sb, "I", 1);
      if (rc)
	goto err;
      rc = string_buffer_format_long (&sb, st.st_ino, 16);
      if (rc)
	goto err;

      rc = string_buffer_append (&sb, "V", 1);
      if (rc)
	goto err;
      rc = string_buffer_format_long (&sb, st.st_dev, 16);
      if (rc)
	goto err;
    }
  
  rc = string_buffer_append (&sb, "M", 1);
  if (rc)
    goto err;
  rc = string_buffer_format_long (&sb, tv.tv_usec, 10);
  if (rc)
    goto err;
  
  rc = string_buffer_append (&sb, "P", 1);
  if (rc)
    goto err;
  rc = string_buffer_format_long (&sb, getpid (), 10);
  if (rc)
    goto err;

  rc = string_buffer_append (&sb, "Q", 1);
  if (rc)
    goto err;
  rc = string_buffer_format_long (&sb, amd->msg_count, 10);
  if (rc)
    goto err;

  rc = string_buffer_append (&sb, ".", 1);
  if (rc)
    goto err;
  rc = string_buffer_format_hostname (&sb);
  if (rc)
    goto err;
  
  rc = string_buffer_append (&sb, "", 1);
  
 err:
  if (rc == 0)
    {
      ret = sb.base;
      sb.base = NULL;
    }
  else
    ret = NULL;
  string_buffer_free (&sb);
  return ret;
}

/*
 * AMD interface functions.
 */
static int
maildir_cur_message_name (struct _amd_message *amsg, char **pname)
{
  struct _maildir_message *msg = (struct _maildir_message *) amsg;
  struct string_buffer sb = STRING_BUFFER_INITIALIZER;
  int rc;

  if ((rc = string_buffer_appendz (&sb, amsg->amd->name)) == 0 &&
      (rc = string_buffer_append (&sb, "/", 1)) == 0 &&
      (rc = string_buffer_appendz (&sb, subdir_name[msg->subdir])) == 0 &&
      (rc = string_buffer_append (&sb, "/", 1)) == 0 &&
      (rc = string_buffer_appendz (&sb, msg->file_name)) == 0 &&
      (rc = string_buffer_append (&sb, "", 1)) == 0)
    {
      *pname = sb.base;
      sb.base = NULL;
    }
  string_buffer_free (&sb);
  return rc;
}

static int
maildir_new_message_name (struct _amd_message *amsg, int flags, int expunge,
			  char **pname)
{
  struct _maildir_message *msg = (struct _maildir_message *) amsg;
  int rc = 0;
  
  if (expunge && (flags & MU_ATTRIBUTE_DELETED))
    {
      /* Force amd.c to unlink the file. */
      *pname = NULL;
    }
  else
    {
      struct string_buffer sb = STRING_BUFFER_INITIALIZER;
      if ((rc = string_buffer_appendz (&sb, amsg->amd->name)) == 0 &&
	  (rc = string_buffer_append (&sb, "/", 1)) == 0 &&
	  (rc = string_buffer_appendz (&sb, subdir_name[msg->subdir])) == 0 &&
	  (rc = string_buffer_append (&sb, "/", 1)) == 0)
	{
	  if (msg->subdir == SUB_CUR)
	    rc = string_buffer_format_message_name (&sb, msg, flags);
	  else
	    rc = string_buffer_appendz (&sb, msg->file_name);

	  if (rc == 0)
	    rc = string_buffer_append (&sb, "", 1);
	}

      if (rc == 0)
	{
	  *pname = sb.base;
	}
      else
	string_buffer_free (&sb);
    }
  return rc;
}

static int
maildir_create (struct _amd_data *amd, int flags)
{
  int rc;
  struct _maildir_data *md = (struct _maildir_data *)amd;
  
  rc = maildir_open (md);
  if (rc == 0) 
    {
      int i;
      
      for (i = 0; i < MU_ARRAY_SIZE (subdir_name); i++)
	{
	  int fd;
	  rc = maildir_subdir_open (md, i, NULL, &fd);
	  if (rc)
	    break;
	  close (fd);
	}
      maildir_close (md);
    }

  return rc;
}

static int
maildir_msg_finish_delivery (struct _amd_data *amd, struct _amd_message *amm,
			     const mu_message_t orig_msg)
{
  struct _maildir_data *md = (struct _maildir_data *)amd;
  struct _maildir_message *msg = (struct _maildir_message *) amm;
  mu_attribute_t attr;
  int flags;
  int rc;
  int src_fd = -1, dst_fd = -1;
  struct string_buffer sb = STRING_BUFFER_INITIALIZER;
  char const *newname;
  
  if (mu_message_get_attribute (orig_msg, &attr) == 0
      && mu_attribute_is_read (attr)
      && mu_attribute_get_flags (attr, &flags) == 0)
    {
      msg->subdir = SUB_CUR;
      rc = string_buffer_format_message_name (&sb, msg, flags);
      if (rc == 0)
	rc = string_buffer_append (&sb, "", 1);
      if (rc)
	{
	  string_buffer_free (&sb);
	  return rc;
	}
      newname = sb.base;
    }
  else
    {
      msg->subdir = SUB_NEW;
      newname = msg->file_name;
    }

  rc = maildir_open (md);
  if (rc)
    goto err;
    
  rc = maildir_subdir_open (md, SUB_TMP, NULL, &src_fd);
  if (rc)
    goto err;
  
  rc = maildir_subdir_open (md, msg->subdir, NULL, &dst_fd);
  if (rc)
    goto err;

  if (unlinkat (dst_fd, newname, 0) && errno != ENOENT)
    {
      rc = errno;
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("can't unlink %s/%s/%s: %s",
		 amd->name, subdir_name[msg->subdir],
		 newname, mu_strerror (rc)));
    }
  else if (linkat (src_fd, msg->file_name, dst_fd, newname, 0) == 0)
    {
      if (unlinkat (src_fd, msg->file_name, 0))
	{
	  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		    ("can't unlink %s/%s/%s: %s",
		     amd->name, subdir_name[SUB_TMP], msg->file_name,
		     mu_strerror (errno)));
	}
    }
  else
    {
      rc = errno;
      
      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		("renaming %s/%s to %s/%s in %s failed: %s",
		 subdir_name[SUB_TMP], msg->file_name,
		 subdir_name[msg->subdir], newname, 
		 amd->name, mu_strerror (rc)));
    }

 err:
  string_buffer_free (&sb);
  close (src_fd);
  close (dst_fd);
  maildir_close (md);
  
  return rc;
}
    
static int
maildir_scan0 (mu_mailbox_t mailbox, size_t msgno MU_ARG_UNUSED,
	       size_t *pcount, int do_notify)
{
  struct _amd_data *amd = mailbox->data;
  int rc;
  
  if (amd == NULL)
    return EINVAL;
  if (mailbox->flags & MU_STREAM_APPEND)
    return 0;
  mu_monitor_wrlock (mailbox->monitor);
  rc = maildir_scan_unlocked (mailbox, pcount, do_notify);
  mu_monitor_unlock (mailbox->monitor);
  return rc;
}

static int
maildir_qfetch (struct _amd_data *amd, mu_message_qid_t qid)
{
  struct _maildir_data *md = (struct _maildir_data *)amd;
  struct _maildir_message *msg;
  char *name = (char*)qid;
  char *p;
  int rc;
  int subdir;
  struct stat st;
  
  p = strrchr (name, '/');
  if (!p || p - qid != 3)
    return EINVAL;
  
  if (memcmp (name, subdir_name[SUB_CUR], strlen (subdir_name[SUB_CUR])) == 0)
    subdir = SUB_CUR;
  else if (memcmp (name, subdir_name[SUB_NEW], strlen (subdir_name[SUB_NEW])) == 0)
    subdir = SUB_NEW;
  else
    return EINVAL;
  
  rc = maildir_open (md);
  if (fstatat (md->folder_fd, name, &st, 0) == 0)
    {
      name = p + 1;
  
      rc = maildir_message_alloc (md, subdir, name, &msg);
      if (rc == 0)
	{
	  rc = _amd_message_insert (amd, (struct _amd_message*) msg);
	  if (rc)
	    maildir_message_free ((struct _amd_message*) msg);
	}
    }
  else
    rc = errno;

  maildir_close (md);
  
  return rc;
}

/*
 * Compare two maildir messages A and B.  The purpose is to determine
 * which one was delivered prior to another.
 *
 * Compare seconds, microseconds and number of deliveries, in that
 * order.  If all match (shouldn't happen), resort to lexicographical
 * comparison.
 *
 * FIXME: Use uid, if it is present in both message names?
 */
static int
maildir_message_cmp (struct _amd_message *a, struct _amd_message *b)
{
  char *name_a = ((struct _maildir_message *) a)->file_name;
  char *name_b = ((struct _maildir_message *) b)->file_name;
  char *pa, *pb;
  unsigned long la, lb;
  int d;
  
  la = strtoul (name_a, &name_a, 10);
  lb = strtoul (name_b, &name_b, 10);

  if (la > lb)
    return 1;
  if (la < lb)
    return -1;

  if ((d = (*name_a - *name_b)) != 0)
    return d;
  
  name_a++;
  name_b++;

  if ((pa = strchr (name_a, 'M')) != 0 && (pb = strchr (name_b, 'M')) != 0)
    {
      la = strtoul (pa + 1, &name_a, 10);
      lb = strtoul (pb + 1, &name_b, 10);

      if (la > lb)
	return 1;
      if (la < lb)
	return -1;
    }

  if ((pa = strchr (name_a, 'Q')) != 0 && (pb = strchr (name_b, 'Q')) != 0)
    {
      la = strtoul (pa + 1, &name_a, 10);
      lb = strtoul (pb + 1, &name_b, 10);

      if (la > lb)
	return 1;
      if (la < lb)
	return -1;
    }

  for (; *name_a && *name_a != ':' && *name_b && *name_b != ':';
       name_a++, name_b++)
    {
      if ((d = (*name_a - *name_b)) != 0)
	return d;
    }
  
  if ((*name_a == ':' || *name_a == 0) && (*name_b == ':' || *name_b == 0))
    return 0;

  return *name_a - *name_b;
}

static int
maildir_message_uid (mu_message_t msg, size_t *puid)
{
  struct _maildir_message *mp = mu_message_get_owner (msg);
  if (puid)
    *puid = mp->uid;
  return 0;
}

static size_t
maildir_next_uid (struct _amd_data *amd)
{
  struct _maildir_message *msg = (struct _maildir_message *)
                                   _amd_get_message (amd, amd->msg_count);
  return (msg ? msg->uid : 0) + 1;
}

static int
maildir_remove (struct _amd_data *amd)
{
  int i;
  int rc = 0;
  struct string_buffer sb = STRING_BUFFER_INITIALIZER;
  size_t off;

  /* FIXME: amd_remove_dir requires absolute file names */
  if ((rc = string_buffer_appendz (&sb, amd->name)) == 0 &&
      (rc = string_buffer_append (&sb, "/", 1)) == 0)
    {
      off = sb.off;
      for (i = 0; i < MU_ARRAY_SIZE (subdir_name); i++)
	{
	  string_buffer_appendz (&sb, subdir_name[i]);
	  string_buffer_append (&sb, "", 1);
	  
	  rc = amd_remove_dir (sb.base);
	  if (rc)
	    {
	      mu_diag_output (MU_DIAG_WARNING,
			      "removing contents of %s failed: %s", sb.base,
			      mu_strerror (rc));
	      break;
	    }
	  sb.off = off;
	}
    }
  string_buffer_free (&sb);
  return rc;
}

static int
maildir_chattr_msg (struct _amd_message *amsg, int expunge)
{
  struct _maildir_message *mp = (struct _maildir_message *) amsg;
  struct _amd_data *amd = amsg->amd;
  int rc;
  int old_subdir;
  char *cur_name, *new_name;

  rc = maildir_cur_message_name (amsg, &cur_name);
  if (rc)
    return rc;

  old_subdir = mp->subdir; 
  mp->subdir = SUB_CUR;
  rc = amd->new_msg_file_name (amsg, amsg->attr_flags, expunge, &new_name);
  if (rc == 0)
    {
      if (!new_name)
	{
	  if (unlink (mp->file_name))
	    {
	      rc = errno;
	      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			("can't unlink %s: %s",
			 mp->file_name, mu_strerror (rc)));
	    }
	}
      else
	{
	  if (rename (cur_name, new_name))
	    {
	      rc = errno;
	      if (rc == ENOENT)
		mu_observable_notify (amd->mailbox->observable,
				      MU_EVT_MAILBOX_CORRUPT,
				      amd->mailbox);
	      else
		{
		  mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			    ("renaming %s to %s failed: %s",
			     cur_name, new_name, mu_strerror (rc)));
		}
	    }
	}
      if (rc)
	mp->subdir = old_subdir;
      else
	{
	  free (mp->file_name);
	  mp->file_name = strdup (strrchr (new_name, '/') + 1);
	  if (!mp->file_name)
	    rc = errno;
	  else
	    mp->uniq_len = maildir_message_name_parse (mp->file_name,
						       NULL, NULL, NULL);
	}
      free (new_name);
    }
  
  free (cur_name);

  return rc;
}

/* Compute size of the subdirectory SUBDIR.  Add the computed value to
   *PSIZE.
   Note: Maildir must be open. */
static int
maildir_subdir_size (struct _maildir_data *md, int subdir, mu_off_t *psize)
{
  int fd;
  DIR *dir;
  struct dirent *entry;
  int rc = 0;
  struct stat st;
  mu_off_t size = 0;
  
  rc = maildir_subdir_open (md, subdir, &dir, &fd);
  if (rc)
    return rc;

  while ((entry = readdir (dir)))
    {
      switch (entry->d_name[0])
	{
	case '.':
	  break;

	default:
	  if (fstatat (fd, entry->d_name, &st, 0))
	    {
	      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
			("can't stat %s/%s/%s: %s",
			 md->amd.name, subdir_name[subdir], entry->d_name,
			 mu_strerror (errno)));
	      continue;
	    }
	  if (S_ISREG (st.st_mode))
	    size += st.st_size;
	}
    }

  closedir (dir);
  *psize += size;

  return 0;
}

static int
maildir_size_unlocked (struct _amd_data *amd, mu_off_t *psize)
{
  struct _maildir_data *md = (struct _maildir_data *) amd;
  mu_off_t size = 0;
  int rc;

  rc = maildir_open (md);
  if (rc == 0)
    {
      rc = maildir_subdir_size (md, SUB_NEW, &size);
      if (rc == 0)
	{
	  rc = maildir_subdir_size (md, SUB_CUR, &size);
	  if (rc == 0)
	    *psize = size;
	}
      maildir_close (md);
    }
  return rc;
}

static int
maildir_size (mu_mailbox_t mailbox, mu_off_t *psize)
{
  struct _amd_data *amd = mailbox->data;
  int rc;
  
  if (amd == NULL)
    return EINVAL;

  mu_monitor_wrlock (mailbox->monitor);
  rc = maildir_size_unlocked (amd, psize);
  mu_monitor_unlock (mailbox->monitor);

  return rc;
}

/* Delivery to "dir/new" */
#define NTRIES 30

static int
maildir_msg_init (struct _amd_data *amd, struct _amd_message *amm)
{
  struct _maildir_data *md = (struct _maildir_data *)amd;
  struct _maildir_message *msg = (struct _maildir_message *) amm;
  struct stat st;
  int i;
  int rc;
  char *name = NULL;
  int fd;
  
  rc = maildir_open (md);
  if (rc == 0)
    {
      rc = maildir_subdir_open (md, SUB_TMP, NULL, &fd);
      if (rc == 0)
	{
	  name = maildir_uniq_create (amd, -1);
	  rc = EAGAIN;
	  for (i = NTRIES; i > 0; i--)
	    {
	      if (fstatat (fd, name, &st, 0) == 0)
		{
		  mu_diag_output (MU_DIAG_WARNING,
				  "%s/%s/%s exists during delivery",
				  md->amd.name, subdir_name[SUB_TMP], name);
		  if (i > 1)
		    sleep (2);
		}
	      else if (errno == ENOENT)
		{
		  msg->subdir = SUB_TMP;
		  msg->uid = amd->next_uid (amd);
		  msg->file_name = name;
		  msg->uniq_len = strlen (name);
		  name = NULL;
		  rc = 0;
		  break;
		}
	      else
		{
		  mu_diag_output (MU_DIAG_WARNING, "cannot stat %s/%s/%s: %s",
				  md->amd.name, subdir_name[SUB_TMP], name,
				  mu_strerror (errno));
		  break;
		}
	    }
	  close (fd);
	}
      maildir_close (md);
    }
  free (name);
  return rc;
}

int
_mailbox_maildir_init (mu_mailbox_t mailbox)
{
  int rc;
  struct _amd_data *amd;
  struct _maildir_data *md;

  rc = amd_init_mailbox (mailbox, sizeof (struct _maildir_data), &amd);
  if (rc)
    return rc;

  amd->msg_size = sizeof (struct _maildir_message);
  amd->msg_free = maildir_message_free;
  amd->create = maildir_create;
  amd->msg_init_delivery = maildir_msg_init;
  amd->msg_finish_delivery = maildir_msg_finish_delivery;
  amd->cur_msg_file_name = maildir_cur_message_name;
  amd->new_msg_file_name = maildir_new_message_name;
  amd->scan0 = maildir_scan0;
  amd->qfetch = maildir_qfetch;
  amd->msg_cmp = maildir_message_cmp;
  amd->message_uid = maildir_message_uid;
  amd->next_uid = maildir_next_uid;
  amd->remove = maildir_remove;
  amd->chattr_msg = maildir_chattr_msg;
  amd->capabilities = MU_AMD_STATUS;
  amd->mailbox_size = maildir_size;
  
  /* Set our properties.  */
  {
    mu_property_t property = NULL;
    mu_mailbox_get_property (mailbox, &property);
    mu_property_set_value (property, "TYPE", "MAILDIR", 1);
  }

  md = (struct _maildir_data *) amd;
  md->folder_fd = -1;
  
  return 0;
}

