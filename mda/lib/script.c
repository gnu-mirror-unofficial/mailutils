/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2020 Free Software Foundation, Inc.

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

static mu_script_t script_handler;
static mu_list_t script_list;
static char *message_id_header;

struct mda_script
{
  mu_script_t scr;   /* Handler script */
  const char *pat;   /* Script name pattern */
};


static int
script_register (const char *pattern)
{
  mu_script_t scr;
  struct mda_script *p;
  
  if (script_handler)
    scr = script_handler;
  else
    {
      scr = mu_script_suffix_handler (pattern);
      if (!scr)
	return EINVAL;
    }

  p = malloc (sizeof (*p));
  if (!p)
    return MU_ERR_FAILURE;
  
  p->scr = scr;
  p->pat = pattern;

  if (!script_list)
    {
      if (mu_list_create (&script_list))
	return MU_ERR_FAILURE;
    }

  if (mu_list_append (script_list, p))
    return MU_ERR_FAILURE;

  return 0;
}

static void
set_script_lang (struct mu_parseopt *po, struct mu_option *opt,
		 char const *arg)
{
  script_handler = mu_script_lang_handler (arg);
  if (!script_handler)
    {
      mu_parseopt_error (po, _("unknown or unsupported language: %s"), arg);
      exit (po->po_exit_error);
    }
}

static void
set_script_pattern (struct mu_parseopt *po, struct mu_option *opt,
		    char const *arg)
{
  switch (script_register (arg))
    {
    case 0:
      return;

    case EINVAL:
      mu_parseopt_error (po, _("%s has unknown file suffix"), arg);
      break;

    default:
      mu_parseopt_error (po, _("error registering script"));
    }
  exit (po->po_exit_error);
}

static void
set_debug (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  if (mu_script_debug_flags (arg, (char**)&arg))
    {
      mu_parseopt_error (po, _("%c is not a valid debug flag"), *arg);
      exit (po->po_exit_error);
    }
}

struct mu_option mda_script_options[] = {
  MU_OPTION_GROUP (N_("Scripting options")),
  { "language", 'l', N_("STRING"), MU_OPTION_DEFAULT,
    N_("define scripting language for the next --script option"),
    mu_c_string, NULL, set_script_lang },
  { "script", 's', N_("PATTERN"), MU_OPTION_DEFAULT,
    N_("set name pattern for user-defined mail filter"),
    mu_c_string, NULL, set_script_pattern },
  { "message-id-header", 0, N_("STRING"), MU_OPTION_DEFAULT,
    N_("use this header to identify messages when logging Sieve actions"),
    mu_c_string, &message_id_header },
  { "script-debug", 'x', N_("FLAGS"), MU_OPTION_DEFAULT,
    N_("enable script debugging; FLAGS are:\n\
g - guile stack traces\n\
t - sieve trace (MU_SIEVE_DEBUG_TRACE)\n\
i - sieve instructions trace (MU_SIEVE_DEBUG_INSTR)\n\
l - sieve action logs"),
    mu_c_string, NULL, set_debug },
  MU_OPTION_END
};

static int
cb_script_language (void *data, mu_config_value_t *val)
{
  if (mu_cfg_assert_value_type (val, MU_CFG_STRING))
    return 1;
  script_handler = mu_script_lang_handler (val->v.string);
  if (!script_handler)
    {
      mu_error (_("unsupported language: %s"), val->v.string);
      return 1;
    }
  return 0;
}

static int
cb_script_pattern (void *data, mu_config_value_t *val)
{
  if (mu_cfg_assert_value_type (val, MU_CFG_STRING))
    return 1;
  
  switch (script_register (val->v.string))
    {
    case 0:
      break;

    case EINVAL:
      mu_error (_("%s has unknown file suffix"), val->v.string);
      break;

    default:
      mu_error (_("error registering script"));
    }
  return 0;
}

static int
cb_debug (void *data, mu_config_value_t *val)
{
  char *p;
  
  if (mu_cfg_assert_value_type (val, MU_CFG_STRING))
    return 1;
  if (mu_script_debug_flags (val->v.string, &p))
    {
      mu_error (_("%c is not a valid debug flag"), *p);
      return 1;
    }
  return 0;
}

struct mu_cfg_param mda_script_cfg[] = {
  { "language", mu_cfg_callback, NULL, 0, cb_script_language,
    N_("Set script language."),
    /* TRANSLATORS: words to the right of : are keywords - do not translate */
    N_("arg: python|guile") },
  { "pattern", mu_cfg_callback, NULL, 0, cb_script_pattern,
    N_("Set script pattern."), 
    N_("arg: glob") },
  { "debug", mu_cfg_callback, NULL, 0, cb_debug,
    N_("Set scripting debug level.  Argument is one or more "
       "of the following letters:\n"
       "  g - guile stack traces\n"
       "  t - sieve trace (MU_SIEVE_DEBUG_TRACE)\n"
       "  i - sieve instructions trace (MU_SIEVE_DEBUG_INSTR)\n"
       "  l - sieve action logs\n"),
    N_("arg: string") },
  { "message-id-header", mu_c_string, &message_id_header, 0, NULL,
    N_("When logging Sieve actions, identify messages by the value of "
       "this header."),
    N_("name") },
  { NULL }
};

struct apply_script_closure
{
  struct mu_auth_data *auth;
  mu_message_t msg;
};

static char const *script_env[] = { "location=MDA", "phase=during", NULL };

static int
apply_script (void *item, void *data)
{
  struct mda_script *scr = item;
  struct apply_script_closure *clos = data;
  char *progfile;
  int rc;
  struct stat st;
  mu_script_descr_t sd;

  progfile = mu_expand_path_pattern (scr->pat, clos->auth->name);
  if (stat (progfile, &st))
    {
      if (errno != ENOENT)
	mu_diag_funcall (MU_DIAG_NOTICE, "stat", progfile, errno);
      free (progfile);
      return 0;
    }

  rc = mu_script_init (scr->scr, progfile, script_env, &sd);
  if (rc)
    mu_error (_("initialization of script %s failed: %s"),
	      progfile, mu_strerror (rc));
  else
    {
      if (mu_script_sieve_log)
	mu_script_log_enable (scr->scr, sd, clos->auth->name,
			      message_id_header);
      rc = mu_script_process_msg (scr->scr, sd, clos->msg);
      if (rc)
	mu_error (_("script %s failed: %s"), progfile, mu_strerror (rc));
      mu_script_done (scr->scr, sd);
    }

  free (progfile);

  return rc;
}
  
int
mda_filter_message (mu_message_t msg, struct mu_auth_data *auth)
{
  if (script_list)
    {
      mu_attribute_t attr;
      struct apply_script_closure clos;
      int rc;
      
      clos.auth = auth;
      clos.msg = msg;

      mu_message_get_attribute (msg, &attr);
      mu_attribute_unset_deleted (attr);
      if (mda_switch_user_id (auth, 1))
	return MDA_FILTER_FAILURE;

      chdir (auth->dir);
      rc = mu_list_foreach (script_list, apply_script, &clos);
      chdir ("/");

      if (mda_switch_user_id (auth, 0))
	return MDA_FILTER_FAILURE;
      
      if (rc == 0)
	{
	  mu_attribute_t attr;
	  mu_message_get_attribute (msg, &attr);
	  if (mu_attribute_is_deleted (attr))
	    return MDA_FILTER_FILTERED;
	}
      else
	return MDA_FILTER_FAILURE;
    }
  return MDA_FILTER_OK;
}

static struct mu_cfg_param filter_cfg_param[] = {
  { "language", mu_cfg_callback, NULL, 0, cb_script_language,
    N_("Set script language."),
    /* TRANSLATORS: words to the right of : are keywords - do not translate */
    N_("arg: sieve|python|scheme") },
  { "pattern", mu_cfg_callback, NULL, 0, cb_script_pattern,
    N_("Set script pattern."), 
    N_("arg: glob") },
  { NULL }
};

void
mda_filter_cfg_init (void)
{
  struct mu_cfg_section *section;
  if (mu_create_canned_section ("filter", &section) == 0)
    {
      section->docstring = N_("Add new message filter.");
      mu_cfg_section_add_params (section, filter_cfg_param);
    }
  mu_cli_capa_register (&mu_cli_capa_sieve);
}
