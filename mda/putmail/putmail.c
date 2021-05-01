/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2007-2021 Free Software Foundation, Inc.

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

static char *capa[] = {
  "auth",
  "debug",
  "mailbox",
  "locking",
  "mailer",
  "sieve",
  "deliver", 
  "script",
  NULL
};

struct mu_cli_setup cli = {
  NULL,
  NULL,
  N_("putmail -- incorporates mail to a mailbox."),
  N_("[URL...]"),
};

int
main (int argc, char **argv)
{
  umask (0077);

  /* Native Language Support */
  MU_APP_INIT_NLS ();

  /* Default locker settings */
  mu_locker_defaults.flags = MU_LOCKER_FLAG_CHECK_PID | MU_LOCKER_FLAG_RETRY;
  mu_locker_defaults.retry_sleep = 1;
  mu_locker_defaults.retry_count = 300;

  /* Register needed modules */
  MU_AUTH_REGISTER_ALL_MODULES ();

  /* Register all supported mailbox and mailer formats */
  mu_register_all_formats ();
  mu_registrar_record (mu_smtp_record);

  mda_filter_cfg_init ();

  mu_log_syslog = 0;
  mu_log_print_severity = 1;

  /* Parse command line */
  mda_cli_capa_init ();
  mu_cli (argc, argv, &cli, capa, NULL, &argc, &argv);
  if (argc == 0)
    {
      mu_error (_("recipients not given"));
      return EX_USAGE;
    }

  return mda_run_delivery (mda_deliver_to_url, argc, argv);
}
  
	
