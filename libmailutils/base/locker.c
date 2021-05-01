/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2021 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <mailutils/errno.h>
#include <mailutils/locker.h>
#include <mailutils/util.h>
#include <mailutils/io.h>

/* First draft by Brian Edmond. */
/* For subsequent modifications, see the GNU mailutils ChangeLog. */

struct _mu_locker
{
  unsigned refcnt;             /* Number of times mu_locker_lock was called */
  enum mu_locker_mode mode;    /* Current locking mode (if refcnt > 0) */

  int type;
  char *file;
  int flags;
  int expire_time;
  int retry_count;
  int retry_sleep;

  union lock_data
  {
    struct
    {
      char *dotlock;
      char *nfslock;
    } dot;             /* MU_LOCKER_TYPE_DOTLOCK */
    
    struct
    {
      char *name;
    } external;        /* MU_LOCKER_TYPE_EXTERNAL */   
    
    int fd;            /* MU_LOCKER_TYPE_KERNEL */
  } data;
};

static int
stat_check (const char *file, int fd, int links)
{
  struct stat fn_stat;
  struct stat fd_stat;
  int err = 0;
  int localfd = -1;

  if (fd == -1)
    {
      localfd = open (file, O_RDONLY);
      
      if (localfd == -1)
	return errno;
      fd = localfd;
    }

  /* We should always be able to stat a valid fd, so this
     is an error condition. */
  if (lstat (file, &fn_stat) || fstat (fd, &fd_stat))
    err = errno;
  else
    {
      /* If the link and stat don't report the same info, or the
         file is a symlink, fail the locking. */
      if (!S_ISREG (fn_stat.st_mode)
	  || !S_ISREG (fd_stat.st_mode)
	  || fn_stat.st_nlink != links
	  || fn_stat.st_dev != fd_stat.st_dev
	  || fn_stat.st_ino != fd_stat.st_ino
	  || fn_stat.st_mode != fd_stat.st_mode
	  || fn_stat.st_nlink != fd_stat.st_nlink
	  || fn_stat.st_uid != fd_stat.st_uid
	  || fn_stat.st_gid != fd_stat.st_gid
	  || fn_stat.st_rdev != fd_stat.st_rdev)
	err = EINVAL;
    }
  if (localfd != -1)
    close (localfd);

  return err;
}

static int
check_file_permissions (const char *file)
{
  int fd = -1;
  int err = 0;

  if ((fd = open (file, O_RDONLY)) == -1)
    return errno == ENOENT ? 0 : errno;

  err = stat_check (file, fd, 1);
  close (fd);
  fd = -1;
  if (err)
    {
      if (err == EINVAL)
	err = MU_ERR_LOCK_BAD_FILE;
      return err;
    }

  return 0;
}

static int
prelock_common (mu_locker_t locker)
{
  /* Check if we are trying to lock a regular file, with a link count
     of 1, that we have permission to read, etc., or don't lock it. */
  return check_file_permissions (locker->file);
}

/* Dotlock type */
#define DOTLOCK_SUFFIX ".lock"

/* expire a stale lock (if MU_LOCKER_FLAG_CHECK_PID or
   MU_LOCKER_FLAG_EXPIRE_TIME) */
static void
expire_stale_lock (mu_locker_t lock)
{
  int stale = 0;
  int fd = open (lock->data.dot.dotlock, O_RDONLY);
  
  if (fd == -1)
    return;

  /* Check to see if this process is still running.  */
  if (lock->flags & MU_LOCKER_FLAG_CHECK_PID)
    {
      char buf[16];
      pid_t pid;
      int nread = read (fd, buf, sizeof (buf) - 1);
      if (nread > 0)
	{
	  buf[nread] = '\0';
	  pid = strtol (buf, NULL, 10);
	  if (pid > 0)
	    {
	      /* Process is gone so we try to remove the lock. */
	      if (kill (pid, 0) == -1)
		stale = 1;
	    }
	  else
	    stale = 1;		/* Corrupted file, remove the lock. */
	}
    }
  
  /* Check to see if the lock expired.  */
  if (lock->flags & MU_LOCKER_FLAG_EXPIRE_TIME)
    {
      struct stat stbuf;

      fstat (fd, &stbuf);
      /* The lock has expired. */
      if ((time (NULL) - stbuf.st_mtime) > lock->expire_time)
	stale = 1;
    }

  close (fd);
  if (stale)
    unlink (lock->data.dot.dotlock);
}

static int
init_dotlock (mu_locker_t lck, mu_locker_hints_t *hints)
{
  char *tmp, *p;

  /* Make sure the spool directory is writable */
  tmp = strdup (lck->file);
  if (!tmp)
    return ENOMEM;

  strcpy (tmp, lck->file);
  p = strrchr (tmp, '/');
  if (!p)
    {
      free (tmp);
      tmp = strdup (".");
      if (!tmp)
	return ENOMEM;
    }
  else
    *p = 0; 

  if (access (tmp, W_OK))
    {
      /* Fallback to kernel locking */
      mu_locker_hints_t hints = {
	.flags = MU_LOCKER_FLAG_TYPE,
	.type  = MU_LOCKER_TYPE_KERNEL
      };
      free (tmp);
      return mu_locker_modify (lck, &hints);
    }
  
  free (tmp);

  lck->data.dot.dotlock = malloc (strlen (lck->file)
				  + sizeof (DOTLOCK_SUFFIX));
  
  if (!lck->data.dot.dotlock)
    return ENOMEM;
  strcpy (lck->data.dot.dotlock, lck->file);
  strcat (lck->data.dot.dotlock, DOTLOCK_SUFFIX);

  return 0;
}

static void
destroy_dotlock (mu_locker_t lck)
{
  free (lck->data.dot.dotlock);
  free (lck->data.dot.nfslock);
}

static int
lock_dotlock (mu_locker_t lck, enum mu_locker_mode mode)
{
  int rc;
  char *host = NULL;
  time_t now;
  int err = 0;
  int fd;
    
  if (lck->data.dot.nfslock)
    {
      unlink (lck->data.dot.nfslock);
      free (lck->data.dot.nfslock);
      lck->data.dot.nfslock = NULL;
    }

  expire_stale_lock (lck);

  /* build the NFS hitching-post to the lock file */

  rc = mu_get_host_name (&host);
  if (rc)
    return rc;
  time (&now);
  rc = mu_asprintf (&lck->data.dot.nfslock,
		    "%s.%lu.%lu.%s",
		    lck->file,
		    (unsigned long) getpid (),
		    (unsigned long) now, host);
  free (host);
  if (rc)
    return rc;
  
  fd = open (lck->data.dot.nfslock,
	     O_WRONLY | O_CREAT | O_EXCL, MU_LOCKFILE_MODE);
  if (fd == -1)
    {
      if (errno == EEXIST)
	return EAGAIN;
      else
	return errno;
    }
  close (fd);
  
  /* Try to link to the lockfile. */
  if (link (lck->data.dot.nfslock, lck->data.dot.dotlock) == -1)
    {
      unlink (lck->data.dot.nfslock);
      if (errno == EEXIST)
	return EAGAIN;
      return errno;
    }

  if ((fd = open (lck->data.dot.dotlock, O_RDWR)) == -1)
    {
      unlink (lck->data.dot.nfslock);
      return errno;
    }
  
  err = stat_check (lck->data.dot.nfslock, fd, 2);
  if (err)
    {
      unlink (lck->data.dot.nfslock);
      if (err == EINVAL)
	return MU_ERR_LOCK_BAD_LOCK;
      return errno;
    }

  unlink (lck->data.dot.nfslock);

  if (lck->flags & MU_LOCKER_FLAG_CHECK_PID)
    {
      char buf[16];
      sprintf (buf, "%ld", (long) getpid ());
      write (fd, buf, strlen (buf));
    }
  close (fd);
  return 0;
}

static int
unlock_dotlock (mu_locker_t lck)
{
  if (unlink (lck->data.dot.dotlock) == -1)
    {
      int err = errno;
      if (err == ENOENT)
	{
	  lck->refcnt = 0; /*FIXME?*/
	  err = MU_ERR_LOCK_NOT_HELD;
	  return err;
	}
      return err;
    }
  return 0;
}

/* Kernel locking */
static int
lock_kernel (mu_locker_t lck, enum mu_locker_mode mode)
{
  int fd;
  struct flock fl;

  switch (mode)
    {
    case mu_lck_shr:
    case mu_lck_opt:
      mode = O_RDONLY;
      fl.l_type = F_RDLCK;
      break;

    case mu_lck_exc:
      mode = O_RDWR;
      fl.l_type = F_WRLCK;
      break;

    default:
      return EINVAL;
    }
  
  fd = open (lck->file, O_RDWR);
  if (fd == -1)
    return errno;
  lck->data.fd = fd;
  
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0; /* Lock entire file */
  if (fcntl (fd, F_SETLK, &fl))
    {
#ifdef EACCES      
      if (errno == EACCES)
	return EAGAIN;
#endif
      if (errno == EAGAIN)
	return EAGAIN;
      return errno;
    }
  return 0;
}

static int
unlock_kernel (mu_locker_t lck)
{
  struct flock fl;

  fl.l_type = F_UNLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0; /* Unlock entire file */
  if (fcntl (lck->data.fd, F_SETLK, &fl))
    {
#ifdef EACCESS
      if (errno == EACCESS)
	return EAGAIN;
#endif
      if (errno == EAGAIN)
	return EAGAIN;
      return errno;
    }
  close (lck->data.fd);
  lck->data.fd = -1;
  return 0;
}

/* External locking */
static int
init_external (mu_locker_t lck, mu_locker_hints_t *hints)
{
  char const *ext_locker = hints->flags & MU_LOCKER_FLAG_EXT_LOCKER
                              ? hints->ext_locker
                              : MU_LOCKER_DEFAULT_EXT_LOCKER;
  if (!(lck->data.external.name = strdup (ext_locker)))
    return ENOMEM;
  return 0;
}

static void
destroy_external (mu_locker_t lck)
{
  free (lck->data.external.name);
}

/*
  Estimate 1 decimal digit per 3 bits, + 1 for round off.
*/
#define DEC_DIGS_PER_INT (sizeof(int) * 8 / 3 + 1)

static int
external_locker (mu_locker_t lck, int lock)
{
  int err = 0;
  char *av[6];
  int ac = 0;
  char aforce[3 + DEC_DIGS_PER_INT + 1];
  char aretry[3 + DEC_DIGS_PER_INT + 1];
  int status;

  av[ac++] = lck->data.external.name;

  if (lck->flags & MU_LOCKER_FLAG_EXPIRE_TIME)
    {
      snprintf (aforce, sizeof (aforce), "-f%d", lck->expire_time);
      aforce[sizeof (aforce) - 1] = 0;
      av[ac++] = aforce;
    }
  
  if (lck->flags & MU_LOCKER_FLAG_RETRY)
    {
      snprintf (aretry, sizeof (aretry), "-r%d", lck->retry_count);
      aretry[sizeof (aretry) - 1] = 0;
      av[ac++] = aretry;
    }

  if (!lock)
    av[ac++] = "-u";

  av[ac++] = lck->file;

  av[ac++] = NULL;

  if ((err = mu_spawnvp (av[0], av, &status)))
    {
      perror ("mu_spawnvp");
      fprintf (stderr, "errcode %d\n", err);
      return err;
    }
  
  if (!WIFEXITED (status))
    {
      err = MU_ERR_LOCK_EXT_KILLED;
    }
  else
    {
      switch (WEXITSTATUS (status))
	{
	case 127:
	  err = MU_ERR_LOCK_EXT_FAIL;
	  break;
	  
	case MU_DL_EX_OK:
	  err = 0;
	  lck->refcnt = lock;
	  break;
	  
	case MU_DL_EX_NEXIST:
	  err = MU_ERR_LOCK_NOT_HELD;
	  break;
	  
	case MU_DL_EX_EXIST:
	  err = MU_ERR_LOCK_CONFLICT;
	  break;
	  
	case MU_DL_EX_PERM:
	  err = EPERM;
	  break;
	  
	default:
	case MU_DL_EX_ERROR:
	  err = MU_ERR_LOCK_EXT_ERR;
	  break;
	}
    }

  return err;
}

static int
lock_external (mu_locker_t lck, enum mu_locker_mode mode)
{
  return external_locker (lck, 1);
}

static int
unlock_external (mu_locker_t lck)
{
  return external_locker (lck, 0);
}

mu_locker_hints_t mu_locker_defaults = {
  .flags = MU_LOCKER_FLAG_TYPE | MU_LOCKER_FLAG_RETRY,
  .type  = MU_LOCKER_TYPE_DEFAULT,
  .retry_count = MU_LOCKER_DEFAULT_RETRY_COUNT,
  .retry_sleep = MU_LOCKER_DEFAULT_RETRY_SLEEP
};

struct locker_tab
{
  int (*init) (mu_locker_t, mu_locker_hints_t *);
  void (*destroy) (mu_locker_t);
  int (*prelock) (mu_locker_t); 
  int (*lock) (mu_locker_t, enum mu_locker_mode);
  int (*unlock) (mu_locker_t);
};

static struct locker_tab locker_tab[] = {
  /* MU_LOCKER_TYPE_DOTLOCK */
  { init_dotlock, destroy_dotlock, prelock_common,
    lock_dotlock, unlock_dotlock },
  /* MU_LOCKER_TYPE_EXTERNAL */
  { init_external, destroy_external, prelock_common,
    lock_external, unlock_external },
  /* MU_LOCKER_TYPE_KERNEL */
  { NULL, NULL, NULL, lock_kernel, unlock_kernel },
  /* MU_LOCKER_TYPE_NULL */
  { NULL, NULL, NULL, NULL, NULL }
};

#define MU_LOCKER_NTYPES (sizeof (locker_tab) / sizeof (locker_tab[0]))

int
mu_locker_create_ext (mu_locker_t *plocker, const char *fname,
		  mu_locker_hints_t *user_hints)
{
  mu_locker_t lck;
  char *filename;
  int err = 0;
  mu_locker_hints_t hints;
  
  if (plocker == NULL)
    return MU_ERR_OUT_PTR_NULL;

  if (fname == NULL)
    return EINVAL;

  if ((err = mu_unroll_symlink (fname, &filename)))
    {
      if (err == ENOENT)
	{
	  /* Try the directory part.  If it unrolls successfully (i.e.
	     all its components exist), tuck the filename part back in
	     the resulting path and use it as the lock filename. */
	  char *p, *new_name, *tmp = strdup (fname);
	  if (!tmp)
	    return ENOMEM;
	  p = strrchr (tmp, '/');
	  if (!p)
	    filename = tmp;
	  else
	    {
	      *p = 0;
	      err = mu_unroll_symlink (tmp, &filename);
	      if (err)
		{
		  free (tmp);
		  return err;
		}

	      new_name = mu_make_file_name_suf (filename, p + 1, NULL);
	      free (tmp);
	      free (filename);
	      if (!new_name)
		return ENOMEM;
	      filename = new_name;
	    }
	}
      else
	return err;
    }

  lck = calloc (1, sizeof (*lck));

  if (lck == NULL)
    {
      free (filename);
      return ENOMEM;
    }
  
  lck->file = filename;

  hints = user_hints ? *user_hints : mu_locker_defaults;
  if ((hints.flags & MU_LOCKER_FLAG_TYPE) == 0)
    {
      hints.flags |= MU_LOCKER_FLAG_TYPE;
      hints.type = MU_LOCKER_TYPE_DEFAULT;
    }
  err = mu_locker_modify (lck, &hints);
  if (err)
    mu_locker_destroy (&lck);
  else
    *plocker = lck;

  return err;
}

int
mu_locker_modify (mu_locker_t lck, mu_locker_hints_t *hints)
{
  if (!lck || !hints)
    return EINVAL;
  
  if (hints->flags & MU_LOCKER_FLAG_TYPE)
    {
      struct _mu_locker new_lck;
      int type;
      
      if (hints->type < 0 || hints->type >= MU_LOCKER_NTYPES)
	return EINVAL;

      if (lck->flags == 0 || hints->type != lck->type)
	{
	  if (strcmp (lck->file, "/dev/null") == 0)
	    type = MU_LOCKER_TYPE_NULL;
	  else
	    type = hints->type;
	  
	  memset (&new_lck, 0, sizeof (new_lck));
	  new_lck.type = type;
	  new_lck.file = lck->file;
	  if (locker_tab[type].init)
	    {
	      int rc = locker_tab[type].init (&new_lck, hints);
	      if (rc)
		{
		  if (locker_tab[type].destroy)
		    locker_tab[type].destroy (&new_lck);
		  return rc;
		}
	    }
	  
	  if (lck->flags != 0 && locker_tab[lck->type].destroy)
	    locker_tab[lck->type].destroy (lck);
	  
	  *lck = new_lck;
	}
    }

  if (hints->flags & MU_LOCKER_FLAG_RETRY)
    {
      lck->retry_count = hints->retry_count > 0
	                     ? hints->retry_count
	                     : MU_LOCKER_DEFAULT_RETRY_COUNT;
      lck->retry_sleep = hints->retry_sleep > 0
	                     ? hints->retry_sleep
	                     : MU_LOCKER_DEFAULT_RETRY_SLEEP;
    }

  if (hints->flags & MU_LOCKER_FLAG_EXPIRE_TIME)
    lck->expire_time = hints->expire_time > 0 ? hints->expire_time
                                              : MU_LOCKER_DEFAULT_EXPIRE_TIME;

  lck->flags = hints->flags;

  return 0;
}

void
mu_locker_destroy (mu_locker_t *plocker)
{
  if (plocker && *plocker)
    {
      mu_locker_t lck = *plocker;
      if (locker_tab[lck->type].destroy)
	locker_tab[lck->type].destroy (lck);
      free (lck->file);
      free (lck);
      *plocker = NULL;
    }
}

int
mu_locker_lock_mode (mu_locker_t lck, enum mu_locker_mode mode)
{
  int rc;
  unsigned retries = 1;
  
  if (!lck || lck->type < 0 || lck->type >= MU_LOCKER_NTYPES)
    return EINVAL;

  if (locker_tab[lck->type].prelock && (rc = locker_tab[lck->type].prelock (lck)))
    return rc;
  
  /* Is the lock already applied? */
  if (lck->refcnt > 0)
    {
      lck->refcnt++;
      if (mode == lck->mode)
	return 0;
    }

  lck->mode = mode;

  if (lck->flags & MU_LOCKER_FLAG_RETRY)
    retries = lck->retry_count;

  if (locker_tab[lck->type].lock)
    {
      while (retries--)
	{
	  rc = locker_tab[lck->type].lock (lck, mode);
	  if (rc == EAGAIN && retries)
	    sleep (lck->retry_sleep);
	  else
	    break;
	}

      if (rc == EAGAIN)
	rc = MU_ERR_LOCK_CONFLICT;
    }
  else
    rc = 0;

  if (rc == 0)
    lck->refcnt++;
  
  return rc;
}

int
mu_locker_lock (mu_locker_t lck)
{
  return mu_locker_lock_mode (lck, mu_lck_exc);
}

int
mu_locker_unlock (mu_locker_t lck)
{
  int rc = 0;
  
  if (!lck)
    return MU_ERR_LOCKER_NULL;

  if (lck->refcnt == 0)
    return MU_ERR_LOCK_NOT_HELD;

  if ((rc = check_file_permissions (lck->file)))
    return rc;

  if (--lck->refcnt > 0)
    return 0;

  if (locker_tab[lck->type].unlock)
    rc = locker_tab[lck->type].unlock (lck);
  else
    rc = 0;
  
  return rc;
}

int
mu_locker_remove_lock (mu_locker_t lck)
{
  if (!lck)
    return MU_ERR_LOCKER_NULL;

  /* Force the reference count to 1 to unlock the file. */
  lck->refcnt = 1;
  return mu_locker_unlock (lck);
}

int
mu_locker_touchlock (mu_locker_t lck)
{
  if (!lck)
    return MU_ERR_LOCKER_NULL;

  if (lck->type != MU_LOCKER_TYPE_DOTLOCK)
    return 0;
  
  if (lck->refcnt > 0)
    return utime (lck->data.dot.dotlock, NULL);

  return MU_ERR_LOCK_NOT_HELD;
}

int
mu_locker_get_hints (mu_locker_t lck, mu_locker_hints_t *hints)
{
  if (!lck || !hints)
    return EINVAL;

  if (hints->flags & MU_LOCKER_FLAG_TYPE)
    hints->type = lck->type;

  hints->flags &= ~(lck->flags & hints->flags);
    
  if (hints->flags & MU_LOCKER_FLAG_RETRY)
    {
      hints->retry_count = lck->retry_count;
      hints->retry_sleep = lck->retry_sleep;
    }
  if (hints->flags & MU_LOCKER_FLAG_EXPIRE_TIME)
    hints->expire_time = lck->expire_time;
  if (hints->flags & MU_LOCKER_FLAG_EXT_LOCKER)
    {
      if (lck->type == MU_LOCKER_TYPE_EXTERNAL)
	{
	  if ((hints->ext_locker = strdup (lck->data.external.name)) == NULL)
	    return errno;
	}
      else
	hints->ext_locker = NULL;
    }

  return 0;
}

/* Deprecated interfaces */

int
mu_locker_create (mu_locker_t *lck, const char *filename, int flags)
{
  mu_locker_hints_t hints, *hp;

  if (flags == 0)
    hp = NULL;
  else
    {
      hints.type  = MU_LOCKER_FLAG_TO_TYPE(flags);
      hints.flags = flags & MU_LOCKER_OPTION_MASK;
      hp = &hints;
    }
  return mu_locker_create_ext (lck, filename, hp);
}

int
mu_locker_set_default_flags (int flags, enum mu_locker_set_mode mode)
{
  int type = MU_LOCKER_FLAG_TO_TYPE (flags);
  flags &= MU_LOCKER_OPTION_MASK;
  switch (mode)
    {
    case mu_locker_assign:
      mu_locker_defaults.flags = flags;
      mu_locker_defaults.type = type;
      break;
      
    case mu_locker_set_bit:
      mu_locker_defaults.flags |= flags;
      mu_locker_defaults.type = type;
      break;
      
    case mu_locker_clear_bit:
      mu_locker_defaults.flags &= flags;
      if (type != MU_LOCKER_TYPE_DOTLOCK)
	mu_locker_defaults.type = MU_LOCKER_TYPE_DOTLOCK;	
      break;
    }
  mu_locker_defaults.flags |= MU_LOCKER_FLAG_TYPE;
  return 0;
}

void
mu_locker_set_default_retry_timeout (time_t to)
{
  mu_locker_defaults.retry_sleep = to;
}

void
mu_locker_set_default_retry_count (size_t n)
{
  mu_locker_defaults.retry_count = n;
}

void
mu_locker_set_default_expire_timeout (time_t t)
{
  mu_locker_defaults.expire_time = t;
}

int
mu_locker_set_default_external_program (char const *path)
{
  char *p = strdup (path);
  if (!p)
    return errno;
  free (mu_locker_defaults.ext_locker);
  mu_locker_defaults.ext_locker = p;
  return 0;
}

static int
legacy_locker_mod_flags (mu_locker_t lck, int flags,
			 enum mu_locker_set_mode mode)
{
  mu_locker_hints_t hints;
  int type = MU_LOCKER_FLAG_TO_TYPE (flags);
  
  flags &= MU_LOCKER_OPTION_MASK;
  
  switch (mode)
    {
    case mu_locker_assign:
      hints.flags = flags | MU_LOCKER_FLAG_TYPE;
      hints.type = type;
      break;
      
    case mu_locker_set_bit:
      hints.flags = lck->flags | flags | MU_LOCKER_FLAG_TYPE;
      hints.type = type;      
      break;
      
    case mu_locker_clear_bit:
      hints.flags = lck->flags & ~flags;
      if (type != MU_LOCKER_TYPE_DOTLOCK)
	{
	  hints.flags |= MU_LOCKER_FLAG_TYPE;
	  hints.type = MU_LOCKER_TYPE_DOTLOCK;	
	}
      break;
    }
  return mu_locker_modify (lck, &hints);
}

int
mu_locker_set_flags (mu_locker_t lck, int flags)
{
  return legacy_locker_mod_flags (lck, flags, mu_locker_assign);
}

int
mu_locker_mod_flags (mu_locker_t lck, int flags,
		     enum mu_locker_set_mode mode)
{
  return legacy_locker_mod_flags (lck, flags, mode);
}

int
mu_locker_set_expire_time (mu_locker_t lck, int v)
{
  mu_locker_hints_t hints;

  if (v < 0)
    return EINVAL;
  hints.flags = MU_LOCKER_FLAG_EXPIRE_TIME;
  hints.expire_time = v;
  return mu_locker_modify (lck, &hints);
}

int
mu_locker_set_retries (mu_locker_t lck, int v)
{
  mu_locker_hints_t hints;

  if (v < 0)
    return EINVAL;
  hints.flags = MU_LOCKER_FLAG_RETRY;
  hints.retry_count = v;
  return mu_locker_modify (lck, &hints);
}

int
mu_locker_set_retry_sleep (mu_locker_t lck, int v)
{
  mu_locker_hints_t hints;

  if (v < 0)
    return EINVAL;
  hints.flags = MU_LOCKER_FLAG_RETRY;
  hints.retry_sleep = v;
  return mu_locker_modify (lck, &hints);
}
  
int
mu_locker_set_external (mu_locker_t lck, const char *program)
{
  mu_locker_hints_t hints;

  if (lck->type != MU_LOCKER_TYPE_EXTERNAL)
    return EINVAL;
  
  if (!program)
    program = MU_LOCKER_DEFAULT_EXT_LOCKER;

  hints.flags = MU_LOCKER_FLAG_EXT_LOCKER;
  hints.ext_locker = (char*) program;
  return mu_locker_modify (lck, &hints);
}

int
mu_locker_get_flags (mu_locker_t lck, int *flags)
{
  mu_locker_hints_t hints;
  int rc;

  if (!flags)
    return EINVAL;
  
  hints.flags = MU_LOCKER_FLAGS_ALL;
  if ((rc = mu_locker_get_hints (lck, &hints)) != 0)
    return rc;
  *flags = hints.flags | MU_LOCKER_TYPE_TO_FLAG (hints.type);
  return 0;
}
  
int
mu_locker_get_expire_time (mu_locker_t lck, int *pv)
{
  int rc;
  mu_locker_hints_t hints;

  if (!pv)
    return EINVAL;

  hints.flags = MU_LOCKER_FLAG_EXPIRE_TIME;
  if ((rc = mu_locker_get_hints (lck, &hints)) != 0)
    return rc;
  if ((hints.flags & MU_LOCKER_FLAG_EXPIRE_TIME) == 0)
    *pv = 0;
  else
    {
      if (hints.expire_time > INT_MAX)
	return ERANGE;
      *pv = hints.expire_time;
    }
  return 0;
}

int
mu_locker_get_retries (mu_locker_t lck, int *pv)
{
  int rc;
  mu_locker_hints_t hints;

  if (!pv)
    return EINVAL;

  hints.flags = MU_LOCKER_FLAG_RETRY;
  if ((rc = mu_locker_get_hints (lck, &hints)) != 0)
    return rc;
  if ((hints.flags & MU_LOCKER_FLAG_RETRY) == 0)
    *pv = 0;
  else
    {
      if (hints.expire_time > INT_MAX)
	return ERANGE;
      *pv = hints.retry_count;
    }
  return 0;
}

int
mu_locker_get_retry_sleep (mu_locker_t lck, int *pv)
{
  int rc;
  mu_locker_hints_t hints;

  if (!pv)
    return EINVAL;

  hints.flags = MU_LOCKER_FLAG_RETRY;
  if ((rc = mu_locker_get_hints (lck, &hints)) != 0)
    return rc;
  if ((hints.flags & MU_LOCKER_FLAG_RETRY) == 0)
    *pv = 0;
  else
    {
      if (hints.expire_time > INT_MAX)
	return ERANGE;
      *pv = hints.retry_sleep;
    }
  return 0;
}

/* mu_locker_get_external was never implemented */  
  
