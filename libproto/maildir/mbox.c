/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999, 2000, 2001, 2002, 2003,
   2004, 2005 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301 USA */

/* First draft by Sergey Poznyakoff */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef ENABLE_MAILDIR

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
#include <mailutils/mutil.h>
#include <mailutils/property.h>
#include <mailutils/stream.h>
#include <mailutils/url.h>
#include <mailutils/observer.h>
#include <mailutils/errno.h>
#include <mailutils/locker.h>
#include <mailbox0.h>
#include <registrar0.h>
#include <amd.h>
#include <maildir.h>

#ifndef PATH_MAX 
# define PATH_MAX _POSIX_PATH_MAX
#endif

struct _maildir_message
{
  struct _amd_message amd_message;
  int newflag;
  char *file_name;
  unsigned long uid;
};


/* Attribute handling.
   FIXME: P (Passed), D (Draft) and F (Flagged) are not handled */

static struct info_map {
  char letter;
  int flag;
} info_map[] = {
  { 'R', MU_ATTRIBUTE_READ },
  { 'S', MU_ATTRIBUTE_SEEN },
  { 'T', MU_ATTRIBUTE_DELETED },
  { 0 },
};
#define info_map_size (sizeof (info_map) / sizeof (info_map[0]))

/* FIXME: is not used */
static int
info_map_letter (int c)
{
  struct info_map *p;

  for (p = info_map; p < info_map + info_map_size; p++)
    if (p->letter == c)
      return p->flag;
  return 0;
}

/* NOTE: BUF must be at least 7 bytes long */
static int
flags_to_info (int flags, char *buf)
{
  struct info_map *p;
  
  for (p = info_map; p < info_map + info_map_size; p++)
    if (p->flag & flags)
      *buf++ = p->letter;
  *buf = 0;
  return 0;
}

static int
info_to_flags (char *buf)
{
  int flags = 0;
  struct info_map *p;

  for (p = info_map; p < info_map + info_map_size; p++)
    if (strchr (buf, p->letter))
      flags |= p->flag;
  return 0;
}


static int
maildir_message_cmp (struct _amd_message *a, struct _amd_message *b)
{
  char *name_a, *name_b;
  unsigned long na = strtoul (((struct _maildir_message *) a)->file_name,
			      &name_a, 10);
  unsigned long nb = strtoul (((struct _maildir_message *) b)->file_name,
			      &name_b, 10);
  int rc;
  
  if (na > nb)
    return 1;
  if (na < nb)
    return -1;

  /* If timestamps are equal, compare rests of the filenames up to the
     info marker */

  if (name_a && !name_b)
    return 1;
  else if (!name_a)
    {
      if (name_b)
	return -1;
      else 
	return 0;
    }
  
  for (; *name_a && *name_a != ':' && *name_b && *name_b != ':';
       name_a++, name_b++)
    if ((rc = (*name_a - *name_b)))
      return rc;

  if (*name_a == ':' && *name_b == ':')
    {
      name_a++;
      name_b++;
    }

  return *name_a - *name_b;
}

void
msg_free (struct _amd_message *amsg)
{
  struct _maildir_message *mp = (struct _maildir_message *) amsg;
  free (mp->file_name);
}

char *
maildir_gethostname ()
{
  char hostname[256];
  char *hp;
  char *p;
  size_t s;
  
  if (gethostname (hostname, sizeof hostname) < 0)
    strcpy (hostname, "localhost");

  for (s = 0, p = hostname; *p; p++)
    if (*p == '/' || *p == ':')
      s += 4;

  if (s)
    {
      char *q;
      
      hp = malloc (strlen (hostname) + s + 1);
      for (p = hostname, q = hp; *p; p++)
	switch (*p)
	  {
	  case '/':
	    memcpy (q, "\\057", 4);
	    q += 4;
	    break;

	  case ':':
	    memcpy (q, "\\072", 4);
	    q += 4;
	    break;

	  default:
	    *q++ = *p++;
	  }
      *q = 0;
    }
  else
    hp = strdup (hostname);
  return hp;
}

int
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

char *
maildir_mkfilename (const char *directory, const char *suffix, const char *name)
{
  size_t size = strlen (directory) + 1 + strlen (suffix) + 1;
  char *tmp;

  if (name)
    size += 1 + strlen (name);
  
  tmp = malloc (size);
  sprintf (tmp, "%s/%s", directory, suffix);
  if (name)
    {
      strcat (tmp, "/");
      strcat (tmp, name);
    }
  return tmp;
}

static char *
mk_info_filename (char *directory, char *suffix, char *name, int flags)
{
  char fbuf[9];
  char *tmp;
  int namelen;
  size_t size;

  tmp = strchr (name, ':');
  if (!tmp)
    namelen = strlen (name);
  else
    namelen = tmp - name;

  size = strlen (directory)
          + 1 + strlen (suffix)
          + 1 + namelen + 1;

  flags_to_info (flags, fbuf);
  size += 3 + strlen (fbuf);

  tmp = malloc (size);
  if (fbuf[0])
    sprintf (tmp, "%s/%s/%*.*s:2", directory, suffix, namelen, namelen, name);
  else
    sprintf (tmp, "%s/%s/%*.*s:2,%s", directory, suffix, namelen, namelen, name, fbuf);
  return tmp;
}

char *
maildir_uniq (struct _amd_data *amd, int fd)
{
  char buffer[PATH_MAX];
  int ind = 0;
#define FMT(fmt,val) do {\
  ind += snprintf(buffer+ind, sizeof buffer-ind, fmt, val); \
} while (0) 
#define PFX(pfx,fmt,val) do {\
  if (ind < sizeof buffer-1) {\
    buffer[ind++] = pfx;\
    FMT(fmt,val);\
  }\
} while (0) 
#define COPY(s) do {\
  char *p; \
  for (p = s; ind < sizeof buffer-1 && *p;) \
    buffer[ind++] = *p++;\
} while (0);
  char *hostname = maildir_gethostname ();
  struct timeval tv;
  unsigned long n;
  struct stat st;
  
  gettimeofday (&tv, NULL);
  FMT ("%lu", tv.tv_sec);
  COPY (".");

  if (read_random (&n, sizeof (unsigned long))) /* FIXME: 32 bits */
    PFX ('R', "%lX", n);

  if (fd > 0 && fstat (fd, &st) == 0)
    {
      PFX ('I', "%lX", (unsigned long) st.st_ino);
      PFX ('V', "%lX", (unsigned long) st.st_dev);
    }

  PFX ('M', "%lu", tv.tv_usec);
  PFX ('P', "%lu", (unsigned long) getpid ());
  PFX ('Q', "%lu", (unsigned long) amd->msg_count);
  PFX ('.', "%s", hostname);
  free (hostname);

  buffer[ind] = 0;
  
  return strdup (buffer);
}

char *
maildir_message_name (struct _amd_message *amsg, int deleted)
{
  struct _maildir_message *msg = (struct _maildir_message *) amsg;
  if (deleted)
    return NULL; /* Force amd.c to unlink the file.
		    FIXME: We could also add a 'T' info to it. Should
		    we have an option deciding which approach to take? */
  return maildir_mkfilename (amsg->amd->name,
			     msg->newflag ? NEWSUF : CURSUF, msg->file_name);
}

static void
maildir_msg_free (struct _amd_message *amsg)
{
  struct _maildir_message *mp = (struct _maildir_message *) amsg;
  free (mp->file_name);
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

/* According to http://www.qmail.org/qmail-manual-html/man5/maildir.html 
   a file in tmp may be safely removed if it has not been accessed in 36
   hours */

static void
maildir_delete_file (char *dirname, char *filename)
{
  struct stat st;
  char *name = maildir_mkfilename (dirname, filename, NULL);

  if (stat (name, &st) == 0)
    {
      if (time (NULL) - st.st_atime > 36 * 3600)	
	remove (name);
    }
  free (name);
}



int
maildir_opendir (DIR **dir, char *name, int permissions)
{
  *dir = opendir (name);
  if (!*dir)
    {
      if (errno == ENOENT)
	{
	  if (mkdir (name, permissions))
	    return errno;
	  *dir = opendir (name);
	  if (!*dir)
	    return errno;
	}
      else
	return errno;
    }
  return 0;
}


#define NTRIES 30

/* Delivery to "dir/new" */

static int
maildir_msg_init (struct _amd_data *amd, struct _amd_message *amm)
{
  struct _maildir_message *msg = (struct _maildir_message *) amm;
  char *name;
  struct stat st;
  int i;
  
  /* chdir (amd->name);  FIXME */
  name = maildir_uniq (amd, -1);

  for (i = 0; i < NTRIES; i++)
    {
      if (stat (name, &st) < 0 && errno == ENOENT)
	{
	  msg->uid = amd->next_uid (amd);
	  msg->file_name = name;
	  return 0;
	}
      sleep (2);
    }

  free (name);
  return MU_ERR_BAD_RESUMPTION;
}

static int
maildir_msg_finish_delivery (struct _amd_data *amd, struct _amd_message *amm)
{
  struct _maildir_message *msg = (struct _maildir_message *) amm;
  char *oldname = maildir_mkfilename (amd->name, TMPSUF, msg->file_name);
  char *newname = maildir_mkfilename (amd->name, NEWSUF, msg->file_name);

  unlink (newname);
  if (link (oldname, newname))
    {
      unlink (oldname);
      msg->newflag = 1;
    }
  free (oldname);
  free (newname);
  return 0;
}


/* Maldir scanning */

int
maildir_flush (struct _amd_data *amd)
{
  int rc;
  DIR *dir;
  struct dirent *entry;
  char *tmpname = maildir_mkfilename (amd->name, TMPSUF, NULL);

  rc = maildir_opendir (&dir, tmpname, PERMS);
  if (rc)
    {
      free (tmpname);
      return rc;
    }
      
  while ((entry = readdir (dir)))
    {
      switch (entry->d_name[0])
	{
	case '.':
	  break;

	default:
	  maildir_delete_file (tmpname, entry->d_name);
	  break;
	}
    }

  free (tmpname);

  closedir (dir);
  return 0;
}

int
maildir_deliver_new (struct _amd_data *amd, DIR *dir)
{
  struct dirent *entry;

  while ((entry = readdir (dir)))
    {
      char *oldname, *newname;
	
      switch (entry->d_name[0])
	{
	case '.':
	  break;

	default:
	  oldname = maildir_mkfilename (amd->name, NEWSUF, entry->d_name);
	  newname = mk_info_filename (amd->name, CURSUF, entry->d_name, 0);
	  rename (oldname, newname);
	  free (oldname);
	  free (newname);
	}
    }
  return 0;
}

static struct _maildir_message *
maildir_message_lookup (struct _amd_data *amd, char *file_name)
{
  struct _maildir_message *msg;
  char *p = strchr (file_name, ':');
  size_t length = p ? p - file_name : strlen (file_name);
  size_t i;
  
  /*FIXME: Replace linear search with binary one! */
  for (i = 0; i < amd->msg_count; i++)
    {
      msg = (struct _maildir_message *) amd->msg_array[i];

      if (strlen (msg->file_name) <= length
	  && memcmp (msg->file_name, file_name, length) == 0)
	return msg;
    }
  return NULL;
}

static int
maildir_scan_dir (struct _amd_data *amd, DIR *dir)
{
  struct _maildir_message *msg;
  struct dirent *entry;

  while ((entry = readdir (dir)))
    {
      char *p;
      int insert;
      
      switch (entry->d_name[0])
	{
	case '.':
	  break;

	default:
	  msg = maildir_message_lookup (amd, entry->d_name);
	  if (msg)
	    {
	      free (msg->file_name);
	      msg->newflag = 0;
	      insert = 0;
	    }
	  else
	    {
	      msg = calloc (1, sizeof(*msg));
	      insert = 1;
	    }
	  
	  msg->file_name = strdup (entry->d_name);

	  p = strchr (msg->file_name, ':');
	  if (p && strcmp (p+1, "2,") == 0)
	    msg->amd_message.attr_flags = info_to_flags (p+3);
	  else
	    msg->amd_message.attr_flags = 0;
	  msg->amd_message.deleted = msg->amd_message.attr_flags & MU_ATTRIBUTE_DELETED;
	  if (insert)
	    {
	      msg->uid = amd->next_uid (amd);
	      _amd_message_insert (amd, (struct _amd_message*) msg);
	    }
	}
    }
  return 0;
}

static int
maildir_scan0 (mu_mailbox_t mailbox, size_t msgno ARG_UNUSED, size_t *pcount, 
	       int do_notify)
{
  struct _amd_data *amd = mailbox->data;
  DIR *dir;
  int status = 0;
  char *name;
  struct stat st;
  
  if (amd == NULL)
    return EINVAL;

  mu_monitor_wrlock (mailbox->monitor);

  /* 1st phase: Flush tmp/ */
  maildir_flush (amd);

  /* 2nd phase: Scan and deliver messages from new */
  name = maildir_mkfilename (amd->name, NEWSUF, NULL);

  status = maildir_opendir (&dir, name, PERMS);
  if (status == 0)
    {
      maildir_deliver_new (amd, dir);
      closedir (dir);
    }
  free (name);

  name = maildir_mkfilename (amd->name, CURSUF, NULL);
  /* 3rd phase: Scan cur/ */
  status = maildir_opendir (&dir, name, PERMS);
  if (status == 0)
    {
      status = maildir_scan_dir (amd, dir);
      closedir (dir);
    }
  free (name);

  if (do_notify)
    {
      size_t i;
      for (i = 0; i < amd->msg_count; i++)
	{
	  DISPATCH_ADD_MSG(mailbox, amd);
	}
    }
  
  if (stat (amd->name, &st) == 0)
    amd->mtime = st.st_mtime;

  if (pcount)
    *pcount = amd->msg_count;

  /* Reset the uidvalidity.  */
  if (amd->msg_count > 0)
    {
      if (amd->uidvalidity == 0)
	{
	  amd->uidvalidity = (unsigned long) time (NULL);
	  /* FIXME amd->uidnext = amd->msg_count + 1;*/
	  /* Tell that we have been modified for expunging.  */
	  if (amd->msg_count)
	    {
	      amd_message_stream_open (amd->msg_array[0]);
	      amd_message_stream_close (amd->msg_array[0]);
	      amd->msg_array[0]->attr_flags |= MU_ATTRIBUTE_MODIFIED;
	    }
	}
    }

  /* Clean up the things */
  amd_cleanup (mailbox);
  return status;
}


int
_mailbox_maildir_init (mu_mailbox_t mailbox)
{
  int rc;
  struct _amd_data *amd;

  rc = amd_init_mailbox (mailbox, sizeof (struct _amd_data), &amd);
  if (rc)
    return rc;

  amd->msg_size = sizeof (struct _maildir_message);
  amd->msg_free = maildir_msg_free;
  amd->msg_init_delivery = maildir_msg_init;
  amd->msg_finish_delivery = maildir_msg_finish_delivery;
  amd->msg_file_name = maildir_message_name;
  amd->scan0 = maildir_scan0;
  amd->msg_cmp = maildir_message_cmp;
  amd->message_uid = maildir_message_uid;
  amd->next_uid = maildir_next_uid;
  
  /* Set our properties.  */
  {
    mu_property_t property = NULL;
    mu_mailbox_get_property (mailbox, &property);
    mu_property_set_value (property, "TYPE", "MAILDIR", 1);
  }

  return 0;
}
#endif