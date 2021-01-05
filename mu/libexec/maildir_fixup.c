/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2010-2021 Free Software Foundation, Inc.

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

#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif
#include <mailutils/mailutils.h>
#include <sysexits.h>
#include "mu.h"
#include "muaux.h"

/* Utility-specific exit codes */
enum
  {
    EXIT_OK,        /* Success */
    EXIT_HARDFAIL,  /* None of the maildirs fixed */
    EXIT_SOFTFAIL,  /* Some of the maildirs failed to fix */ 
  };

static int verbose_option;
static int dry_run_option;
static unsigned long fail_count;
static unsigned long succ_count;

static struct mu_option options[] = 
{
  { "verbose", 'v', NULL, MU_OPTION_DEFAULT,
    "verbosely list what is being done",
    mu_c_incr, &verbose_option },
  { "dry-run", 'n', NULL, MU_OPTION_DEFAULT,
    "do nothing, print everything",
    mu_c_incr, &dry_run_option },  
    MU_OPTION_END
};

static int
mailbox_fixup (void *item, void *data)
{
  struct mu_list_response *resp = item;
  mu_mailbox_t mbox = NULL;
  int rc;

  if (!(resp->type & MU_FOLDER_ATTRIBUTE_FILE))
    return 0;
  
  if (verbose_option)
    mu_diag_output (MU_DIAG_INFO, "Fixing %s", resp->name);
  if (dry_run_option)
    return 0;
  
  rc = manlock_open_mailbox (&mbox, resp->name, 1, MU_STREAM_RDWR);
  if (rc == 0)
    {
      rc = mu_mailbox_scan (mbox, 1, NULL);
      mu_mailbox_close (mbox);
      manlock_unlock (mbox);
      mu_mailbox_destroy (&mbox);
    }
  else
    mu_error ("can't open mailbox: %s", mu_strerror (rc));
  
  if (rc)
    fail_count++;
  else
    succ_count++;
  return 0;
}

static inline int
filename_ok (char const *fname)
{
  return fname[0] == '/' ||
         fname[0] == '~' ||
         (fname[0] == '.' &&
	  (fname[1] == '/' || (fname[1] == '.' && fname[2] == '/')));
}

static int
fix_mailboxes_in_folder (char *fname)
{
  mu_folder_t folder;
  struct mu_folder_scanner scn = MU_FOLDER_SCANNER_INITIALIZER;
  int rc;
     
  if (!filename_ok (fname))
    {
      char *cwd = mu_getcwd ();
      size_t prefix_len = strlen (cwd);
      if (cwd[prefix_len-1] != '/')
	prefix_len++;
      fname = mu_make_file_name (cwd, fname);
      free (cwd);
      if (!fname)
	mu_alloc_die ();
    }
  
  rc = mu_folder_create (&folder, fname);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_folder_create", fname, rc);
      return rc;
    }
  rc = mu_folder_open (folder, MU_STREAM_READ);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_folder_open", fname, rc);
      goto err;
    }

  rc = mu_list_create (&scn.result);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_list_create", NULL, rc);
      goto err;
    }
  
  rc = mu_folder_scan (folder, &scn);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_folder_scan", NULL, rc);
  else
    mu_list_foreach (scn.result, mailbox_fixup, NULL);
  
 err:
  mu_list_destroy (&scn.result);
  mu_folder_destroy (&folder);
  return rc;
}

static char docstring[] = N_(
  "recursively scans all maildirs in the folder\n"
  "\nThis induces fixing of the maildir message attributes (bug #56428) "
  "and attaching persistent UID numbers (commits fd9b19bac-d7110faa).\n"
);
static char argdoc[] = N_("FOLDER ...");
static char *capa[] = {
  "debug",
  "locking",
  NULL
};

static struct mu_cfg_param config_param[] = {
  { "mandatory-locking", mu_cfg_section },
  { NULL }
};

int
main (int argc, char **argv)
{
  int i;

  mu_registrar_record (mu_maildir_record);
  mu_registrar_set_default_scheme ("maildir");
  manlock_cfg_init ();
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_OPTIONS, common_options,
		 MU_CLI_OPTION_PROG_NAME, getenv ("MAILUTILS_PROGNAME"),
		 MU_CLI_OPTION_PROG_DOC, docstring,
		 MU_CLI_OPTION_PROG_ARGS, argdoc,
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
                 MU_CLI_OPTION_RETURN_ARGV, &argv,
		 MU_CLI_OPTION_PACKAGE_NAME, PACKAGE_NAME,   
		 MU_CLI_OPTION_PACKAGE_URL, PACKAGE_URL,   
		 MU_CLI_OPTION_BUG_ADDRESS, PACKAGE_BUGREPORT,   
		 MU_CLI_OPTION_VERSION_HOOK, mu_version_hook,
		 MU_CLI_OPTION_CAPABILITIES, capa,
		 MU_CLI_OPTION_CONFIG, config_param,
		 MU_CLI_OPTION_CONF_SITE_FILE, NULL,
		 MU_CLI_OPTION_CONF_PROGNAME, "maildir_fixup",
		 MU_CLI_OPTION_END);
                 
  if (argc == 0)
    {
      mu_error ("required argument missing; try --help for more info");
      exit (EX_USAGE);
    }
  
  if (dry_run_option)
    verbose_option++;
  
  for (i = 0; i < argc; i++)
    fix_mailboxes_in_folder (argv[i]);

  if (fail_count)
    exit (succ_count == 0 ? EXIT_HARDFAIL : EXIT_SOFTFAIL);
  
  exit (EXIT_OK); 
}

