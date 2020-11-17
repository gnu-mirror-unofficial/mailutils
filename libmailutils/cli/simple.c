/* cli.c -- Command line interface for GNU Mailutils
   Copyright (C) 2016-2020 Free Software Foundation, Inc.

   GNU Mailutils is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3, or (at
   your option) any later version.

   GNU Mailutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <mailutils/cfg.h>
#include <mailutils/opt.h>
#include <mailutils/cli.h>
#include <mailutils/nls.h>

void
mu_cli_simple (int argc, char **argv, ...)
{
  struct mu_cli_setup setup;
  struct mu_parseopt pohint;
  struct mu_cfg_parse_hints cfhint;
  char **capa = NULL;
  void *data = NULL;
  int *ret_argc = NULL;
  char ***ret_argv = NULL;

  mu_opool_t args_pool = NULL, optv_pool = NULL;
  
  va_list ap;
  int opt;

  memset (&setup, 0, sizeof (setup));
  memset (&pohint, 0, sizeof (pohint));
  memset (&cfhint, 0, sizeof (cfhint));
  
  va_start (ap, argv);
  while ((opt = va_arg (ap, int)) != MU_CLI_OPTION_END)
    {
      switch (opt)
	{
	case MU_CLI_OPTION_OPTIONS:
	  {
	    struct mu_option *p;
	    if (!optv_pool)
	      mu_opool_create (&optv_pool, MU_OPOOL_ENOMEMABRT);	      
	    p = va_arg (ap, struct mu_option *);
	    mu_opool_append (optv_pool, &p, sizeof p);
	  }
	  break;
	  
	case MU_CLI_OPTION_CONFIG:
	  setup.cfg = va_arg (ap, struct mu_cfg_param *);
	  break;

	case MU_CLI_OPTION_CAPABILITIES:
	  capa = va_arg (ap, char **);
	  break;

	case MU_CLI_OPTION_EX_USAGE:
	  setup.ex_usage = va_arg (ap, int);
	  break;
	  
	case MU_CLI_OPTION_EX_CONFIG:
	  setup.ex_config = va_arg (ap, int);
	  break;
	  
	case MU_CLI_OPTION_DATA:
	  data = va_arg (ap, void *);
	  break;
	  
	case MU_CLI_OPTION_IN_ORDER:
	  setup.inorder = 1;
	  break;

	case MU_CLI_OPTION_RETURN_ARGC:
	  ret_argc = va_arg (ap, int *);
	  break;

	case MU_CLI_OPTION_RETURN_ARGV:
	  ret_argv = va_arg (ap, char ***);
	  break;
	  
	case MU_CLI_OPTION_IGNORE_ERRORS:
	  pohint.po_flags |= MU_PARSEOPT_IGNORE_ERRORS;
	  break;
	  
	case MU_CLI_OPTION_NO_STDOPT:      
	  pohint.po_flags |= MU_PARSEOPT_NO_STDOPT;
	  break;
	  
	case MU_CLI_OPTION_NO_ERREXIT:     
	  pohint.po_flags |= MU_PARSEOPT_NO_ERREXIT;
	  break;
	  
	case MU_CLI_OPTION_IMMEDIATE:      
	  pohint.po_flags |= MU_PARSEOPT_IMMEDIATE;
	  break;
	  
	case MU_CLI_OPTION_NO_SORT:        
	  pohint.po_flags |= MU_PARSEOPT_NO_SORT;
	  break;
	  
	case MU_CLI_OPTION_SINGLE_DASH:
	  pohint.po_flags |= MU_PARSEOPT_SINGLE_DASH;
	  break;
	  
	case MU_CLI_OPTION_PROG_NAME:      
	  pohint.po_prog_name = va_arg (ap, char *);
          if (pohint.po_prog_name)
	    pohint.po_flags |= MU_PARSEOPT_PROG_NAME;
	  break;

	case MU_CLI_OPTION_PROG_DOC:       
	  pohint.po_prog_doc = va_arg (ap, char *);
          if (pohint.po_prog_doc)
	    pohint.po_flags |= MU_PARSEOPT_PROG_DOC;
	  break;

	case MU_CLI_OPTION_PROG_ARGS:
	  {
	    char *p;
	    if (!args_pool)
	      mu_opool_create (&args_pool, MU_OPOOL_ENOMEMABRT);
	    p = va_arg (ap, char *);
	    mu_opool_append (args_pool, &p, sizeof p);
	  }
	  break;

	case MU_CLI_OPTION_BUG_ADDRESS:
	  pohint.po_bug_address = va_arg (ap, char *);
          if (pohint.po_bug_address)
	    pohint.po_flags |= MU_PARSEOPT_BUG_ADDRESS;
	  break;
	  
	case MU_CLI_OPTION_PACKAGE_NAME:
	  pohint.po_package_name = va_arg (ap, char *);
          if (pohint.po_package_name)
	    pohint.po_flags |= MU_PARSEOPT_PACKAGE_NAME;
	  break;
	  
	case MU_CLI_OPTION_PACKAGE_URL:
	  pohint.po_package_url = va_arg (ap, char *);
          if (pohint.po_package_url)
	    pohint.po_flags |= MU_PARSEOPT_PACKAGE_URL;
	  break;
	  
	case MU_CLI_OPTION_EXTRA_INFO:
	  pohint.po_extra_info = va_arg (ap, char *);
          if (pohint.po_extra_info)
	    pohint.po_flags |= MU_PARSEOPT_EXTRA_INFO;
	  break;
	  
	case MU_CLI_OPTION_HELP_HOOK:
	  pohint.po_help_hook =
	    va_arg (ap, void (*) (struct mu_parseopt *, mu_stream_t));
          if (pohint.po_help_hook)
	    pohint.po_flags |= MU_PARSEOPT_HELP_HOOK;
	  break;
	  
	case MU_CLI_OPTION_VERSION_HOOK:
	  pohint.po_version_hook =
	    va_arg (ap, void (*) (struct mu_parseopt *, mu_stream_t));
          if (pohint.po_version_hook)
	    pohint.po_flags |= MU_PARSEOPT_VERSION_HOOK;
	  break;
	  
	case MU_CLI_OPTION_PROG_DOC_HOOK:
	  pohint.po_prog_doc_hook =
	    va_arg (ap, void (*) (struct mu_parseopt *, mu_stream_t));
          if (pohint.po_prog_doc_hook)
	    pohint.po_flags |= MU_PARSEOPT_PROG_DOC_HOOK;
	  break;
	  
	case MU_CLI_OPTION_NEGATION:
	  pohint.po_negation = va_arg (ap, char *);
	  if (pohint.po_negation)
	    pohint.po_flags |= MU_PARSEOPT_NEGATION;
	  break;
	  
	case MU_CLI_OPTION_SPECIAL_ARGS:
	  pohint.po_special_args = va_arg (ap, char *);
          if (pohint.po_special_args)
	    pohint.po_flags |= MU_PARSEOPT_SPECIAL_ARGS;
	  break;

	case MU_CLI_OPTION_CONF_SITE_FILE:
	  if ((cfhint.site_file = va_arg (ap, char *)) == NULL)
	    cfhint.site_file = mu_site_config_file ();
	  cfhint.flags = MU_CFHINT_SITE_FILE;
	  break;
  
	case MU_CLI_OPTION_CONF_PER_USER_FILE:
	  cfhint.flags |= MU_CFHINT_PER_USER_FILE;
	  break;
	  
	case MU_CLI_OPTION_CONF_NO_OVERRIDE:
	  cfhint.flags |= MU_CFHINT_NO_CONFIG_OVERRIDE;
	  break;

	case MU_CLI_OPTION_CONF_PROGNAME:
	  if ((cfhint.program = va_arg (ap, char *)) != NULL)
	    cfhint.flags |= MU_CFHINT_PROGRAM;
	  break;

	default:
	  mu_diag_output (MU_DIAG_CRIT,
			  _("%s:%d: INTERNAL ERROR: unrecognized mu_cli_simple option"),
			  __FILE__, __LINE__);
	  abort ();
	}      
    }
  if (optv_pool)
    {
      struct mu_option *p = NULL;
      mu_opool_append (optv_pool, &p, sizeof p);
      setup.optv = mu_opool_finish (optv_pool, NULL);
    }
  if (args_pool)
    {
      char *p = NULL;
      mu_opool_append (args_pool, &p, sizeof p);
      pohint.po_prog_args = mu_opool_finish (args_pool, NULL);
      pohint.po_flags |= MU_PARSEOPT_PROG_ARGS;
    }
  mu_cli_ext (argc, argv, &setup, &pohint, &cfhint, capa, data,
	      ret_argc, ret_argv);
  mu_opool_destroy (&args_pool);
  mu_opool_destroy (&optv_pool);
  va_end (ap);
}
