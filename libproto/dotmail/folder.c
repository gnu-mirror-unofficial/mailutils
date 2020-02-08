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
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <mailutils/sys/dotmail.h>
#include <mailutils/sys/folder.h>
#include <mailutils/sys/registrar.h>
#include <mailutils/url.h>
#include <mailutils/util.h>
#include <mailutils/cctype.h>

/* Return MU_FOLDER_ATTRIBUTE_FILE if NAME looks like a dotmail
   mailbox.

   If MU_AUTODETECT_ACCURACY is 0 (i.e. autodetection is disabled),
   always returns MU_FOLDER_ATTRIBUTE_FILE.
   
   Otherwise, the function analyzes first 128 bytes from file. If they
   look like a message header start, i.e. match "^[A-Za-z_][A-Za-z0-9_-]*:",
   then the file is considered a dotmail mailbox.

   Additionally, if MU_AUTODETECT_ACCURACY is greater than 1, the last
   3 characters of the file are considered. For valid dotmail they must
   be "\n.\n".
*/
static int
dotmail_detect (char const *name)
{
  FILE *fp;
  int rc = 0;

  if (mu_autodetect_accuracy () == 0)
    return MU_FOLDER_ATTRIBUTE_FILE;
  
  fp = fopen (name, "r");
  if (fp)
    {
      size_t i;
      int c;
      /* Allowed character classes for first and subsequent characters
	 in the first line: */
      int allowed[] = {  MU_CTYPE_IDENT, MU_CTYPE_HEADR };

      for (i = 0;
	   i < 128
	     && (c = getc (fp)) != EOF
	     && mu_c_is_class (c, allowed[i>0]);
	   i++)
	;
      if (c == ':')
	{
	  /* Possibly a header line */
	  char buf[3];
	  if (mu_autodetect_accuracy () == 1
	      || (fseek (fp, -3, SEEK_END) == 0
		  && fread (buf, 3, 1, fp) == 1
		  && memcmp (buf, "\n.\n", 3) == 0))
	    rc = MU_FOLDER_ATTRIBUTE_FILE;
	}
      fclose (fp);
    }

  return rc;
}

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
	      rc |= dotmail_detect (path);
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
  mu_url_expand_path, /* URL init. */
  mu_dotmail_mailbox_init, /* Mailbox init.  */
  NULL, /* Mailer init.  */
  _mu_fsfolder_init, /* Folder init.  */
  NULL, /* No need for back pointer.  */
  dotmail_is_scheme, /* _is_scheme method.  */
  NULL, /* _get_url method.  */
  NULL, /* _get_mailbox method.  */
  NULL, /* _get_mailer method.  */
  NULL  /* _get_folder method.  */
};
mu_record_t mu_dotmail_record = &_dotmail_record;
