/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2007-2020 Free Software Foundation, Inc.

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

#include "libmda.h"

static void
set_stderr (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  mu_log_syslog = 0;
}

static struct mu_option mda_options[] = {
  MU_OPTION_GROUP (N_("General options")),
  { "stderr", 0, NULL, MU_OPTION_DEFAULT,
    N_("log to standard error"),
    mu_c_string, NULL, set_stderr },
  MU_OPTION_END
}, *options[] = { mda_options, NULL };

static char *capa[] = {
  "auth",
  "debug",
  "logging",
  "mailbox",
  "locking",
  "mailer",
  "sieve",
  "deliver", 
  "forward", 
  "quota",
  "script",
  NULL
};

static int
cb_stderr (void *data, mu_config_value_t *val)
{
  int res;
  
  if (mu_cfg_assert_value_type (val, MU_CFG_STRING))
    return 1;
  if (mu_str_to_c (val->v.string, mu_c_bool, &res, NULL))
    mu_error (_("not a boolean"));
  else
    mu_log_syslog = !res;
  return 0;
}

struct mu_cfg_param mda_cfg_param[] = {
  { "stderr", mu_cfg_callback, NULL, 0, cb_stderr,
    N_("Log to stderr instead of syslog."),
    N_("arg: bool") },
  { NULL }
};

struct mu_cli_setup cli = {
  options,
  mda_cfg_param,
  N_("mda -- the GNU local mail delivery agent."),
  N_("[recipient...]"),
};

static void
version_hook (struct mu_parseopt *po, mu_stream_t stream)
{
  mu_version_hook (po, stream);
#if defined(TESTSUITE_CONFIG_FILE)
  mu_stream_printf (stream, "%s\n",
		    _("THIS BINARY IS COMPILED ONLY FOR TESTING MAILUTILS."
		      "  DON'T USE IT IN PRODUCTION!"));
#endif
}

int
main (int argc, char **argv)
{
  struct mu_parseopt pohint;
  struct mu_cfg_parse_hints cfhint;

  umask (0077);

  /* Native Language Support */
  MU_APP_INIT_NLS ();

  /* Default locker settings */
  mu_locker_set_default_flags (MU_LOCKER_PID|MU_LOCKER_RETRY,
			       mu_locker_assign);
  mu_locker_set_default_retry_timeout (1);
  mu_locker_set_default_retry_count (300);

  /* Register needed modules */
  MU_AUTH_REGISTER_ALL_MODULES ();

  /* Register all supported mailbox and mailer formats */
  mu_register_all_formats ();
  mu_registrar_record (mu_smtp_record);

  mda_filter_cfg_init ();

  mu_log_syslog = 1;
  mu_log_print_severity = 1;

  /* Parse command line */
  mda_cli_capa_init ();

  pohint.po_flags = 0;
  
  pohint.po_package_name = PACKAGE_NAME;
  pohint.po_flags |= MU_PARSEOPT_PACKAGE_NAME;

  pohint.po_package_url = PACKAGE_URL;
  pohint.po_flags |= MU_PARSEOPT_PACKAGE_URL;

  pohint.po_bug_address = PACKAGE_BUGREPORT;
  pohint.po_flags |= MU_PARSEOPT_BUG_ADDRESS;

  pohint.po_extra_info = mu_general_help_text;
  pohint.po_flags |= MU_PARSEOPT_EXTRA_INFO;

  pohint.po_version_hook = version_hook;
  pohint.po_flags |= MU_PARSEOPT_VERSION_HOOK;

  pohint.po_negation = "no-";
  pohint.po_flags |= MU_PARSEOPT_NEGATION;
  
#if defined(TESTSUITE_CONFIG_FILE)
  /* This is for test version (see the tests directory) */
  cfhint.site_file = TESTSUITE_CONFIG_FILE;
  mu_log_syslog = 0;
#else
  cfhint.site_file = mu_site_config_file ();
#endif
  cfhint.flags = MU_CFHINT_SITE_FILE | MU_CFHINT_NO_CONFIG_OVERRIDE;
  
  mu_cli_ext (argc, argv, &cli, &pohint, &cfhint,
	      capa, NULL, &argc, &argv);
  if (argc == 0)
    {
      mu_error (_("recipients not given"));
      return EX_USAGE;
    }

  mu_stdstream_strerr_setup (mu_log_syslog ?
			     MU_STRERR_SYSLOG : MU_STRERR_STDERR);

  return mda_run_delivery (mda_deliver_to_user, argc, argv);
}
