/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019 Free Software Foundation, Inc.

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
#include <sys/stat.h>
#include <errno.h>
#include <mailutils/sys/dotmail.h>
#include <mailutils/sys/folder.h>
#include <mailutils/sys/registrar.h>
#include <mailutils/url.h>
#include <mailutils/util.h>

static int
dotmail_is_scheme (mu_record_t record, mu_url_t url, int flags)
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
	      rc |= MU_FOLDER_ATTRIBUTE_FILE;
 	    }
	}
	  
      if ((flags & MU_FOLDER_ATTRIBUTE_DIRECTORY)
	  && S_ISDIR (st.st_mode))
	rc |= MU_FOLDER_ATTRIBUTE_DIRECTORY;
    }
  return rc;
}

static struct _mu_record _dotmail_record =
{
  MU_MBOX_PRIO,
  "dotmail",
  MU_RECORD_LOCAL,
  MU_URL_SCHEME | MU_URL_PATH | MU_URL_PARAM,
  MU_URL_PATH,
  mu_url_expand_path, /* URL init.  */
  mu_dotmail_mailbox_init, /* Mailbox init.  */
  NULL, /* Mailer init.  */
  _mu_fsfolder_init, /* Folder init.  */
  NULL, /* No need for an back pointer.  */
  dotmail_is_scheme, /* _is_scheme method.  */
  NULL, /* _get_url method.  */
  NULL, /* _get_mailbox method.  */
  NULL, /* _get_mailer method.  */
  NULL  /* _get_folder method.  */
};
mu_record_t mu_dotmail_record = &_dotmail_record;
