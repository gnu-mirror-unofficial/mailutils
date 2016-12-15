#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <mailutils/stream.h>
#include <mailutils/util.h>
#include <mailutils/diag.h>
#include <mailutils/error.h>
#include <mailutils/errno.h>
#include <mailutils/nls.h>

static int copy_regular_file (const char *srcpath, const char *dstpath,
			      int flags, struct stat *st);
static int copy_symlink (const char *srcpath, const char *dstpath);
static int copy_dir (const char *srcpath, const char *dstpath, int flags);

int
mu_copy_file (const char *srcpath, const char *dstpath, int flags)
{
  int rc = 0;
  struct stat st;

  if (((flags & MU_COPY_SYMLINK) ? lstat : stat) (srcpath, &st))
    {
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		(_("can't stat file %s: %s"),
		 srcpath, mu_strerror (errno)));
      return errno;
    }

  switch (st.st_mode & S_IFMT)
    {
    case S_IFREG:
      return copy_regular_file (srcpath, dstpath, flags, &st);

    case S_IFLNK:
      return copy_symlink (srcpath, dstpath);
      break;

    case S_IFDIR:
      return copy_dir (srcpath, dstpath, flags);
      break;

    case S_IFBLK:
    case S_IFCHR:
      if (mknod (dstpath, st.st_mode & 0777, st.st_dev))
	{
	  rc = errno;
	  mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		    (_("%s: cannot create node: %s"),
		     dstpath,
		     mu_strerror (rc)));
	}
      break;
      
    case S_IFIFO:
      if (mkfifo (dstpath, st.st_mode & 0777))
	{
	  rc = errno;
	  mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		    (_("%s: cannot create node: %s"),
		     dstpath,
		     mu_strerror (rc)));
	}
      break;

    default:
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		(_("%s: don't know how to copy file of that type"),
		 srcpath));
      return ENOTSUP;
    }

  return rc;
}
      
static int
copy_regular_file (const char *srcpath, const char *dstpath, int flags,
		   struct stat *st)
{
  int rc;
  mu_stream_t src, dst;
  mode_t mask, mode;

  rc = mu_file_stream_create (&src, srcpath, MU_STREAM_READ);
  if (rc)
    {
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		(_("cannot open source file %s: %s"),
		 srcpath, mu_strerror (rc)));
      return rc;
    }

  mask = umask (077);
  mode = ((flags & MU_COPY_MODE) ? st->st_mode : (0666 & ~mask)) & 0777;

  rc = mu_file_stream_create (&dst, dstpath, MU_STREAM_CREAT|MU_STREAM_WRITE);
  umask (mask);
  if (rc)
    {
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		(_("cannot open destination file %s: %s"),
		 dstpath, mu_strerror (rc)));
      mu_stream_destroy (&src);
      return rc;
    }

  rc = mu_stream_copy (dst, src, 0, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		(_("failed to copy %s to %s: %s"),
		 srcpath, dstpath, mu_strerror (rc)));
    }
  else 
    {
      mu_transport_t trans[2];

      rc = mu_stream_ioctl (dst, MU_IOCTL_TRANSPORT, MU_IOCTL_OP_GET, trans);
      if (rc == 0)
	{	    
	  if (fchmod ((int) trans[0], mode))
	    {
	      rc = errno;
	      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
			(_("%s: cannot chmod: %s"),
			 dstpath, mu_strerror (rc)));
	    }
	  else if (flags & MU_COPY_OWNER)
	    {
	      uid_t uid;
	      gid_t gid;
	      
	      if (getuid () == 0)
		{
		  uid = st->st_uid;
		  gid = st->st_gid;
		}
	      else if (getuid () == st->st_uid)
		{
		  uid = -1;
		  gid = st->st_gid;
		}
	      else
		{
		  uid = -1;
		  gid = -1;
		}

	      if (gid != -1)
		{
		  if (fchown ((int) trans[0], uid, gid))
		    {
		      rc = errno;
		      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
				(_("%s: cannot chown to %lu.%lu: %s"),
				 dstpath,
				 (unsigned long) uid,
				 (unsigned long) gid,
				 mu_strerror (rc)));
		    }
		}
	    }
	}
      else
	{
	  mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		    (_("can't change file mode and ownership after copying %s to %s;"
		       " cannot get file handle: %s"),
		     srcpath, dstpath,
		     mu_strerror (rc)));
	}      
    }
  
  mu_stream_destroy (&src);
  mu_stream_destroy (&dst);
  
  return rc;
}

static int
copy_symlink (const char *srcpath, const char *dstpath)
{
  int rc;
  char *buf = NULL;
  size_t size = 0;
  
  rc = mu_readlink (srcpath, &buf, &size, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		(_("%s: cannot read link: %s"),
		 srcpath, mu_strerror (rc)));
      return rc;
    }

  if (symlink (buf, dstpath))
    {
      rc = errno;
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		(_("%s: can't link %s to %s: %s"),
		 srcpath, buf, dstpath, mu_strerror (rc)));
    }
  free (buf);
  return rc;
}
  
static int
copy_dir (const char *srcpath, const char *dstpath, int flags)
{
  DIR *dirp;
  struct dirent *dp;
  struct stat st, st1;
  int rc;
  int create = 0;
  mode_t mode, mask;
  
  if (stat (srcpath, &st))
    {
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		(_("can't stat file %s: %s"),
		 srcpath, mu_strerror (errno)));
      return errno;
    }

  if (stat (dstpath, &st1))
    {
      if (errno == ENOENT)
	create = 1;
      else
	{
	  mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		    (_("can't stat directory %s: %s"),
		     dstpath, mu_strerror (errno)));
	  return errno;
	}
    }
  else if (!S_ISDIR (st1.st_mode))
    {
      if (flags & MU_COPY_FORCE)
	{
	  if (unlink (dstpath))
	    {
	      rc = errno;
	      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
			(_("%s is not a directory and cannot be unlinked: %s"),
			 dstpath, mu_strerror (rc)));
	      return rc;
	    }
	  create = 1;
	}
      else
	{
	  mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		    (_("%s is not a directory"),
		     dstpath));
	  return EEXIST;
	}
    }      

  mask = umask (077);
  mode = ((flags & MU_COPY_MODE) ? st.st_mode : (0777 & ~mask)) & 0777;
  
  if (create)
    {
      rc = mkdir (dstpath, 0700);
      umask (mask);
	  
      if (rc)
	{
	  mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		    (_("can't create directory %s: %s"),
		     dstpath, mu_strerror (errno)));
	  return errno;
	}
    }
  else
    umask (mask);
  
  dirp = opendir (srcpath);
  if (dirp == NULL)
    {
      mu_debug (MU_DEBCAT_FOLDER, MU_DEBUG_ERROR,
		("cannot open directory %s: %s",
		 srcpath, mu_strerror (errno)));
      return 1;
    }

  while ((dp = readdir (dirp)))
    {
      char const *ename = dp->d_name;
      char *src, *dst;
      
      if (ename[ename[0] != '.' ? 0 : ename[1] != '.' ? 1 : 2] == 0)
	continue;

      src = mu_make_file_name (srcpath, ename);
      dst = mu_make_file_name (dstpath, ename);
      rc = mu_copy_file (src, dst, flags);
      free (dst);
      free (src);

      if (rc)
	break;
    }
  closedir (dirp);

  if (chmod (dstpath, mode))
    {
      rc = errno;
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		(_("%s: cannot chmod: %s"),
		 dstpath, mu_strerror (rc)));
    }
  else if (flags & MU_COPY_OWNER)
    {
      uid_t uid;
      gid_t gid;
	      
      if (getuid () == 0)
	{
	  uid = st.st_uid;
	  gid = st.st_gid;
	}
      else if (getuid () == st.st_uid)
	{
	  uid = -1;
	  gid = st.st_gid;
	}
      else
	{
	  uid = -1;
	  gid = -1;
	}
      
      if (gid != -1)
	{
	  if (chown (dstpath, uid, gid))
	    {
	      rc = errno;
	      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
			(_("%s: cannot chown to %lu.%lu: %s"),
			 dstpath,
			 (unsigned long) uid,
			 (unsigned long) gid,
			 mu_strerror (rc)));
	    }
	}
    }
  return rc;
}

