/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2011-2019 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <mailutils/error.h>
#include <mailutils/errno.h>
#include <mailutils/folder.h>
#include <mailutils/stream.h>
#include <mailutils/stdstream.h>
#include <mailutils/list.h>
#include <mailutils/url.h>
#include <mailutils/util.h>
#include <mailutils/registrar.h>
#include <mailutils/sys/folder.h>
#include <mailutils/sys/registrar.h>
#include <mailutils/opt.h>
#include "tesh.h"

int sort_option;
int prefix_len;

static int
compare_response (void const *a, void const *b)
{
  struct mu_list_response const *ra = a;
  struct mu_list_response const *rb = b;

  if (ra->level < rb->level)
    return -1;
  if (ra->level > rb->level)
    return 1;
  return strcmp (ra->name, rb->name);
}

static int
_print_list_entry (void *item, void *data)
{
  struct mu_list_response *resp = item;
  int len = data ? *(int*) data : 0;
  mu_printf ("%c%c %c %4d %s\n",
	     (resp->type & MU_FOLDER_ATTRIBUTE_DIRECTORY) ? 'd' : '-',
	     (resp->type & MU_FOLDER_ATTRIBUTE_FILE) ? 'f' : '-',
	     resp->separator ? resp->separator : ' ',
	     resp->level,
	     resp->name + len);
  return 0;
}

static int
com_list (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_folder_t folder = env;
  int rc;
  mu_list_t list;
  
  mu_printf ("listing '%s' '%s'\n", argv[1], argv[2]);
  rc = mu_folder_list (folder, argv[1], argv[2], 0, &list);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_folder_list", argv[1], rc);
  else
    {
      if (sort_option)
	mu_list_sort (list, compare_response);
      mu_list_foreach (list, _print_list_entry, &prefix_len);
      mu_list_destroy (&list);
    }
  return 0;
}

static int
com_lsub (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_folder_t folder = env;
  int rc;
  mu_list_t list;
  
  mu_printf ("listing subscriptions for '%s' '%s'\n", argv[1], argv[2]);
  rc = mu_folder_lsub (folder, argv[1], argv[2], &list);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_folder_lsub", argv[1], rc);
  else
    {
      if (sort_option)
	mu_list_sort (list, compare_response);
      mu_list_foreach (list, _print_list_entry, NULL);
      mu_list_destroy (&list);
    }
  return 0;
}

static int
com_rename (int argc, char **argv, mu_assoc_t options, void *env)
{
  int rc;
  mu_folder_t folder = env;
  
  mu_printf ("renaming %s to %s\n", argv[1], argv[2]);
  rc = mu_folder_rename (folder, argv[1], argv[2]);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_folder_rename", argv[1], rc);
  else
    mu_printf ("rename successful\n");
  return 0;
}

static int
com_subscribe (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_folder_t folder = env;
  int rc;

  mu_printf ("subscribing %s\n", argv[1]);
  rc = mu_folder_subscribe (folder, argv[1]);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_folder_subscribe", argv[1], rc);
  else
    mu_printf ("subscribe successful\n");
  return 0;
}

static int
com_unsubscribe (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_folder_t folder = env;
  int rc;

  mu_printf ("unsubscribing %s\n", argv[1]);
  rc = mu_folder_unsubscribe (folder, argv[1]);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_folder_unsubscribe", argv[1], rc);
  else
    mu_printf ("unsubscribe successful\n");
  return 0;
}

static struct mu_tesh_command comtab[] = {
  { "list", "REF MBX", com_list },
  { "lsub", "REF MBX", com_lsub },
  { "rename", "OLD NEW", com_rename },
  { "subscribe", "MBX", com_subscribe },
  { "unsubscribe", "MBX", com_unsubscribe },
  { NULL }
};

static void
usage (void)
{
  mu_printf (
    "usage: %s [-debug=SPEC] -name=URL [-sort] [-glob] OP ARG... [\\; OP ARG...]...]\n",
    mu_program_name);
  mu_printf ("OPerations and corresponding ARGuments are:\n");
  mu_tesh_help (comtab, NULL);
}

static int
_always_is_scheme (mu_record_t record, mu_url_t url, int flags)
{
  int res = 0;
  char const *p;
  struct stat st;
  int rc = mu_url_sget_path (url, &p);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_url_sget_path", NULL, rc);
      return 0;
    }

  if (lstat (p, &st))
    {
      mu_diag_funcall (MU_DIAG_ERROR, "lstat", p, rc);
      return 0;
    }
      
  if (S_ISDIR (st.st_mode))
    res |= MU_FOLDER_ATTRIBUTE_DIRECTORY;
  if (S_ISREG (st.st_mode))
    res |= MU_FOLDER_ATTRIBUTE_FILE;
  if (S_ISLNK (st.st_mode))
    res |= MU_FOLDER_ATTRIBUTE_LINK;
  
  return res & flags;
}

static struct _mu_record test_record =
{
  0,
  "file",
  MU_RECORD_LOCAL,
  MU_URL_SCHEME | MU_URL_PATH,
  MU_URL_PATH,
  mu_url_expand_path, /* URL init.  */
  NULL, /* Mailbox init.  */
  NULL, /* Mailer init.  */
  _mu_fsfolder_init, /* Folder init.  */
  NULL, /* No need for an back pointer.  */
  _always_is_scheme, /* _is_scheme method.  */
  NULL, /* _get_url method.  */
  NULL, /* _get_mailbox method.  */
  NULL, /* _get_mailer method.  */
  NULL  /* _get_folder method.  */
};

int
main (int argc, char **argv)
{
  int i;
  int rc;
  mu_folder_t folder;
  char *fname = NULL;
  int glob_option = 0;

  mu_tesh_init (argv[0]);
  mu_registrar_record (&test_record);

  if (argc == 1)
    {
      usage ();
      exit (0);
    }

  for (i = 1; i < argc; i++)
    {
      if (strncmp (argv[i], "-debug=", 7) == 0)
	mu_debug_parse_spec (argv[i] + 7);
      else if (strncmp (argv[i], "-name=", 6) == 0)
	fname = argv[i] + 6;
      else if (strcmp (argv[i], "-sort") == 0)
	sort_option = 1;
      else if (strcmp (argv[i], "-glob") == 0)
	glob_option = 1;
      else
	break;
    }

  if (!fname)
    {
      mu_error ("name not specified");
      exit (1);
    }
  
  if (fname[0] != '/')
    {
      char *cwd = mu_getcwd ();
      prefix_len = strlen (cwd);
      if (cwd[prefix_len-1] != '/')
	prefix_len++;
      fname = mu_make_file_name (cwd, fname);
      free (cwd);
    }
  
  rc = mu_folder_create (&folder, fname);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_folder_create", fname, rc);
      return 1;
    }
  rc = mu_folder_open (folder, MU_STREAM_READ);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_folder_open", fname, rc);
      return 1;
    }

  if (glob_option)
    mu_folder_set_match (folder, mu_folder_glob_match);

  mu_tesh_read_and_eval (argc - i, argv + i, comtab, folder);

  mu_folder_close (folder);
  mu_folder_destroy (&folder);

  return 0;
}
