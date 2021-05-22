/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2021 Free Software Foundation, Inc.

   GNU Mailutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Mailutils is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. */

#include "mail.h"
#include <mailutils/folder.h>

static int
list_response_printer (void *item, void *data)
{
  struct mu_list_response *resp = item;

  mu_printf ("%s", resp->name);
  if (resp->type & MU_FOLDER_ATTRIBUTE_DIRECTORY)
    mu_printf ("%c", resp->separator);
  mu_printf ("\n");
  return 0;
}

static int
show_folders (mu_folder_t folder)
{
  mu_list_t list;

  if (mu_folder_is_local (folder))
    {
      char *lister = getenv ("LISTER");
      if (lister && *lister)
	{
	  char const *path;
	  mu_url_t url;
	  int rc;
	  
	  mu_folder_get_url (folder, &url);
	  if ((rc = mu_url_sget_path (url, &path)) == 0)
	    {
	      util_do_command("! %s '%s'", getenv ("LISTER"), path);
	      return 0;
	    }
	  else
	    {
	      mu_diag_funcall (MU_DIAG_ERROR, "mu_url_sget_path", NULL, rc);
	      /* Retry using standard folder lister */
	    }
	}
    }
  
  if (mu_folder_list (folder, "", "%", 1, &list) == 0)
    {
      mu_list_foreach (list, list_response_printer, NULL);
      mu_list_destroy (&list);
    }

  return 0;
}

int
mail_folders (int argc MU_ARG_UNUSED, char **argv MU_ARG_UNUSED)
{
  int rc;
  mu_folder_t folder;
  mu_url_t url;
  char *folder_path, *temp_folder_path = NULL;
  
  if (mailvar_get (&folder_path, mailvar_name_folder, mailvar_type_string, 1))
    return 1;

  if (!mu_is_proto (folder_path) && folder_path[0] != '/' &&
      folder_path[0] != '~')
    {
      char *tmp = mu_alloc (strlen (folder_path) + 3);
      tmp[0] = '~';
      tmp[1] = '/';
      strcpy (tmp + 2, folder_path);
      temp_folder_path = util_fullpath (tmp);
      folder_path = temp_folder_path;
      free (tmp);
    }
  
  rc = mu_url_create (&url, folder_path);
  if (rc == 0)
    {
      rc = util_get_folder (&folder, url, any_folder);
      if (rc == 0)
	{
	  url = NULL; /* Prevent double free: folder steals url */
	  rc = show_folders (folder);
	  mu_folder_destroy (&folder);
	}
      else
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_mailbox_get_folder",
			   folder_path, rc);
	}
    }
  else
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_url_create", folder_path, rc);
    }

  free (temp_folder_path);
  mu_url_destroy (&url);
  return rc;
}
  
