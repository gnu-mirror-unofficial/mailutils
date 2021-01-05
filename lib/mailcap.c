/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2005-2021 Free Software Foundation, Inc.

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
#include <mailutils/mailutils.h>
#include <sys/wait.h>

/* Default mailcap path, the $HOME/.mailcap: entry is prepended to it */
#define DEFAULT_MAILCAP \
 "/usr/local/etc/mailcap:"\
 "/usr/etc/mailcap:"\
 "/etc/mailcap:"\
 "/etc/mail/mailcap:"\
 "/usr/public/lib/mailcap"

#define FLAGS_DRY_RUN      0x0001
#define FLAGS_INTERACTIVE  0x0002

struct mime_context
{
  mu_stream_t input;
  mu_header_t hdr;
  mu_content_type_t content_type;

  char *temp_file;
  int unlink_temp_file;

  char *no_ask_types;
  int dh;
  int flags;
};

static int
mime_context_fill (struct mime_context *ctx, const char *file,
		   mu_stream_t input, mu_header_t hdr, const char *no_ask,
		   int interactive, int dry_run, mu_debug_handle_t dh)
{
  int rc;
  char *buffer;

  memset (ctx, 0, sizeof *ctx);
  ctx->input = input;
  ctx->hdr = hdr;

  rc = mu_header_aget_value_unfold (hdr, MU_HEADER_CONTENT_TYPE, &buffer);
  if (rc)
    return 1;
  rc = mu_content_type_parse (buffer, NULL, &ctx->content_type);
  free (buffer);
  if (rc)
    return 1;

  ctx->temp_file = file ? mu_strdup (file) : NULL;
  ctx->unlink_temp_file = 0;

  if (interactive)
    ctx->flags |= FLAGS_INTERACTIVE;
  if (dry_run)
    ctx->flags |= FLAGS_DRY_RUN;
  ctx->dh = dh;

  ctx->no_ask_types = no_ask ? mu_strdup (no_ask) : NULL;

  return 0;
}

static void
mime_context_release (struct mime_context *ctx)
{
  mu_content_type_destroy (&ctx->content_type);
  if (ctx->unlink_temp_file)
    unlink (ctx->temp_file);
  free (ctx->temp_file);
  free (ctx->no_ask_types);
}

static int
dry_run_p (struct mime_context *ctx)
{
  return ctx->flags & FLAGS_DRY_RUN;
}

static int
interactive_p (struct mime_context *ctx)
{
  return ctx->flags & FLAGS_INTERACTIVE;
}

static void
mime_context_get_input (struct mime_context *ctx, mu_stream_t *pinput)
{
  *pinput = ctx->input;
}

/* FIXME: Rewrite via mu_stream_copy */
static void
mime_context_write_input (struct mime_context *ctx, int fd)
{
  mu_stream_t input;
  char buf[512];
  size_t n;
  int status;

  mime_context_get_input (ctx, &input);
  status = mu_stream_seek (input, 0, SEEK_SET, NULL);
  if (status && status != ENOSYS)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_seek", NULL, status);
      abort (); /* FIXME */
    }
  while ((status = mu_stream_read (input, buf, sizeof buf, &n)) == 0
	 && n)
    write (fd, buf, n);
}

static int
mime_context_get_temp_file (struct mime_context *ctx, char **ptr)
{
  if (!ctx->temp_file)
    {
      int fd;
      if (mu_tempfile (NULL, 0, &fd, &ctx->temp_file))
	return -1;
      mime_context_write_input (ctx, fd);
      close (fd);
      ctx->unlink_temp_file = 1;
    }
  *ptr = ctx->temp_file;
  return 0;
}


static mu_opool_t expand_pool;

static int
expand_string (struct mime_context *ct, char const *input, char **pstr)
{
  char const *p;
  char *s;
  int rc = 0;

  for (p = input; *p; )
    {
      switch (p[0])
	{
	  case '%':
	    switch (p[1])
	      {
	      case 's':
		mime_context_get_temp_file (ct, &s);
		mu_opool_appendz (expand_pool, s);
		rc = 1;
		p += 2;
		break;

	      case 't':
		mu_opool_appendz (expand_pool, ct->content_type->type);
		mu_opool_append_char (expand_pool, '/');
		mu_opool_appendz (expand_pool, ct->content_type->subtype);
		p += 2;
		break;

	      case '{':
		{
		  size_t n;
		  char const *q;
		  char *namebuf;
		  struct mu_mime_param *param;

		  p += 2;
		  q = p;
		  while (*p && *p != '}')
		    p++;
		  n = p - q;
		  namebuf = mu_alloc (n + 1);
		  memcpy (namebuf, q, n);
		  namebuf[n] = 0;
		  param = mu_assoc_get (ct->content_type->param, namebuf);
		  if (param)
		    /* FIXME: cset? */
		    mu_opool_appendz (expand_pool, param->value);
		  free (namebuf);
		  if (*p)
		    p++;
		  break;
		}

	      case 'F':
	      case 'n':
		p++;
		break;

	      default:
		mu_opool_append_char (expand_pool, p[0]);
	      }
	    break;

	case '\\':
	  if (p[1])
	    {
	      mu_opool_append_char (expand_pool, p[1]);
	      p += 2;
	    }
	  else
	    {
	      mu_opool_append_char (expand_pool, p[0]);
	      p++;
	    }
	  break;

	case '"':
	  if (p[1] == p[0])
	    {
	      mu_opool_append_char (expand_pool, '%');
	      p++;
	    }
	  else
	    {
	      mu_opool_append_char (expand_pool, p[0]);
	      p++;
	    }
	  break;

	default:
	  mu_opool_append_char (expand_pool, p[0]);
	  p++;
	}
    }
  mu_opool_append_char (expand_pool, 0);
  *pstr = mu_opool_finish (expand_pool, NULL);
  return rc;
}

static int
confirm_action (struct mime_context *ctx, const char *str)
{
  char repl[128], *p;
  int len;

  if (!interactive_p (ctx)
      || mu_mailcap_content_type_match (ctx->no_ask_types, ',',
					ctx->content_type) == 0)
    return 1;

  printf (_("Run `%s'?"), str);
  fflush (stdout);

  p = fgets (repl, sizeof repl, stdin);
  if (!p)
    return 0;
  len = strlen (p);
  if (len > 0 && p[len-1] == '\n')
    p[len--] = 0;

  return mu_true_answer_p (p);
}

struct list_closure
{
  unsigned long n;
};

static int
list_field (char const *name, char const *value, void *data)
{
  struct list_closure *fc = data;
  printf ("\tfields[%lu]: ", fc->n++);
  if (value)
    printf ("%s=%s", name, value);
  else
    printf ("%s", name);
  printf ("\n");
  return 0;
}

static void
dump_mailcap_entry (mu_mailcap_entry_t entry)
{
  char const *value;
  struct list_closure lc;

  mu_mailcap_entry_sget_type (entry, &value);
  printf ("typefield: %s\n", value);

  /* view-command.  */
  mu_mailcap_entry_sget_command (entry, &value);
  printf ("view-command: %s\n", value);

  /* fields.  */
  lc.n = 1;
  mu_mailcap_entry_fields_foreach (entry, list_field, &lc);
  printf ("\n");
}

/* Return 1 if CMD needs to be executed via sh -c */
static int
need_shell_p (const char *cmd)
{
  for (; *cmd; cmd++)
    if (strchr ("<>|&", *cmd))
      return 1;
  return 0;
}

static pid_t
create_filter (char *cmd, int outfd, int *infd)
{
  pid_t pid;
  int lp[2];

  if (infd)
    pipe (lp);

  pid = fork ();
  if (pid == -1)
    {
      if (infd)
	{
	  close (lp[0]);
	  close (lp[1]);
	}
      mu_error ("fork: %s", mu_strerror (errno));
      return -1;
    }

  if (pid == 0)
    {
      /* Child process */
      struct mu_wordsplit ws;
      char **argv;

      if (need_shell_p (cmd))
	{
	  char *x_argv[4];
	  argv = x_argv;
	  argv[0] = getenv ("SHELL");
	  argv[1] = "-c";
	  argv[2] = cmd;
	  argv[3] = NULL;
	}
      else
	{
	  if (mu_wordsplit (cmd, &ws, MU_WRDSF_DEFFLAGS))
	    {
	      mu_error (_("%s failed: %s"), "mu_wordsplit",
			mu_wordsplit_strerror (&ws));
	      _exit (127);
	    }
	  argv = ws.ws_wordv;
	}
      /* Create input channel: */
      if (infd)
	{
	  if (lp[0] != 0)
	    dup2 (lp[0], 0);
	  close (lp[1]);
	}

      /* Create output channel */
      if (outfd != -1 && outfd != 1)
	dup2 (outfd, 1);

      execvp (argv[0], argv);
      mu_error (_("cannot execute `%s': %s"), cmd, mu_strerror (errno));
      _exit (127);
    }

  /* Master process */
  if (infd)
    {
      *infd = lp[1];
      close (lp[0]);
    }
  return pid;
}

static void
print_exit_status (int status)
{
  if (WIFEXITED (status))
    printf (_("Command exited with status %d\n"), WEXITSTATUS(status));
  else if (WIFSIGNALED (status))
    printf(_("Command terminated on signal %d\n"), WTERMSIG(status));
  else
    printf (_("Command terminated\n"));
}

static char *
get_pager ()
{
  char *pager = getenv ("MIMEVIEW_PAGER");
  if (!pager)
    {
      pager = getenv ("METAMAIL_PAGER");
      if (!pager)
	{
	  pager = getenv ("PAGER");
	  if (!pager)
	    pager = "more";
	}
    }
  return pager;
}

static int
run_test (mu_mailcap_entry_t entry, struct mime_context *ctx)
{
  int status = 0;
  char const *value;

  if (mu_mailcap_entry_sget_field (entry, MU_MAILCAP_TEST, &value) == 0)
    {
      char *str;
      char *argv[] = { "/bin/sh", "-c", NULL, NULL };
      expand_string (ctx, value, &str);
      argv[2] = str;
      if (mu_spawnvp (argv[0], argv, &status))
	status = 1;
    }
  return status;
}

static int
run_mailcap (mu_mailcap_entry_t entry, struct mime_context *ctx)
{
  char const *view_command;
  char *command;
  int status;
  int fd;
  int *pfd = NULL;
  int outfd = -1;
  pid_t pid;
  struct mu_locus_range lr = MU_LOCUS_RANGE_INITIALIZER;

  if (mu_mailcap_entry_get_locus (entry, &lr) == 0)
    {
      mu_stream_lprintf (mu_strout, &lr, "trying entry\n");
      mu_locus_range_deinit (&lr);
    }
  if (mu_debug_level_p (ctx->dh, MU_DEBUG_TRACE2))
    dump_mailcap_entry (entry);

  if (run_test (entry, ctx))
    return -1;

  if (interactive_p (ctx))
    status = mu_mailcap_entry_sget_command (entry, &view_command);
  else
    status = mu_mailcap_entry_sget_field (entry, MU_MAILCAP_PRINT, &view_command);

  if (status)
    return 1;

  /* NOTE: We don't create temporary file for %s, we just use
     mimeview_file instead */
  if (expand_string (ctx, view_command, &command))
    pfd = NULL;
  else
    pfd = &fd;
  mu_debug (ctx->dh, MU_DEBUG_TRACE0, (_("executing %s...\n"), command));

  if (!confirm_action (ctx, command))
    return 1;
  if (dry_run_p (ctx))
    return 0;

  if (interactive_p (ctx)
      && mu_mailcap_entry_sget_field (entry, MU_MAILCAP_COPIOUSOUTPUT, NULL) == 0)
    create_filter (get_pager (), -1, &outfd);

  pid = create_filter (command, outfd, pfd);
  if (pid > 0)
    {
      if (pfd)
	{
	  mime_context_write_input (ctx, fd);
	  close (fd);
	}

      while (waitpid (pid, &status, 0) < 0)
	if (errno != EINTR)
	  {
	    mu_error ("waitpid: %s", mu_strerror (errno));
	    break;
	  }
      if (mu_debug_level_p (ctx->dh, MU_DEBUG_TRACE0))
	print_exit_status (status);
    }
  return 0;
}

static int
entry_selector (mu_mailcap_entry_t entry, void *data)
{
  struct mime_context *ctx = data;
  char const *pattern;

  if (mu_mailcap_entry_sget_type (entry, &pattern))
    return 1;
  return mu_mailcap_content_type_match (pattern, 0, ctx->content_type);
}

int
display_stream_mailcap (const char *ident, mu_stream_t stream, mu_header_t hdr,
			const char *no_ask, int interactive, int dry_run,
			mu_debug_handle_t dh)
{
  char *mailcap_path, *mailcap_path_tmp = NULL;
  struct mu_wordsplit ws;
  struct mime_context ctx;
  int rc = 1;

  if (mime_context_fill (&ctx, ident, stream, hdr,
			 no_ask, interactive, dry_run, dh))
    return 1;
  mailcap_path = getenv ("MAILCAP");
  if (!mailcap_path)
    {
      char *home = mu_get_homedir ();
      mailcap_path_tmp = mu_make_file_name_suf (home, ".mailcap:",
						DEFAULT_MAILCAP);
      free (home);
      if (!mailcap_path_tmp)
	return 1;
      mailcap_path = mailcap_path_tmp;
    }

  mu_opool_create (&expand_pool, MU_OPOOL_ENOMEMABRT);

  ws.ws_delim = ":";
  if (mu_wordsplit (mailcap_path, &ws,
		    MU_WRDSF_DELIM|MU_WRDSF_SQUEEZE_DELIMS|
		    MU_WRDSF_NOVAR|MU_WRDSF_NOCMD))
    {
      mu_error (_("cannot split line `%s': %s"), mailcap_path,
		mu_wordsplit_strerror (&ws));
    }
  else
    {
      mu_mailcap_finder_t finder;
      int flags = MU_MAILCAP_FLAG_DEFAULT;
      struct mu_mailcap_error_closure *errcp = NULL;
      struct mu_mailcap_selector_closure selcl;
      mu_mailcap_entry_t entry;
      int rc;

      if (mu_debug_level_p (ctx.dh, MU_DEBUG_TRACE1)
	  || mu_debug_level_p (ctx.dh, MU_DEBUG_TRACE2))
	flags |= MU_MAILCAP_FLAG_LOCUS;
      if (mu_debug_level_p (ctx.dh, MU_DEBUG_ERROR))
	errcp = &mu_mailcap_default_error_closure;

      memset (&selcl, 0, sizeof (selcl));
      selcl.selector = entry_selector;
      selcl.data = &ctx;

      rc = mu_mailcap_finder_create (&finder, flags,
				     &selcl, errcp, ws.ws_wordv);
      mu_wordsplit_free (&ws);

      while ((rc = mu_mailcap_finder_next_match (finder, &entry)) == 0)
	{
	  if (run_mailcap (entry, &ctx) == 0)
	    break;
	}
      mu_mailcap_finder_destroy (&finder);
    }
  mu_opool_destroy (&expand_pool);
  free (mailcap_path_tmp);
  mime_context_release (&ctx);
  return rc;
}
