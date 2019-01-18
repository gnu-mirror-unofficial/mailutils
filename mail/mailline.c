/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2019 Free Software Foundation, Inc.

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

#include "mail.h"
#include <sys/stat.h>
#include <dirent.h>
#include <mailutils/folder.h>

#ifdef WITH_READLINE
static char **ml_command_completion (char *cmd, int start, int end);
static char *ml_command_generator (const char *text, int state);
#endif

static volatile int _interrupted;

static RETSIGTYPE
sig_handler (int signo)
{
  switch (signo)
    {
    case SIGINT:
      if (mailvar_is_true ("quit"))
	exit (0);
      _interrupted++;
      break;
#if defined (SIGWINCH)
    case SIGWINCH:
      util_do_command ("set screen=%d", util_getlines ());
      util_do_command ("set columns=%d", util_getcols ());
      page_invalidate (1);
      break;
#endif
    }
#ifndef HAVE_SIGACTION
  signal (signo, sig_handler);
#endif
}

void
ml_clear_interrupt (void)
{
  _interrupted = 0;
}

int
ml_got_interrupt (void)
{
  int rc = _interrupted;
  _interrupted = 0;
  return rc;
}

static int
ml_getc (FILE *stream)
{
  unsigned char c;

  while (1)
    {
      if (read (fileno (stream), &c, 1) == 1)
	return c;
      if (errno == EINTR)
	{
	  if (_interrupted)
	    break;
	  /* keep going if we handled the signal */
	}
      else
	break;
    }
  return EOF;
}

void
ml_readline_init (void)
{
  if (!interactive)
    return;

#ifdef WITH_READLINE
  rl_readline_name = "mail";
  rl_attempted_completion_function =
    (rl_completion_func_t*) ml_command_completion;
  rl_getc_function = ml_getc;
#endif
#ifdef HAVE_SIGACTION
  {
    struct sigaction act;
    act.sa_handler = sig_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGINT, &act, NULL);
#if defined(SIGWINCH)
    sigaction (SIGWINCH, &act, NULL);
#endif
  }
#else
  signal (SIGINT, sig_handler);
#if defined(SIGWINCH)
  signal (SIGWINCH, sig_handler);
#endif
#endif
}

char *
ml_readline_internal (void)
{
  char *buf = NULL;
  size_t size = 0, n;
  int rc;

  rc = mu_stream_getline (mu_strin, &buf, &size, &n);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_getline", NULL, rc);
      return NULL;
    }
  if (_interrupted)
    {
      free (buf);
      return NULL;
    }
  if (n == 0)
    return NULL;
  mu_rtrim_cset (buf, "\n");
  return buf;
}

char *
ml_readline (const char *prompt)
{
  if (interactive)
    return readline (prompt);
  return ml_readline_internal ();
}

char *
ml_readline_with_intr (const char *prompt)
{
  char *str = ml_readline (prompt);
  if (_interrupted)
    mu_printf ("\n");
  return str;
}

#ifdef WITH_READLINE

/* Flags for per-command completion functions */
enum
  {
    COMPL_DFL     = 0x0, /* Nothing special */
    COMPL_WS      = 0x1, /* Cursor pointing to a whitespace character
			    at the start of input or past another whitespace */
    COMPL_LASTARG = 0x2  /* Cursor pointing at or past the last argument */
  };

static char *insert_text;

static int
ml_insert_hook (void)
{
  if (insert_text)
    rl_insert_text (insert_text);
  return 0;
}

int
ml_reread (const char *prompt, char **text)
{
  char *s;

  ml_clear_interrupt ();
  insert_text = *text;
  rl_startup_hook = ml_insert_hook;
  s = readline ((char *)prompt);
  if (!ml_got_interrupt ())
    {
      if (*text)
	free (*text);
      *text = s;
    }
  else
    {
      putc('\n', stdout);
    }
  rl_startup_hook = NULL;
  return 0;
}

/*
 * readline tab completion
 */
char **
ml_command_completion (char *cmd, int start, int end)
{
  char **ret;
  char *p;
  struct mu_wordsplit ws;
  
  for (p = rl_line_buffer; p < rl_line_buffer + start && mu_isblank (*p); p++)
    ;

  if (mu_wordsplit_len (p, end, &ws, MU_WRDSF_DEFFLAGS))
    {
      mu_error (_("mu_wordsplit_len failed: %s"),
		mu_wordsplit_strerror (&ws));
      return NULL;
    }
  rl_completion_append_character = ' ';

  if (ws.ws_wordc == 0 ||
      (ws.ws_wordc == 1 && strlen (ws.ws_wordv[0]) <= end - start))
    {
      ret = rl_completion_matches (cmd, ml_command_generator);
      rl_attempted_completion_over = 1;
    }
  else
    {
      const struct mail_command_entry *entry =
	mail_find_command (ws.ws_wordv[0]);
      if (entry && entry->command_completion)
	{
	  int point = COMPL_DFL;

	  if (start == end)
	    point |= COMPL_WS;
	  if (mu_str_skip_class (p + end, MU_CTYPE_SPACE)[0] == 0)
	    point |= COMPL_LASTARG;
	  
	  ret = entry->command_completion (ws.ws_wordc, ws.ws_wordv, point);
	}
      else
	ret = NULL;
    }
  mu_wordsplit_free (&ws);
  return ret;
}

/*
 * more readline
 */
char *
ml_command_generator (const char *text, int state)
{
  static int i, len;
  const char *name;
  const struct mail_command *cp;

  if (!state)
    {
      i = 0;
      len = strlen (text);
    }

  while ((cp = mail_command_name (i)))
    {
      name = cp->longname;
      if (strlen (cp->shortname) > strlen (name))
	name = cp->shortname;
      i++;
      if (strncmp (name, text, len) == 0)
	return mu_strdup (name);
    }

  return NULL;
}

void
ml_set_completion_append_character (int c)
{
  rl_completion_append_character = c;
}

void
ml_attempted_completion_over (void)
{
  rl_attempted_completion_over = 1;
}


/* Concatenate results of two completions. Both A and B are expected to
   be returned by rl_completion_matches, i.e. their first entry is the
   longest common prefix of the remaining entries, which are sorted
   lexicographically. Array B is treated as case-insensitive.
   
   If either of the arrays is NULL, the other one is returned unchanged.

   Otherwise, if A[0] begins with a lowercase letter, all items from B
   will be converted to lowercase.

   Both A and B (but not their elements) are freed prior to returning.

   Note: This function assumes that no string from A is present in B and
   vice versa.
 */
static char **
compl_concat (char **a, char **b)
{
  size_t i, j, k, n = 0, an, bn;
  char **ret;
  int lwr = 0;
  
  if (a)
    {
      lwr = mu_islower (a[0][0]);
      for (an = 0; a[an]; an++)
	;
    }
  else
    return b;
  
  if (b)
    {
      for (bn = 0; b[bn]; bn++)
	{
	  if (lwr)
	    mu_strlower (b[bn]);
	}	  
    }
  else
    return a;

  i = an == 1 ? 0 : 1;
  j = bn == 1 ? 0 : 1;
  
  n = (an - i) + (bn - j) + 1;
  ret = mu_calloc (n + 1, sizeof (ret[0]));

  if (an == bn && an == 1)
    {
      /* Compute LCP of both entries */
      for (i = 0; a[i] && b[i] && a[i] == b[i]; i++)
	;
      ret[0] = mu_alloc (i + 1);
      memcpy (ret[0], a[0], i);
      ret[0][i] = 0;
    }
  else
    /* The first entry is the LCP of the rest. Select the shortest one. */
    ret[0] = mu_strdup ((strlen (a[0]) < strlen (b[0])) ? a[0] : b[0]);

  if (i)
    free (a[0]);
  if (j)
    free (b[0]);
  
  k = 1;
  while (k < n)
    {
      if (!a[i])
	{
	  memcpy (ret + k, b + j, sizeof (b[0]) * (bn - j));
	  break;
	}
      else if (!b[j])
	{
	  memcpy (ret + k, a + i, sizeof (a[0]) * (an - i));
	  break;
	}
      else
	ret[k++] = (strcmp (a[i], b[j]) < 0) ? a[i++] : b[j++];
    }
  ret[n] = NULL;
  free (a);
  free (b);
  return ret;
} 

static char *msgtype_generator (const char *text, int state);
static char *header_generator (const char *text, int state);

/* Internal completion generator for commands that take a message list
   (if MATCHES is NULL), or a messages list followed by another argument
   (if MATCHES is not NULL).

   In the latter case the MATCHES function generates expansions for the
   last argument. It is used for such commands as write, where the last
   object is a mailbox or pipe, where it is a command).
   
   CLOSURE supplies argument for MATCHES. It is ignored if MATCHES is NULL.
*/
static char **
msglist_closure_compl (int argc, char **argv, int point,
		       char **(*matches) (void*),
		       void *closure)
{
  char *text = (point & COMPL_WS) ? "" : argv[argc-1];
  size_t len = strlen (text);
  
  if (text[0] == ':')
    {
      if (text[1] && (text[1] == '/' || text[2]))
	{
	  ml_set_completion_append_character (0);
	  ml_attempted_completion_over ();
	  return NULL;
	}
      ml_set_completion_append_character (' ');
      return rl_completion_matches (text, msgtype_generator);
    }
  
  ml_set_completion_append_character (0);
  if (len && text[len-1] == ':')
    {
      char **ret = mu_calloc(2, sizeof (ret[0]));
      ret[0] = strcat (strcpy (mu_alloc (len + 2), text), "/");
      return ret;
    }

  if (matches && (point & COMPL_LASTARG))
    return compl_concat (matches (closure),
			 rl_completion_matches (text, header_generator));
  else
    return rl_completion_matches (text, header_generator);
}

/* Completion functions */
char **
no_compl (int argc MU_ARG_UNUSED, char **argv MU_ARG_UNUSED,
	  int flags MU_ARG_UNUSED)
{
  ml_attempted_completion_over ();
  return NULL;
}

char **
msglist_compl (int argc, char **argv, int point)
{
  return msglist_closure_compl (argc, argv, point, NULL, NULL);
}

char **
command_compl (int argc, char **argv, int point)
{
  ml_set_completion_append_character (0);
  if (point & COMPL_WS)
    return NULL;
  return rl_completion_matches (argv[argc-1], ml_command_generator);
}

/* Generate file list based on reference prefix TEXT, relative to PATH.
   Remove PATHLEN leading characters from the returned names. Replace
   them with REPL unless it is 0.

   Select only those files that match given FLAGS (MU_FOLDER_ATTRIBUTE_*
   constants).

   STATE is 0 for the first call, 1 otherwise.
 */
static char *
file_generator (const char *text, int state,
		char *path, size_t pathlen,
		char repl,
		int flags)
{
  static mu_list_t list;
  static mu_iterator_t itr;

  if (!state)
    {
      char *wcard;
      mu_folder_t folder;
      size_t count;

      wcard = mu_alloc (strlen (text) + 2);
      strcat (strcpy (wcard, text), "*");

      if (mu_folder_create (&folder, path))
	{
	  free (wcard);
	  return NULL;
	}

      mu_folder_list (folder, path, wcard, 1, &list);
      free (wcard);
      mu_folder_destroy (&folder);

      if (mu_list_count (list, &count) || count == 0)
	{
	  mu_list_destroy (&list);
	  return NULL;
	}
      else if (count == 1)
	ml_set_completion_append_character (0);

      if (mu_list_get_iterator (list, &itr))
	{
	  mu_list_destroy (&list);
	  return NULL;
	}
      mu_iterator_first (itr);
    }

  while (!mu_iterator_is_done (itr))
    {
      struct mu_list_response *resp;
      mu_iterator_current (itr, (void**)&resp);
      mu_iterator_next (itr);
      if (resp->type & flags)
	{
	  char *ret;
	  if (pathlen)
	    {
	      size_t len = strlen (resp->name + pathlen);
	      char *ptr;

	      ret = mu_alloc (len + (repl ? 1 : 0) + 1);
	      ptr = ret;
	      if (repl)
		*ptr++ = repl;
	      memcpy (ptr, resp->name + pathlen, len);
	      ptr[len] = 0;
	    }
	  else
	    ret = mu_strdup (resp->name);
	  return ret;
	}
    }
  mu_iterator_destroy (&itr);
  mu_list_destroy (&list);
  return NULL;
}

static char *
folder_generator (const char *text, int state)
{
  char *ret;
  static size_t pathlen;

  if (!state)
    {
      char *path = util_folder_path ("");
      if (!path)
	return NULL;

      pathlen = strlen (path);
      ret = file_generator (text, state, path, pathlen, '+',
			    MU_FOLDER_ATTRIBUTE_ALL);
      free (path);
    }
  else
    ret = file_generator (text, state, NULL, pathlen, '+',
			  MU_FOLDER_ATTRIBUTE_ALL);
  return ret;
}

static char *
msgtype_generator (const char *text, int state)
{
  /* Allowed message types, plus '/'. The latter can folow a colon,
     meaning body lookup */
  static char types[] = "dnorTtu/"; 
  static int i;
  char c;

  if (!state)
    {
      i = 0;
    }
  while ((c = types[i]))
    {
      i++;
      if (!text[1] || text[1] == c)
	{
	  char *s = mu_alloc (3);
	  s[0] = ':';
	  s[1] = c;
	  s[2] = 0;
	  return s;
	}
    }
  return NULL;
}

static char *
header_generator (const char *text, int state)
{
  static int i, len;
  char *hdr;
  char *hdrlist[] = {
    MU_HEADER_RETURN_PATH,
    MU_HEADER_RECEIVED,
    MU_HEADER_DATE,
    MU_HEADER_DCC,
    MU_HEADER_FROM,
    MU_HEADER_SENDER,
    MU_HEADER_RESENT_FROM,
    MU_HEADER_SUBJECT,
    MU_HEADER_RESENT_SENDER,
    MU_HEADER_TO,
    MU_HEADER_RESENT_TO,
    MU_HEADER_CC,
    MU_HEADER_RESENT_CC,
    MU_HEADER_BCC,
    MU_HEADER_RESENT_BCC,
    MU_HEADER_REPLY_TO,
    MU_HEADER_RESENT_REPLY_TO,
    MU_HEADER_MESSAGE_ID,
    MU_HEADER_RESENT_MESSAGE_ID,
    MU_HEADER_IN_REPLY_TO,
    MU_HEADER_REFERENCE,
    MU_HEADER_REFERENCES,
    MU_HEADER_ENCRYPTED,
    MU_HEADER_PRECEDENCE,
    MU_HEADER_STATUS,
    MU_HEADER_CONTENT_LENGTH,
    MU_HEADER_CONTENT_LANGUAGE,
    MU_HEADER_CONTENT_TRANSFER_ENCODING,
    MU_HEADER_CONTENT_ID,
    MU_HEADER_CONTENT_TYPE,
    MU_HEADER_CONTENT_DESCRIPTION,
    MU_HEADER_CONTENT_DISPOSITION,
    MU_HEADER_CONTENT_MD5,
    MU_HEADER_CONTENT_LOCATION,
    MU_HEADER_MIME_VERSION,
    MU_HEADER_X_MAILER,
    MU_HEADER_X_UIDL,
    MU_HEADER_X_UID,
    MU_HEADER_X_IMAPBASE,
    MU_HEADER_ENV_SENDER,
    MU_HEADER_ENV_DATE,
    MU_HEADER_FCC,
    MU_HEADER_DELIVERY_DATE,
    MU_HEADER_ENVELOPE_TO,
    MU_HEADER_X_EXPIRE_TIMESTAMP,
    NULL
  };

  if (!state)
    {
      i = 0;
      len = strlen (text);
    }

  while ((hdr = hdrlist[i]))
    {
      i++;
      if (mu_c_strncasecmp (hdr, text, len) == 0)
	return strcat (strcpy (mu_alloc (strlen (hdr) + 3), hdr), ":/");
    }

  return NULL;
}

char **
file_compl (int argc, char **argv, int point)
{
  char *text;
  
  if (point & COMPL_WS)
    {
      ml_set_completion_append_character (0);
      ml_attempted_completion_over ();
      return NULL;
    }

  text = argv[argc-1];

  switch (text[0])
    {
    case '+':
      return rl_completion_matches (text + 1, folder_generator);

    case '%':
    case '#':
    case '&':
      ml_attempted_completion_over ();
      break;
      
    default:
      /* Suppose it is a file name */
      return rl_completion_matches (text, rl_filename_completion_function);
    }
  
  return NULL;
}

struct compl_closure
{
  int argc;
  char **argv;
  int point;
};

static char **
file_compl_matches (void *closure)
{
  struct compl_closure *cp = closure;
  return file_compl (cp->argc, cp->argv, cp->point);
}

char **
msglist_file_compl (int argc, char **argv, int point)
{
  struct compl_closure clos = { argc, argv, point };
  return msglist_closure_compl (argc, argv, point, file_compl_matches, &clos);
}

static char *
dir_generator (const char *text, int state)
{
  char *ret;
  static size_t pathlen;
  static int repl;

  if (!state)
    {
      char *path;
      switch (text[0])
	{
	case '+':
	  text++;
	  repl = '+';
	  path = util_folder_path (text);
	  pathlen = strlen (path) - strlen (text);
	  break;

	case '~':
	  repl = '~';
	  if (text[1] == '/')
	    {
	      path = mu_get_homedir ();
	      text += 2;
	      pathlen = strlen (path);
	      break;
	    }
	  /* else FIXME! */

	case '/':
	  path = mu_strdup (text);
	  pathlen = 0;
	  repl = 0;
	  break;

	default:
	  path = mu_strdup ("./");
	  pathlen = 2;
	  repl = 0;
	}

      ret = file_generator (text, state, path, pathlen, repl,
			    MU_FOLDER_ATTRIBUTE_DIRECTORY);
      free (path);
    }
  else
    ret = file_generator (text, state, NULL, pathlen, repl,
			  MU_FOLDER_ATTRIBUTE_DIRECTORY);
  return ret;
}

char **
dir_compl (int argc, char **argv, int point)
{
  ml_attempted_completion_over ();
  if (point & COMPL_WS)
    {
      ml_set_completion_append_character (0);
      return NULL;
    }
  return rl_completion_matches (argv[argc-1], dir_generator);
}

static char *
alias_generator (const char *text, int state)
{
  static alias_iterator_t itr;
  const char *p;

  if (!state)
    p = alias_iterate_first (text, &itr);
  else
    p = alias_iterate_next (itr);

  if (!p)
    {
      alias_iterate_end (&itr);
      return NULL;
    }
  return mu_strdup (p);
}

char **
alias_compl (int argc, char **argv, int point)
{
  ml_attempted_completion_over ();
  return rl_completion_matches ((point & COMPL_WS) ? "" : argv[argc-1],
				alias_generator);
}

static char *
mkfilename (const char *dir, const char *file)
{
  size_t len = strlen (dir) + 1 + strlen (file) + 1;
  char *p = malloc (len);
  if (p)
    {
      strcpy (p, dir);
      strcat (p, "/");
      strcat (p, file);
    }
  return p;
}

static char *
exec_generator (const char *text, int state)
{
  static int prefix_len;
  static char *var;
  static char *dir;
  static size_t dsize;
  static DIR *dp;
  struct dirent *ent;

  if (!state)
    {
      var = getenv ("PATH");
      if (!var)
	return NULL;
      prefix_len = strlen (text);
      dsize = 0;
      dir = NULL;
      dp = NULL;
    }

  while (1)
    {
      if (!dp)
	{
	  char *p;
	  size_t len;

	  if (*var == 0)
	    break;
	  else
	    var++;

	  p = strchr (var, ':');
	  if (!p)
	    len = strlen (var) + 1;
	  else
	    len = p - var + 1;

	  if (dsize == 0)
	    {
	      dir = malloc (len);
	      dsize = len;
	    }
	  else if (len > dsize)
	    {
	      dir = realloc (dir, len);
	      dsize = len;
	    }

	  if (!dir)
	    return NULL;
	  memcpy (dir, var, len - 1);
	  dir[len - 1] = 0;
	  var += len - 1;

	  dp = opendir (dir);
	  if (!dp)
	    continue;
	}

      while ((ent = readdir (dp)))
	{
	  char *name = mkfilename (dir, ent->d_name);
	  if (name)
	    {
	      int rc = access (name, X_OK);
	      if (rc == 0)
		{
		  struct stat st;
		  rc = !(stat (name, &st) == 0 && S_ISREG (st.st_mode));
		}

	      free (name);
	      if (rc == 0
		  && strlen (ent->d_name) >= prefix_len
		  && strncmp (ent->d_name, text, prefix_len) == 0)
		return mu_strdup (ent->d_name);
	    }
	}

      closedir (dp);
      dp = NULL;
    }

  free (dir);
  return NULL;
}

static char **
exec_matches (void *closure)
{
  return rl_completion_matches ((char *)closure, exec_generator);
}

char **
exec_compl (int argc, char **argv, int point)
{
  return msglist_closure_compl (argc, argv, point,
				exec_matches,
				(point & COMPL_WS) ? "" : argv[argc-1]);
}

char **
shell_compl (int argc, char **argv, int point)
{
  if (argc == 2)
    return rl_completion_matches (argv[1], exec_generator);
  return no_compl (argc, argv, point);
}

#else

#include <sys/ioctl.h>

#define STDOUT 1
#ifndef TIOCSTI
static int ch_erase;
static int ch_kill;
#endif

#if defined(TIOCSTI)
#elif defined(HAVE_TERMIOS_H)
# include <termios.h>

static struct termios term_settings;

int
set_tty (void)
{
  struct termios new_settings;

  if (tcgetattr(STDOUT, &term_settings) == -1)
    return 1;

  ch_erase = term_settings.c_cc[VERASE];
  ch_kill = term_settings.c_cc[VKILL];

  new_settings = term_settings;
  new_settings.c_lflag &= ~(ICANON|ECHO);
#if defined(TAB3)
  new_settings.c_oflag &= ~(TAB3);
#elif defined(OXTABS)
  new_settings.c_oflag &= ~(OXTABS);
#endif
  new_settings.c_cc[VMIN] = 1;
  new_settings.c_cc[VTIME] = 0;
  tcsetattr(STDOUT, TCSADRAIN, &new_settings);
  return 0;
}

void
restore_tty (void)
{
  tcsetattr (STDOUT, TCSADRAIN, &term_settings);
}

#elif defined(HAVE_TERMIO_H)
# include <termio.h>

static struct termio term_settings;

int
set_tty (void)
{
  struct termio new_settings;

  if (ioctl(STDOUT, TCGETA, &term_settings) < 0)
    return -1;

  ch_erase = term_settings.c_cc[VERASE];
  ch_kill = term_settings.c_cc[VKILL];

  new_settings = term_settings;
  new_settings.c_lflag &= ~(ICANON | ECHO);
  new_settings.c_oflag &= ~(TAB3);
  new_settings.c_cc[VMIN] = 1;
  new_settings.c_cc[VTIME] = 0;
  ioctl(STDOUT, TCSETA, &new_settings);
  return 0;
}

void
restore_tty (void)
{
  ioctl(STDOUT, TCSETA, &term_settings);
}

#elif defined(HAVE_SGTTY_H)
# include <sgtty.h>

static struct sgttyb term_settings;

int
set_tty (void)
{
  struct sgttyb new_settings;

  if (ioctl(STDOUT, TIOCGETP, &term_settings) < 0)
    return 1;

  ch_erase = term_settings.sg_erase;
  ch_kill = term_settings.sg_kill;

  new_settings = term_settings;
  new_settings.sg_flags |= CBREAK;
  new_settings.sg_flags &= ~(ECHO | XTABS);
  ioctl(STDOUT, TIOCSETP, &new_settings);
  return 0;
}

void
restore_tty (void)
{
  ioctl(STDOUT, TIOCSETP, &term_settings);
}

#else
# define DUMB_MODE
#endif


#define LINE_INC 80

int
ml_reread (const char *prompt, char **text)
{
  int ch;
  char *line;
  int line_size;
  int pos;
  char *p;

  if (*text)
    {
      line = strdup (*text);
      if (line)
	{
	  pos = strlen (line);
	  line_size = pos + 1;
	}
    }
  else
    {
      line_size = LINE_INC;
      line = malloc (line_size);
      pos = 0;
    }

  if (!line)
    {
      mu_error (_("Not enough memory to edit the line"));
      return -1;
    }

  line[pos] = 0;

  if (prompt)
    {
      fputs (prompt, stdout);
      fflush (stdout);
    }

#ifdef TIOCSTI

  for (p = line; *p; p++)
    {
      ioctl(0, TIOCSTI, p);
    }

  pos = 0;

  while ((ch = ml_getc (stdin)) != EOF && ch != '\n')
    {
      if (pos >= line_size)
	{
	  if ((p = realloc (line, line_size + LINE_INC)) == NULL)
	    {
	      fputs ("\n", stdout);
	      mu_error (_("Not enough memory to edit the line"));
	      break;
	    }
	  else
	    {
	      line_size += LINE_INC;
	      line = p;
	    }
	}
      line[pos++] = ch;
    }

#else

  fputs (line, stdout);
  fflush (stdout);

# ifndef DUMB_MODE
  set_tty ();

  while ((ch = ml_getc (stdin)) != EOF)
    {
      if (ch == ch_erase)
	{
	  /* kill last char */
	  if (pos > 0)
	    line[--pos] = 0;
	  putc('\b', stdout);
	}
      else if (ch == ch_kill)
	{
	  /* kill the entire line */
	  pos = 0;
	  line[pos] = 0;
	  putc ('\r', stdout);
	  if (prompt)
	    fputs (prompt, stdout);
	}
      else if (ch == '\n' || ch == EOF)
	break;
      else
	{
	  if (pos >= line_size)
	    {
	      if ((p = realloc (line, line_size + LINE_INC)) == NULL)
		{
		  fputs ("\n", stdout);
		  mu_error (_("Not enough memory to edit the line"));
		  break;
		}
	      else
		{
		  line_size += LINE_INC;
		  line = p;
		}
	    }
	  line[pos++] = ch;
	  putc(ch, stdout);
	}
      fflush (stdout);
    }

  putc ('\n', stdout);
  restore_tty ();
# else
  /* Dumb mode: the last resort */

  putc ('\n', stdout);
  if (prompt)
    {
      fputs (prompt, stdout);
      fflush (stdout);
    }

  pos = 0;

  while ((ch = ml_getc (stdin)) != EOF && ch != '\n')
    {
      if (pos >= line_size)
	{
	  if ((p = realloc (line, line_size + LINE_INC)) == NULL)
	    {
	      fputs ("\n", stdout);
	      mu_error (_("Not enough memory to edit the line"));
	      break;
	    }
	  else
	    {
	      line_size += LINE_INC;
	      line = p;
	    }
	}
      line[pos++] = ch;
    }
# endif
#endif

  line[pos] = 0;

  if (ml_got_interrupt ())
    free (line);
  else
    {
      if (*text)
	free (*text);
      *text = line;
    }
  return 0;
}

char *
readline (char *prompt)
{
  if (prompt)
    {
      mu_printf ("%s", prompt);
      mu_stream_flush (mu_strout);
    }

  return ml_readline_internal ();
}

void
ml_set_completion_append_character (int c MU_ARG_UNUSED)
{
}

void
ml_attempted_completion_over ()
{
}

#endif
