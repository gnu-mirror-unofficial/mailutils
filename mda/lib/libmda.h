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

#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif

#include <sys/types.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>

#ifdef HAVE_SYSEXITS_H
# include <sysexits.h>
#else
# define EX_OK          0       /* successful termination */
# define EX__BASE       64      /* base value for error messages */
# define EX_USAGE       64      /* command line usage error */
# define EX_DATAERR     65      /* data format error */
# define EX_NOINPUT     66      /* cannot open input */
# define EX_NOUSER      67      /* addressee unknown */
# define EX_NOHOST      68      /* host name unknown */
# define EX_UNAVAILABLE 69      /* service unavailable */
# define EX_SOFTWARE    70      /* internal software error */
# define EX_OSERR       71      /* system error (e.g., can't fork) */
# define EX_OSFILE      72      /* critical OS file missing */
# define EX_CANTCREAT   73      /* can't create (user) output file */
# define EX_IOERR       74      /* input/output error */
# define EX_TEMPFAIL    75      /* temp failure; user is invited to retry */
# define EX_PROTOCOL    76      /* remote error in protocol */
# define EX_NOPERM      77      /* permission denied */
# define EX_CONFIG      78      /* configuration error */
# define EX__MAX        78      /* maximum listed value */
#endif

#include <mailutils/mailutils.h>
#include "muscript.h"

/* mailquota settings */
enum {
  MQUOTA_OK,
  MQUOTA_EXCEEDED,
  MQUOTA_UNLIMITED
};

#ifdef USE_MAILBOX_QUOTAS
# define EX_QUOTA ex_quota ()
#else
# define EX_QUOTA EX_UNAVAILABLE
#endif

int ex_quota (void);

extern struct mu_cfg_param mda_mailquota_cfg[];

/* .forward support */
enum mda_forward_result
  {
    mda_forward_none,
    mda_forward_ok,
    mda_forward_metoo,
    mda_forward_error
  };

enum mda_forward_result mda_forward (mu_message_t msg, struct mu_auth_data *auth);

extern struct mu_cfg_param mda_forward_cfg[];

enum {
  MDA_FILTER_OK,
  MDA_FILTER_FILTERED,
  MDA_FILTER_FAILURE
};

int mda_filter_message (mu_message_t msg, struct mu_auth_data *auth);

extern struct mu_option mda_script_options[];
extern struct mu_cfg_param mda_script_cfg[];

void mda_filter_cfg_init (void);


typedef int (*mda_delivery_fn) (mu_message_t, char *, char **);

int mda_run_delivery (mda_delivery_fn delivery_fun, int argc, char **argv);
int mda_deliver_to_url (mu_message_t msg, char *dest_id, char **errp);
int mda_deliver_to_user (mu_message_t msg, char *dest_id, char **errp);
int mda_check_quota (struct mu_auth_data *auth, mu_off_t size, mu_off_t *rest);

extern struct mu_cfg_param mda_deliver_cfg[];
extern struct mu_option mda_deliver_options[];

extern int exit_code;
void mda_close_fds (void);
int mda_switch_user_id (struct mu_auth_data *auth, int user);
void mda_error (const char *fmt, ...);

void mda_cli_capa_init (void);





