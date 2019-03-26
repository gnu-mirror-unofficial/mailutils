/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2019 Free Software Foundation, Inc.

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
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>

#include <mailutils/auth.h>
#include <mailutils/debug.h>
#include <mailutils/iterator.h>
#include <mailutils/list.h>
#include <mailutils/monitor.h>
#include <mailutils/observer.h>
#include <mailutils/registrar.h>
#include <mailutils/url.h>
#include <mailutils/errno.h>
#include <mailutils/property.h>
#include <mailutils/mailbox.h>
#include <mailutils/imaputil.h>
#include <mailutils/util.h>
#include <mailutils/sys/folder.h>

int
mu_folder_glob_match (const char *name, void *pattern, int flags)
{
  return fnmatch (pattern, name[0] == '/' ? name + 1 : name, 0);
}

int
mu_folder_imap_match (const char *name, void *pattern, int flags)
{
  return mu_imap_wildmatch (pattern, name, '/');
}

int
mu_folder_create_from_record (mu_folder_t *pfolder, mu_url_t url,
			      mu_record_t record)
{
  if (!pfolder)
    return MU_ERR_OUT_PTR_NULL;

  if (record || 
      /* Look in the registrar list(iterator), for a possible concrete mailbox
	 implementation that could match the URL.  */
      mu_registrar_lookup_url (url, MU_FOLDER_ATTRIBUTE_DIRECTORY, &record,
			       NULL) == 0)
    {
      int (*f_init) (mu_folder_t) = NULL;
      
      mu_record_get_folder (record, &f_init);
      if (f_init)
        {
	  int status, mask;
	  mu_folder_t folder;
	  int (*u_init) (mu_url_t) = NULL;

	  status = mu_record_check_url (record, url, &mask);
	  if (status)
	    /* FIXME: mask would provide more info */
	    return status;
	  
	  mu_record_get_url (record, &u_init);
	  if (u_init)
	    {
	      status = u_init (url);
	      if (status)
		return status;
	    }
	  
	  /* Create a new folder.  */

	  /* Allocate memory for the folder.  */
	  folder = calloc (1, sizeof (*folder));
	  if (folder != NULL)
	    {
	      folder->url = url;
	      folder->is_local = record->flags & MU_RECORD_LOCAL;
	      /* Initialize the internal foilder lock, now so the
		 concrete folder could use it.  */
	      status = mu_monitor_create (&folder->monitor, 0, folder);
	      if (status == 0)
		{
		  /* Create the concrete folder type.  */
		  status = f_init (folder);
		  if (status == 0)
		    {
		      if (!folder->_match)
			folder->_match = mu_folder_imap_match;
		      *pfolder = folder;
		      folder->ref++;
		    }
		}
	      /* Something went wrong, destroy the object. */
	      if (status)
		{
		  if (folder->monitor)
		    mu_monitor_destroy (&folder->monitor, folder);
		  free (folder);
		}
	    }
	  return status;
	}
    }

  return MU_ERR_NOENT;
}

int
mu_folder_create (mu_folder_t *pfolder, const char *name)
{
  int rc;
  mu_url_t url;
  
  rc = mu_url_create (&url, name);
  if (rc)
    return rc;
  rc = mu_folder_create_from_record (pfolder, url, NULL);
  if (rc)
    mu_url_destroy (&url);
  return rc;
}

int
mu_folder_attach_ticket (mu_folder_t folder)
{
  mu_authority_t auth = NULL;
  int rc = MU_ERR_NOENT;

  if (mu_folder_get_authority (folder, &auth) == 0 && auth)
    {
      char *filename = mu_tilde_expansion (mu_ticket_file,
					   MU_HIERARCHY_DELIMITER, NULL);
      mu_wicket_t wicket;

      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_TRACE1,
		("Reading user ticket file %s", filename));
      if ((rc = mu_file_wicket_create (&wicket, filename)) == 0)
	{
	  mu_ticket_t ticket;

	  if ((rc = mu_wicket_get_ticket (wicket, NULL, &ticket)) == 0)
	    {
	      rc = mu_authority_set_ticket (auth, ticket);
	      mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_TRACE1,
			("Retrieved and set ticket: %d", rc));
	    }
	  else
	    mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		      ("Error retrieving ticket: %s\n",
		       mu_strerror (rc)));
	  mu_wicket_destroy (&wicket);
	}
      else
	mu_debug (MU_DEBCAT_MAILBOX, MU_DEBUG_ERROR,
		  ("Error creating wicket: %s\n", mu_strerror (rc)));
      free (filename);
    }
  return rc;
}

int
mu_folder_is_local (mu_folder_t folder)
{
  if (!folder)
    return -1;
  return folder->is_local;
}

/* The folder is destroy if it is the last reference.  */
void
mu_folder_destroy (mu_folder_t *pfolder)
{
  if (pfolder && *pfolder)
    {
      mu_folder_t folder = *pfolder;
      int destroy_lock = 0;
      mu_monitor_t monitor = folder->monitor;

      mu_monitor_wrlock (monitor);

      folder->ref--;

      if (folder->ref <= 0)
	{
	  mu_monitor_unlock (monitor);
	  destroy_lock = 1;
	  /* Notify the observers.  */
	  if (folder->observable)
	    {
	      mu_observable_notify (folder->observable, MU_EVT_FOLDER_DESTROY,
				    folder);
	      mu_observable_destroy (&folder->observable, folder);
	    }
	  if (folder->_destroy)
	    folder->_destroy (folder);
	  mu_monitor_wrlock (monitor);
	  if (folder->authority)
	    mu_authority_destroy (&folder->authority, folder);
	  if (folder->url)
	    mu_url_destroy (&folder->url);
	  if (folder->property)
	    mu_property_destroy (&folder->property);
	  free (folder);
	}
      mu_monitor_unlock (monitor);
      if (destroy_lock)
	mu_monitor_destroy (&monitor, folder);
      *pfolder = NULL;
    }
}

int
mu_folder_get_property (mu_folder_t folder, mu_property_t *prop)
{
  if (folder == NULL)
    return EINVAL;
  if (prop == NULL)
    return MU_ERR_OUT_PTR_NULL;
  
  if (folder->property == NULL)
    {
      int status;

      if (folder->_get_property)
	status = folder->_get_property (folder, &folder->property);
      else
	status = mu_property_create_init (&folder->property,
					  mu_assoc_property_init, NULL);
      if (status != 0)
	return status;
    }
  *prop = folder->property;
  return 0;
}


/* Cover functions.  */
int
mu_folder_open (mu_folder_t folder, int flags)
{
  if (folder == NULL)
    return EINVAL;
  if (folder->_open == NULL)
    return ENOSYS;
  return folder->_open (folder, flags);
}

int
mu_folder_close (mu_folder_t folder)
{
  if (folder == NULL)
    return EINVAL;
  if (folder->_close == NULL)
    return ENOSYS;
  return folder->_close (folder);
}

int
mu_folder_set_authority (mu_folder_t folder, mu_authority_t authority)
{
  if (folder == NULL)
    return EINVAL;
  if (folder->authority)
    mu_authority_destroy (&folder->authority, folder);
  folder->authority = authority;
  return 0;
}

int
mu_folder_get_authority (mu_folder_t folder, mu_authority_t *pauthority)
{
  if (folder == NULL)
    return EINVAL;
  if (pauthority == NULL)
    return MU_ERR_OUT_PTR_NULL;
  *pauthority = folder->authority;
  return 0;
}

int
mu_folder_get_observable (mu_folder_t folder, mu_observable_t *pobservable)
{
  if (folder == NULL)
    return EINVAL;
  if (pobservable == NULL)
    return MU_ERR_OUT_PTR_NULL;

  if (folder->observable == NULL)
    {
      int status = mu_observable_create (&folder->observable, folder);
      if (status != 0)
        return status;
    }
  *pobservable = folder->observable;
  return 0;
}

int
mu_folder_set_match (mu_folder_t folder, mu_folder_match_fp pmatch)
{
  if (folder == NULL)
    return EINVAL;
  folder->_match = pmatch;
  return 0;
}

int
mu_folder_get_match (mu_folder_t folder, mu_folder_match_fp *pmatch)
{
  if (folder == NULL)
    return EINVAL;
  if (pmatch == NULL)
    return MU_ERR_OUT_PTR_NULL;
  *pmatch = folder->_match;
  return 0;
}

void
mu_list_response_free (void *data)
{
  struct mu_list_response *f = data;
  free (f->name);
  free (f);
}

int
mu_folder_scan (mu_folder_t folder, struct mu_folder_scanner *scn)
{
  if (!folder || !scn)
    return EINVAL;
  if (folder->_list == NULL)
    return ENOSYS;
  if (scn->result)
    mu_list_set_destroy_item (scn->result, mu_list_response_free);
  return folder->_list (folder, scn);
}

int
mu_folder_list (mu_folder_t folder, const char *dirname, void *pattern,
		size_t max_depth,
		mu_list_t *pflist)
{
  return mu_folder_enumerate (folder, dirname, pattern,
			      MU_FOLDER_ATTRIBUTE_ALL, max_depth,
			      pflist, NULL, NULL);
}

int
mu_folder_enumerate (mu_folder_t folder, const char *name,
		     void *pattern, int flags,
		     size_t max_depth,
		     mu_list_t *pflist,
		     mu_folder_enumerate_fp enumfun, void *enumdata)
{
  int status;
  if (folder == NULL || (!pflist && !enumfun))
    return EINVAL;
  else if (folder->_list == NULL)
    return ENOSYS;
  else
    {
      struct mu_folder_scanner scn;

      scn.refname = name;
      scn.pattern = pattern;
      scn.match_flags = flags;
      scn.max_depth = max_depth;
      scn.enumfun = enumfun;
      scn.enumdata = enumdata;
      scn.records = NULL;
      if (pflist)
	{
	  status = mu_list_create (&scn.result);
	  if (status)
	    return status;
	}
      status = mu_folder_scan (folder, &scn);
      if (status == 0)
	{
	  if (pflist)
	    *pflist = scn.result;
	}
      else
	mu_list_destroy (&scn.result);
    }
  return status;
}

int
mu_folder_lsub (mu_folder_t folder, const char *dirname, const char *basename,
		mu_list_t *pflist)
{
  int status;
  
  if (folder == NULL)
    return EINVAL;
  else if (folder->_lsub == NULL)
    return ENOSYS;
  else
    {
      status = mu_list_create (pflist);
      if (status)
	return status;
      mu_list_set_destroy_item (*pflist, mu_list_response_free);
      status = folder->_lsub (folder, dirname, basename, *pflist);
    }
  return status;
}

int
mu_folder_subscribe (mu_folder_t folder, const char *name)
{
  if (folder == NULL)
    return EINVAL;
  if (folder->_subscribe == NULL)
    return ENOSYS;
  return folder->_subscribe (folder, name);
}

int
mu_folder_unsubscribe (mu_folder_t folder, const char *name)
{
  if (folder == NULL)
    return EINVAL;
  if (folder->_unsubscribe == NULL)
    return ENOSYS;
  return folder->_unsubscribe (folder, name);
}

int
mu_folder_delete (mu_folder_t folder, const char *name)
{
  int rc;
  
  if (folder == NULL)
    return EINVAL;
  if (folder->_delete)
    rc = folder->_delete (folder, name);
  else
    {
      /* If there is no folder-specific _delete method, then try to create the
	 mailbox and call mailbox delete (remove) method.  This is necessary
	 because certain types of mailboxes share a common folder (e.g. mbox,
	 maildir and mh all use filesystem folder), but have a different
	 internal structure.  Supplying mu_folder_t with knowledge about
	 mailbox internals will harm separation of concerns.  On the other 
	 hand, removing something without looking into it may well yield 
	 undesired results.  For example, a MH mailbox can hold another
	 mailboxes, i.e. be a folder itself.  Removing it blindly would 
	 result in removing these mailboxes as well, which is clearly not 
	 intended.

	 To solve this, both folder and mailbox delete methods are tightly
	 paired, but without looking into each-others internal mechanisms. */
      mu_mailbox_t mbox;
      rc = mu_mailbox_create_at (&mbox, folder, name);
      if (rc == 0)
	{
	  rc = mu_mailbox_remove (mbox);
	  mu_mailbox_destroy (&mbox);
	}
    }
  return rc;
}

int
mu_folder_rename (mu_folder_t folder, const char *oldname, const char *newname)
{
  if (folder == NULL)
    return EINVAL;
  if (folder->_rename == NULL)
    return ENOSYS;
  return folder->_rename (folder, oldname, newname);
}

int
mu_folder_get_url (mu_folder_t folder, mu_url_t *purl)
{
  if (folder == NULL)
    return EINVAL;
  if (purl == NULL)
    return MU_ERR_OUT_PTR_NULL;
  *purl = folder->url;
  return 0;
}

