/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2004-2021 Free Software Foundation, Inc.

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

#ifdef ENABLE_MAILDIR

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <mailutils/sys/folder.h>
#include <mailutils/sys/registrar.h>

#include <maildir.h>
#include <mailutils/util.h>
#include <mailutils/url.h>
#include <mailutils/sys/amd.h>

/* Check if SUBDIR exists in directory NAME.  Return 0 on success,
   or error code on failure. */
static int
subdir_exists (const char *name, int subdir)
{
  struct stat st;
  char *s;

  s = mu_make_file_name (name, mu_maildir_subdir_name (subdir));
  if (!s)
    return ENOMEM; /* FIXME: error message */
  
  if (stat (s, &st) < 0)
    return errno;

  free (s);
  
  if (!S_ISDIR (st.st_mode))
    return ENOTDIR;

  return 0;
}

static int
_maildir_is_scheme (mu_record_t record, mu_url_t url, int flags)
{
  int scheme_matched = mu_url_is_scheme (url, record->scheme);
  int rc = 0;
  
  if (scheme_matched || mu_scheme_autodetect_p (url))
    {
      /* Attemp auto-detection */
      const char *path;
      struct stat st;

      if (mu_url_sget_path (url, &path))
        return 0;

      if (stat (path, &st) < 0)
	{
	  if (errno == ENOENT && scheme_matched)
	    return MU_FOLDER_ATTRIBUTE_ALL & flags; 
	  return 0; 
	}
      
      if (!S_ISDIR (st.st_mode))
	return 0;

      if (scheme_matched)
	rc = MU_FOLDER_ATTRIBUTE_ALL;
      else
	{
	  rc |= MU_FOLDER_ATTRIBUTE_DIRECTORY;
      
	  if ((flags & MU_FOLDER_ATTRIBUTE_FILE)
	      && subdir_exists (path, SUB_TMP) == 0
	      && subdir_exists (path, SUB_CUR) == 0
	      && subdir_exists (path, SUB_NEW) == 0)
	    rc |= MU_FOLDER_ATTRIBUTE_FILE;
	}
    }
  return rc & flags;
}

static int
_maildir_list_p (mu_record_t record, const char *name, int flags MU_ARG_UNUSED)
{
  return !mu_maildir_reserved_name (name);
}

static struct _mu_record _maildir_record =
{
  MU_MAILDIR_PRIO,
  MU_MAILDIR_SCHEME,
  MU_RECORD_LOCAL,
  MU_URL_SCHEME | MU_URL_PATH | MU_URL_PARAM,
  MU_URL_PATH,
  mu_url_expand_path, /* Url init.  */
  _mailbox_maildir_init, /* Mailbox init.  */
  NULL, /* Mailer init.  */
  _mu_fsfolder_init, /* Folder init.  */
  NULL, /* back pointer.  */
  _maildir_is_scheme, /* _is_scheme method.  */
  NULL, /* _get_url method.  */
  NULL, /* _get_mailbox method.  */
  NULL, /* _get_mailer method.  */
  NULL, /* _get_folder method.  */
  _maildir_list_p
};
mu_record_t mu_maildir_record = &_maildir_record;

#else
#include <stdio.h>
#include <mailutils/sys/registrar.h>
mu_record_t mu_maildir_record = NULL;
#endif
