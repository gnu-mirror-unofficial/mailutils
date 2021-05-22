/* Implementation of file-system folder for GNU Mailutils
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
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <glob.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>

#include <mailutils/sys/folder.h>
#include <mailutils/sys/registrar.h>

#include <mailutils/auth.h>
#include <mailutils/url.h>
#include <mailutils/stream.h>
#include <mailutils/util.h>
#include <mailutils/errno.h>
#include <mailutils/debug.h>
#include <mailutils/property.h>
#include <mailutils/iterator.h>

/* File-system folder is shared between UNIX mbox, maildir and MH
   mailboxes.  It implements all usual folder methods, excepting
   for _delete, which is implemented on the mailbox level.  See
   comment to mu_folder_delete in folder.c */

struct _mu_fsfolder
{
  char *dirname;
  mu_property_t subscription;
};

static int
open_subscription (struct _mu_fsfolder *folder)
{
  int rc;
  mu_property_t prop;
  mu_stream_t str;
  char *filename = mu_make_file_name (folder->dirname, ".mu-subscr");
  
  rc = mu_file_stream_create (&str, filename, MU_STREAM_RDWR|MU_STREAM_CREAT);
  if (rc)
    return rc;
  rc = mu_property_create_init (&prop, mu_assoc_property_init, str);
  free (filename);
  if (rc == 0)
    folder->subscription = prop;
  return rc;
}


static char *
get_pathname (const char *dirname, const char *basename)
{
  char *pathname = NULL, *p;

  /* Skip eventual protocol designator. */
  p = strchr (dirname, ':');
  if (p && p[1] == '/' && p[2] == '/')
    dirname = p + 3;
  
  /* null basename gives dirname.  */
  if (basename == NULL)
    pathname = strdup (dirname ? dirname : ".");
  /* Absolute.  */
  else if (basename[0] == '/')
    pathname = strdup (basename);
  /* Relative.  */
  else
    {
      size_t baselen = strlen (basename);
      size_t dirlen = strlen (dirname);
      while (dirlen > 0 && dirname[dirlen-1] == '/')
	dirlen--;
      pathname = calloc (dirlen + baselen + 2, sizeof (char));
      if (pathname)
	{
	  memcpy (pathname, dirname, dirlen);
	  pathname[dirlen] = '/';
	  strcpy (pathname + dirlen + 1, basename);
	}
    }
  return pathname;
}

static void
_fsfolder_destroy (mu_folder_t folder)
{
  if (folder->data)
    {
      struct _mu_fsfolder *fsfolder = folder->data;
      free (fsfolder->dirname);
      mu_property_destroy (&fsfolder->subscription);
      free (folder->data);
      folder->data = NULL;
    }
}

/* Noop. */
static int
_fsfolder_open (mu_folder_t folder, int flags MU_ARG_UNUSED)
{
  struct _mu_fsfolder *fsfolder = folder->data;
  if (flags & MU_STREAM_CREAT)
    {
      return (mkdir (fsfolder->dirname, S_IRWXU) == 0) ? 0 : errno;
    }
  return 0;
}

/*  Noop.  */
static int
_fsfolder_close (mu_folder_t folder MU_ARG_UNUSED)
{
  int rc = 0;
  struct _mu_fsfolder *fsfolder = folder->data;
  
  if (fsfolder->subscription)
    rc = mu_property_save (fsfolder->subscription);
  return rc;
}

static int
_fsfolder_rename (mu_folder_t folder, const char *oldpath,
		  const char *newpath)
{
  struct _mu_fsfolder *fsfolder = folder->data;
  if (oldpath && newpath)
    {
      int status = 0;
      char *pathold = get_pathname (fsfolder->dirname, oldpath);
      if (pathold)
	{
	  char *pathnew = get_pathname (fsfolder->dirname, newpath);
	  if (pathnew)
	    {
	      if (access (pathnew, F_OK) == 0)
		status = EEXIST;
	      else if (rename (pathold, pathnew) != 0)
		status = errno;
	      free (pathnew);
	    }
	  else
	    status = ENOMEM;
	  free (pathold);
	}
      else
	status = ENOMEM;
      return status;
    }
  return EINVAL;
}

struct inode_list           /* Inode/dev number list used to cut off
			       recursion */
{
  struct inode_list *next;
  ino_t inode;
  dev_t dev;
};

struct folder_scan_data
{
  mu_folder_t folder;
  char *dirname;
  size_t dirlen;
  size_t prefix_len;
  size_t errcnt;
};

static int
inode_list_lookup (struct inode_list *list, struct stat *st)
{
  for (; list; list = list->next)
    if (list->inode == st->st_ino && list->dev == st->st_dev)
      return 1;
  return 0;
}

static int
fold_record_match (void *item, void *data, void *prev, void **ret)
{
  struct mu_record_match *cur_match = item;
  struct mu_record_match *prev_match = prev;
  if (prev == NULL || cur_match->flags >= prev_match->flags)
    *ret = cur_match;
  else
    *ret = prev_match;
  return 0;
}

/* List item comparator for computing an intersection between a list
   of mu_record_t objects and a list of struct mi_record_match pointers.
*/
static int
mcomp (const void *a, const void *b)
{
  struct _mu_record const * r = a;
  struct mu_record_match const *m = b;
  return !(m->record == r);
}

/* Find a record from RECORDS that is the best match for mailbox REFNAME.
   Return the record found in PREC and mailbox attribute flags (the
   MU_FOLDER_ATTRIBUTE_* bitmask) in PFLAGS.

   Return 0 on success, MU_ERR_NOENT if no match was found and mailutils
   error code on error.
 */
static int
best_match (mu_list_t records, char const *refname,
	    mu_record_t *prec, int *pflags)
{
  int rc;
  mu_list_t mlist, isect;
  mu_list_comparator_t prev;
  struct mu_record_match *m;

  rc = mu_registrar_match_records (refname,
				   MU_FOLDER_ATTRIBUTE_ALL,
				   &mlist);
  if (rc)
    {
      mu_debug (MU_DEBCAT_FOLDER, MU_DEBUG_ERROR,
		("%s():%s: %s",
		 __func__,
		 "mu_registrar_match_records",
		 mu_strerror (rc)));
      return rc;
    }

  prev = mu_list_set_comparator (records, mcomp);
  rc = mu_list_intersect (&isect, mlist, records);
  mu_list_set_comparator (records, prev);
  if (rc)
    {
      mu_debug (MU_DEBCAT_FOLDER, MU_DEBUG_ERROR,
		("%s():%s: %s",
		 __func__,
		 "mu_list_intersect",
		 mu_strerror (rc)));
      mu_list_destroy (&mlist);
      return rc;
    }

  rc = mu_list_fold (isect, fold_record_match, NULL, NULL, &m);
  if (rc == 0)
    {
      if (m == NULL)
	rc = MU_ERR_NOENT;
      else
	{
	  *prec = m->record;
	  *pflags = m->flags;
	}
    }
  else
    {
      mu_debug (MU_DEBCAT_FOLDER, MU_DEBUG_ERROR,
		("%s():%s: %s",
		 __func__,
		 "mu_list_fold",
		 mu_strerror (rc)));
    }
  mu_list_destroy (&mlist);
  mu_list_destroy (&isect);
  return rc;
}

static int
list_helper (struct mu_folder_scanner *scn,
	     struct folder_scan_data *data,
	     struct inode_list *ilist,
	     const char *dirname, size_t depth)
{
  DIR *dirp;
  struct dirent *dp;
  int stop = 0;
    
  if (scn->max_depth && depth >= scn->max_depth)
    return 0;

  dirp = opendir (dirname);
  if (dirp == NULL)
    {
      mu_debug (MU_DEBCAT_FOLDER, MU_DEBUG_ERROR,
		("%s: %s(%s): %s",
		 __func__, "opendir", dirname, mu_strerror (errno)));
      data->errcnt++;
      return 1;
    }

  while ((dp = readdir (dirp)))
    {
      char const *ename = dp->d_name;
      char *fname;
      struct stat st;
      
      if (ename[ename[0] != '.' ? 0 : ename[1] != '.' ? 1 : 2] == 0)
	continue;
      if (strncmp (ename, ".mu-", 4) == 0)
	continue;
      fname = get_pathname (dirname, ename);
      if (lstat (fname, &st) == 0)
	{
	  int f;
	  if (S_ISDIR (st.st_mode))
	    f = MU_FOLDER_ATTRIBUTE_DIRECTORY;
	  else if (S_ISREG (st.st_mode))
	    f = MU_FOLDER_ATTRIBUTE_FILE;
	  else if (S_ISLNK (st.st_mode))
	    f = MU_FOLDER_ATTRIBUTE_LINK;
	  else
	    f = 0;
	  if (mu_registrar_list_p (scn->records, ename, f))
	    {
	      if (scn->pattern == NULL
		  || data->folder->_match == NULL
		  || data->folder->_match (fname + data->dirlen +
					   ((data->dirlen > 1
					     && data->dirname[data->dirlen-1] != '/') ?
					    1 : 0),
					   scn->pattern,
					   scn->match_flags) == 0)
		{
		  char *refname = fname;
		  int type = 0;
		  struct mu_list_response *resp;
		  mu_record_t rec = NULL;
		  int rc;
		  
		  resp = malloc (sizeof (*resp));
		  if (resp == NULL)
		    {
		      mu_debug (MU_DEBCAT_FOLDER, MU_DEBUG_ERROR,
				("%s: %s", __func__, mu_strerror (ENOMEM)));
		      data->errcnt++;
		      free (fname);
		      continue;
		    }

		  if (scn->records)
		    rc = best_match (scn->records, refname, &rec, &type);
		  else
		    rc = mu_registrar_lookup (refname, MU_FOLDER_ATTRIBUTE_ALL,
					      &rec, &type);

		  if (rc || type == 0)
		    {
		      free (resp);
		      if (f == MU_FOLDER_ATTRIBUTE_DIRECTORY)
			type = f;
		    }
		  else
		    {
		      resp->name = strdup (fname + data->prefix_len + 1);
		      resp->depth = depth;
		      resp->separator = '/';
		      resp->type = type;
		      resp->format = rec;

		      if (scn->enumfun)
			{
			  if (scn->enumfun (data->folder, resp, scn->enumdata))
			    {
			      free (resp->name);
			      free (resp);
			      stop = 1;
			      break;
			    }
			}
		  
		      if (scn->result)
			{
			  int rc;
			  rc = mu_list_append (scn->result, resp);
			  if (rc)
			    mu_debug (MU_DEBCAT_FOLDER, MU_DEBUG_ERROR,
				      ("%s(%s):%s: %s",
				       __func__, dirname, "mu_list_append",
				       mu_strerror (rc)));

			  /* Prevent fname from being freed at the end of the
			     loop
			  */
			  fname = NULL;
			}
		      else
			free (resp);
		    }
		  
		  if ((type & MU_FOLDER_ATTRIBUTE_DIRECTORY)
		      && !inode_list_lookup (ilist, &st))
		    {
		      struct inode_list idata;
		      
		      idata.inode = st.st_ino;
		      idata.dev   = st.st_dev;
		      idata.next  = ilist;
		      stop = list_helper (scn, data, &idata, refname,
					  depth + 1);
		    }
		}
	      else if (S_ISDIR (st.st_mode))
		{
		  struct inode_list idata;
		  
		  idata.inode = st.st_ino;
		  idata.dev   = st.st_dev;
		  idata.next  = ilist;
		  stop = list_helper (scn, data, &idata, fname, depth + 1);
		}
	    }
	}
      else
	{
	  mu_debug (MU_DEBCAT_FOLDER, MU_DEBUG_ERROR,
		    ("%s: lstat(%s): %s",
		     __func__, fname, mu_strerror (errno)));
	}
      free (fname);
    }
  closedir (dirp);
  return stop;
}

static int
_fsfolder_list (mu_folder_t folder, struct mu_folder_scanner *scn)
{
  struct _mu_fsfolder *fsfolder = folder->data;
  struct inode_list iroot;
  struct folder_scan_data sdata;
  
  memset (&iroot, 0, sizeof iroot);
  sdata.folder = folder;
  sdata.dirname = get_pathname (fsfolder->dirname, scn->refname);
  sdata.dirlen = strlen (sdata.dirname);
  sdata.prefix_len = strlen (fsfolder->dirname);
  if (sdata.prefix_len > 0 && fsfolder->dirname[sdata.prefix_len-1] == '/')
    sdata.prefix_len--;
  sdata.errcnt = 0;
  list_helper (scn, &sdata, &iroot, sdata.dirname, 0);
  free (sdata.dirname);
  /* FIXME: error code */
  return 0;
}

static int
_fsfolder_lsub (mu_folder_t folder, const char *ref, const char *name,
		mu_list_t flist)
{
  struct _mu_fsfolder *fsfolder = folder->data;
  int rc;
  char *pattern;
  mu_iterator_t itr;
  
  if (name == NULL || *name == '\0')
    name = "*";

  if (!fsfolder->subscription && (rc = open_subscription (fsfolder)))
    return rc;
    
  pattern = mu_make_file_name (ref, name);
  
  rc = mu_property_get_iterator (fsfolder->subscription, &itr);
  if (rc == 0)
    {
      for (mu_iterator_first (itr); !mu_iterator_is_done (itr);
	   mu_iterator_next (itr))
	{
	  const char *key, *val;
	  
	  mu_iterator_current_kv (itr, (const void **)&key, (void**)&val);

	  if (fnmatch (pattern, key, 0) == 0)
	    {
	      struct mu_list_response *resp;
	      resp = malloc (sizeof (*resp));
	      if (resp == NULL)
		{
		  rc = ENOMEM;
		  break;
		}
	      else if ((resp->name = strdup (key)) == NULL)
		{
		  free (resp);
		  rc = ENOMEM;
		  break;
		}
	      resp->type = MU_FOLDER_ATTRIBUTE_FILE;
	      resp->depth = 0;
	      resp->separator = '/';
	      rc = mu_list_append (flist, resp);
	      if (rc)
		{
		  free (resp);
		  break;
		}
	    }
	}
      mu_iterator_destroy (&itr);
    }
  free (pattern);
  return rc;
}

static int
_fsfolder_subscribe (mu_folder_t folder, const char *name)
{
  struct _mu_fsfolder *fsfolder = folder->data;
  int rc;
  
  if (!fsfolder->subscription && (rc = open_subscription (fsfolder)))
    return rc;

  return mu_property_set_value (fsfolder->subscription, name, "", 1);
}  

static int
_fsfolder_unsubscribe (mu_folder_t folder, const char *name)
{
  struct _mu_fsfolder *fsfolder = folder->data;
  int rc;

  if (!fsfolder->subscription && (rc = open_subscription (fsfolder)))
    return rc;

  return mu_property_unset (fsfolder->subscription, name);
}

static int
_fsfolder_get_authority (mu_folder_t folder, mu_authority_t *pauth)
{
  int status = 0;
  if (folder->authority == NULL)
    status = mu_authority_create_null (&folder->authority, folder);
  if (!status && pauth)
    *pauth = folder->authority;
  return status;
}

int
_mu_fsfolder_init (mu_folder_t folder)
{
  struct _mu_fsfolder *dfolder;
  int status = 0;

  /* We create an authority so the API is uniform across the mailbox
     types. */
  status = _fsfolder_get_authority (folder, NULL);
  if (status != 0)
    return status;

  dfolder = folder->data = calloc (1, sizeof (*dfolder));
  if (dfolder == NULL)
    return ENOMEM;

  status = mu_url_aget_path (folder->url, &dfolder->dirname);
  if (status == MU_ERR_NOENT)
    {
      dfolder->dirname = malloc (2);
      if (dfolder->dirname == NULL)
	status = ENOMEM;
      else
	{
	  strcpy (dfolder->dirname, ".");
	  status = 0;
	}
    }
  
  if (status)  
    {
      free (dfolder);
      folder->data = NULL;
      return status;
    }

  folder->_destroy = _fsfolder_destroy;

  folder->_open = _fsfolder_open;
  folder->_close = _fsfolder_close;

  folder->_list = _fsfolder_list;
  folder->_lsub = _fsfolder_lsub;
  folder->_subscribe = _fsfolder_subscribe;
  folder->_unsubscribe = _fsfolder_unsubscribe;
  folder->_delete = NULL;
  folder->_rename = _fsfolder_rename;
  return 0;
}

