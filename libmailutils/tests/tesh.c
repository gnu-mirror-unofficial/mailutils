/* Test shell framework for GNU Mailutils.
   Copyright (C) 2003-2020 Free Software Foundation, Inc.

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

#include <config.h>
#include "mailutils/mailutils.h"
#include "tesh.h"

static struct mu_tesh_command *
find_command (struct mu_tesh_command *cmd, char const *name)
{
  while (cmd->verb)
    {
      if (strcmp (cmd->verb, name) == 0)
	return cmd;
      cmd++;
    }
  return NULL;
}

static int
cmdspecial (char const *special, struct mu_tesh_command *cmdtab,
	    int argc, char **argv, mu_assoc_t opt, void *env, int defval)
{
  struct mu_tesh_command *cmd = find_command (cmdtab, special);
  if (cmd)
    return cmd->func (argc, argv, opt, env);
  return defval;
}

static int
is_reserved (char const *str)
{
  return mu_string_prefix (str, "__") && mu_string_suffix (str, "__");
}

#define VARIADIC (-1)

static int
interpret (int argc, char **xargv, struct mu_tesh_command *cmdtab, void *env)
{
  int rc;
  struct mu_tesh_command *cmd;
  mu_assoc_t options = NULL;
  char **argv = xargv;

  if (strcmp (argv[0], "help") == 0)
    {
      mu_tesh_help (cmdtab, env);
      return 0;
    }

  if (is_reserved (argv[0]))
    cmd = NULL;
  else
    cmd = find_command (cmdtab, argv[0]);
  if (!cmd)
    {
      if (cmdspecial("__NOCMD__", cmdtab, argc, argv, NULL, env, MU_ERR_NOENT))
	{
	  mu_error ("%s: no such command", argv[0]);
	  return MU_ERR_NOENT;
	}
      return 0;
    }
  if (cmd->param_min == 0)
    {
      struct mu_wordsplit ws;

      /* See the description of the args field in tesh.h (Note [2]) */
      if (mu_wordsplit (cmd->args, &ws,
			MU_WRDSF_NOVAR
			| MU_WRDSF_NOCMD
			| MU_WRDSF_ALLOC_DIE
			| MU_WRDSF_SHOWERR))
	return MU_ERR_PARSE;

      if (ws.ws_wordc == 0)
	{
	  cmd->param_min = cmd->param_max = 1;
	  cmd->options = NULL;
	}
      else
	{
	  int i;
	  int variadic = 0;

	  cmd->param_min = cmd->param_max = 1;
	  for (i = 0; i < ws.ws_wordc; i++)
	    {
	      char *argdef = ws.ws_wordv[i];

	      if (mu_string_suffix (argdef, "..."))
		{
		  variadic = 1;
		  argdef[strlen (argdef) - 3] = 0;
		  if (argdef[0] == 0)
		    {
		      if (i != ws.ws_wordc - 1)
			{
			  mu_error ("%s: ellipsis must be last (found at #%d)",
				    cmd->args, i);
			  abort ();
			}
		      break;
		    }
		}

	      if (mu_string_prefix (argdef, "[-")
		  && mu_string_suffix (argdef, "]"))
		{
		  int *type;
		  char *p;

		  type = mu_alloc (sizeof (*type));

		  argdef += 2;
		  argdef[strlen (argdef) - 1] = 0;

		  if (!cmd->options)
		    {
		      MU_ASSERT (mu_assoc_create (&cmd->options, 0));
		    }

		  p = strchr (argdef, '=');
		  if (p)
		    {
		      *p = 0;
		      if (p[-1] == '[' && mu_string_suffix (argdef, "]"))
			{
			  *type = mu_tesh_arg_optional;
			  p[-1] = 0;
			}
		      else
			*type = mu_tesh_arg_required;
		    }
		  else
		    *type = mu_tesh_noarg;
		  MU_ASSERT (mu_assoc_install (cmd->options, argdef, type));
		}
	      else if (mu_string_prefix (argdef, "["))
		{
		  int j;
		  int lev = 0;
		  for (j = i; j < ws.ws_wordc; )
		    {
		      if (mu_string_prefix (ws.ws_wordv[j], "["))
			lev++;
		      if (mu_string_suffix (ws.ws_wordv[j], "]"))
			lev--;
		      j++;
		      if (lev == 0)
			break;
		    }
		  if (lev == 0)
		    {
		      cmd->param_max += j - i;
		      i = j;
		    }
		}
	      else
		{
		  cmd->param_min++;
		  cmd->param_max++;
		}
	    }

	  if (!variadic
	      && mu_string_prefix (ws.ws_wordv[ws.ws_wordc-1], "[")
	      && mu_string_suffix (ws.ws_wordv[ws.ws_wordc-1], "...]"))
	    {
	      variadic = 1;
	    }

	  if (variadic)
	    cmd->param_max = VARIADIC;
	  mu_wordsplit_free (&ws);
	}
    }

  if (cmd->options)
    {
      int i;

      /* Extract options */
      MU_ASSERT (mu_assoc_create (&options, 0));
      for (i = 1; i < argc; i++)
	{
	  if (strcmp (argv[i], "--") == 0)
	    {
	      i++;
	      break;
	    }

	  if (argv[i][0] == '-')
	    {
	      int *type;
	      char *opt = argv[i];
	      char *arg;

	      arg = strchr (opt, '=');
	      if (arg)
		*arg++ = 0;

	      rc = mu_assoc_lookup (cmd->options, opt + 1, &type);
	      if (rc == MU_ERR_NOENT)
		{
		  mu_error ("%s: no such option %s", argv[0], opt);
		  mu_assoc_destroy (&options);
		  return MU_ERR_NOENT;
		}

	      if (arg)
		{
		  if (*type == mu_tesh_noarg)
		    {
		      mu_error ("%s: option %s doesn't take argument",
				argv[0], opt);
		      mu_assoc_destroy (&options);
		      return MU_ERR_PARSE;
		    }
		}
	      else if (*type == mu_tesh_arg_required)
		{
		  if (i + 1 < argc)
		    arg = argv[++i];
		  else
		    {
		      mu_error ("%s: option %s requires argument",
				argv[0], opt);
		      mu_assoc_destroy (&options);
		      return MU_ERR_PARSE;
		    }
		}

	      if (arg)
		arg = mu_strdup (arg);
	      MU_ASSERT (mu_assoc_install (options, opt + 1, arg));
	    }
	  else
	    break;
	}

      if (i > 1)
	{
	  char *t;

	  --i;
	  t = argv[i];
	  argv[i] = argv[0];
	  argv[0] = t;
	  argc -= i;
	  argv += i;
	}
    }

  if (argc < cmd->param_min)
    {
      mu_error ("%s: not enough arguments", argv[0]);
      return MU_ERR_PARSE;
    }

  if (cmd->param_max != VARIADIC && argc > cmd->param_max)
    {
      mu_error ("%s: too many arguments", argv[0]);
      return MU_ERR_PARSE;
    }

  rc = cmdspecial ("__ENVINIT__", cmdtab, argc, argv, options, env, 0);
  if (rc == 0)
    {
      rc = cmd->func (argc, argv, options, env);
      cmdspecial ("__ENVFINI__", cmdtab, argc, argv, options, env, 0);
    }
  mu_assoc_destroy (&options);
  return rc;
}

static void
cleanup (struct mu_tesh_command *cmd)
{
  while (cmd->verb)
    {
      mu_assoc_destroy (&cmd->options);
      cmd++;
    }
}

void
mu_tesh_read_and_eval (int argc, char **argv,
		       struct mu_tesh_command *cmd,
		       void *env)
{
  if (argc)
    {
      while (argc)
	{
	  int i, n = 0;
	  for (i = 0; i < argc; i++)
	    {
	      size_t len = strlen (argv[i]);
	      if (argv[i][len - 1] == ';')
		{
		  if (len == 1)
		    n = 1;
		  else
		    argv[i][len - 1] = 0;
		  i++;
		  break;
		}
	    }

	  interpret (i - n, argv, cmd, env);
	  argc -= i;
	  argv += i;
	}
    }
  else
    {
      char *buf = NULL;
      size_t size = 0, n;
      struct mu_wordsplit ws;
      int wsflags;
      int rc;

      wsflags  = MU_WRDSF_DEFFLAGS
	       | MU_WRDSF_COMMENT
	       | MU_WRDSF_ALLOC_DIE
	       | MU_WRDSF_SHOWERR;
      ws.ws_comment = "#";

      while ((rc = mu_stream_getline (mu_strin, &buf, &size, &n)) == 0 && n > 0)
	{
	  char *larg[2];

	  mu_ltrim_class (buf, MU_CTYPE_SPACE);
	  mu_rtrim_class (buf, MU_CTYPE_SPACE);

	  larg[0] = buf;
	  larg[1] = NULL;
	  if (!cmdspecial("__LINEPROC__", cmd, 1, larg, NULL, env, MU_ERR_NOENT))
	    continue;

	  MU_ASSERT (mu_wordsplit (larg[0], &ws, wsflags));
	  wsflags |= MU_WRDSF_REUSE;

	  if (ws.ws_wordc == 0)
	    continue;
	  interpret (ws.ws_wordc, ws.ws_wordv, cmd, env);
	}
      if (wsflags & MU_WRDSF_REUSE)
	mu_wordsplit_free (&ws);
    }

  cleanup (cmd);
}

void
mu_tesh_init (char const *argv0)
{
  mu_set_program_name (argv0);
  mu_stdstream_setup (MU_STDSTREAM_RESET_NONE);
}

void
mu_tesh_help (struct mu_tesh_command *cmd, void *env)
{
  cmdspecial("__HELPINIT__", cmd, 0, NULL, NULL, env, 0);
  for (; cmd->verb; cmd++)
    if (!is_reserved (cmd->verb))
      mu_printf (" %s %s\n", cmd->verb, cmd->args);
  cmdspecial("__HELPFINI__", cmd, 0, NULL, NULL, env, 0);
}
