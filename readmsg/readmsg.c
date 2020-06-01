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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "readmsg.h"
#include <fnmatch.h>
#include <regex.h>
#include "mailutils/cli.h"
#include "mailutils/mu_auth.h"
#include "mailutils/alloc.h"
#include "mu_umaxtostr.h"
#include "muaux.h"

#define WEEDLIST_SEPARATOR " :,"

static void print_unix_header (mu_message_t);
static void print_header (mu_message_t, int, int, char **);
static void print_body (mu_message_t);
static int  string_starts_with (const char * s1, const char *s2);

int dbug = 0;
const char *mailbox_name = NULL;
const char *weedlist = NULL;
int no_header = 0;
int all_header = 0;
int form_feed = 0;
int show_all = 0;
int mime_decode = 0;
char *charset;


enum
  {
    PAT_EXACT,
    PAT_GLOB,
    PAT_REGEX
  };

int pattern_type = PAT_EXACT;
int pattern_ci = 0;

static void *
generic_init (char const *pattern)
{
  char *res;
  if (pattern_ci)
    unistr_downcase (pattern, &res);
  else
    res = mu_strdup (pattern);
  return res;
}

static void
generic_free (void *p)
{
  free (p);
}

static int
pat_exact_match (void *pat, char const *text)
{
  return (pattern_ci ? unistr_is_substring_dn : unistr_is_substring)
            (text, pat);
}
	  
static int
pat_glob_match (void *pat, char const *text)
{
  return fnmatch (pat, text, 0) == 0;
}

static void *
pat_regex_init (char const *pattern)
{
  regex_t *rx = mu_alloc (sizeof (*rx));
  int rc = regcomp (rx, pattern, REG_EXTENDED|REG_NOSUB
		                 |(pattern_ci ? REG_ICASE : 0));
  if (rc)
    {
      char errbuf[512];
      regerror (rc, rx, errbuf, sizeof errbuf);
      mu_error ("%s: %s", pattern, errbuf);
      return NULL;
    }
  return rx;
}

static void
pat_regex_free (void *p)
{
  regex_t *rx = p;
  regfree (rx);
  free (rx);
}

static int
pat_regex_match (void *pat, char const *str)
{
  regex_t *rx = pat;
  return regexec (rx, str, 0, NULL, 0) == 0;
}

static struct pattern_match_fun
{
  void *(*pat_init) (char const *pattern);
  int (*pat_match) (void *pat, char const *str);
  void (*pat_free) (void *);
} pattern_match_tab[] = {
  { generic_init, pat_exact_match, generic_free },
  { generic_init, pat_glob_match, generic_free },
  { pat_regex_init, pat_regex_match, pat_regex_free }
};

void *
pattern_init (char const *pattern)
{
  return pattern_match_tab[pattern_type].pat_init (pattern);
}

int
pattern_match (void *pat, char const *str)
{
  return pattern_match_tab[pattern_type].pat_match (pat, str);
}

void
pattern_free (void *pat)
{
  return pattern_match_tab[pattern_type].pat_free (pat);
}

static void
cli_pattern_match (struct mu_parseopt *po, struct mu_option *opt,
		   char const *arg)
{
  switch (opt->opt_short)
    {
    case 'e':
      pattern_type = PAT_EXACT;
      break;
      
    case 'g':
      pattern_type = PAT_GLOB;
      break;
      
    case 'r':
      pattern_type = PAT_REGEX;
      break;
      
    case 'i':
      pattern_ci = 1;
    }
}

static struct mu_option readmsg_options[] = 
{
  { "debug", 'd', NULL, MU_OPTION_DEFAULT,
    N_("display debugging information"),
    mu_c_incr, &dbug },
  { "header", 'h', NULL, MU_OPTION_DEFAULT,
    N_("display entire headers"),
    mu_c_bool, &all_header },
  { "weedlist", 'w', N_("LIST"), MU_OPTION_DEFAULT,
    N_("list of header names separated by whitespace or commas"),
    mu_c_string, &weedlist },
  { "folder", 'f', N_("FOLDER"), MU_OPTION_DEFAULT,
    N_("folder to use"), mu_c_string, &mailbox_name },
  { "no-header", 'n', NULL, MU_OPTION_DEFAULT,
    N_("exclude all headers"),
    mu_c_bool, &no_header },
  { "form-feeds", 'p', NULL, MU_OPTION_DEFAULT,
    N_("output formfeeds between messages"),
    mu_c_bool, &form_feed },
  { "show-all-match", 'a', NULL, MU_OPTION_DEFAULT,
    N_("print all messages matching pattern, not only the first"),
    mu_c_bool, &show_all },
  { "exact",          'e', NULL, MU_OPTION_DEFAULT,
    N_("match exact string (default)"),
    mu_c_int, NULL, cli_pattern_match },    
  { "glob",           'g', NULL, MU_OPTION_DEFAULT,
    N_("match using globbing pattern"),
    mu_c_int, NULL, cli_pattern_match },
  { "regex",          'r', NULL, MU_OPTION_DEFAULT,
    N_("match using POSIX regular expressions"),
    mu_c_int, NULL, cli_pattern_match },
  { "ignorecase",     'i', NULL, MU_OPTION_DEFAULT,
    N_("case-insensitive matching"),
    mu_c_int, NULL, cli_pattern_match },
  { "mime",           'm', NULL, MU_OPTION_DEFAULT,
    N_("decode MIME messages on output"),
    mu_c_bool, &mime_decode },
  MU_OPTION_END
}, *options[] = { readmsg_options, NULL };

struct mu_cfg_param readmsg_cfg_param[] = {
  { "debug", mu_c_int, &dbug, 0, NULL,
    N_("Set debug verbosity level.") },
  { "header", mu_c_bool, &all_header, 0, NULL,
    N_("Display entire headers.") },
  { "weedlist", mu_c_string, &weedlist, 0, NULL,
    N_("Display only headers from this list.  Argument is a list of header "
       "names separated by whitespace or commas."),
    N_("list") },
  { "folder", mu_c_string, &mailbox_name, 0, NULL,
    N_("Read messages from this folder.") },
  { "no-header", mu_c_bool, &no_header, 0, NULL,
    N_("Exclude all headers.") }, 
  { "form-feeds", mu_c_bool, &form_feed, 0, NULL,
    N_("Output formfeed character between messages.") },
  { "show-all-match", mu_c_bool, &show_all, 0, NULL,
    N_("Print all messages matching pattern, not only the first.") },
  { NULL }
};

struct mu_cli_setup cli = {
  options,
  readmsg_cfg_param,
  N_("GNU readmsg -- print messages."),
  NULL
};

static char *readmsg_capa[] = {
  "debug",
  "mailbox",
  "locking",
  NULL
};

static int
string_starts_with (const char * s1, const char *s2)
{
  const unsigned char * p1 = (const unsigned char *) s1;
  const unsigned char * p2 = (const unsigned char *) s2;
  int n = 0;

  /* Sanity.  */
  if (s1 == NULL || s2 == NULL)
    return n;

  while (*p1 && *p2)
    {
      if ((n = mu_toupper (*p1++) - mu_toupper (*p2++)) != 0)
	break;
    }
  return (n == 0);
}

static void
print_unix_header (mu_message_t message)
{
  const char *buf;
  size_t size;
  mu_envelope_t envelope = NULL;

  mu_message_get_envelope (message, &envelope);
      
  if (mu_envelope_sget_sender (envelope, &buf))
    buf = "UNKNOWN";
  mu_printf ("From %s ", buf);
  
  if (mu_envelope_sget_date (envelope, &buf))
    { 
      char datebuf[MU_DATETIME_FROM_LENGTH+1];
      time_t t;
      struct tm *tm;

      t = time (NULL);
      tm = gmtime (&t);
      mu_strftime (datebuf, sizeof datebuf, MU_DATETIME_FROM, tm);
      buf = datebuf;
    }

  mu_printf ("%s", buf);
  size = strlen (buf);
  if (size > 1 && buf[size-1] != '\n')
    mu_printf ("\n");
}

static void
print_header_field (const char *name, const char *value)
{
  if (mime_decode)
    {
      char *s;
      int rc = mu_rfc2047_decode (charset, value, &s);
      if (rc == 0)
	{
	  mu_printf ("%s: %s\n", name, s);
	  free (s);
	}
    }
  else
    mu_printf ("%s: %s\n", name, value);    
}
  

static void
print_header (mu_message_t message, int unix_header, int weedc, char **weedv)
{
  mu_header_t header = NULL;

  mu_message_get_header (message, &header);

  if (weedc == 0)
    {
      mu_stream_t stream = NULL;

      mu_header_get_streamref (header, &stream);
      mu_stream_copy (mu_strout, stream, 0, NULL);
      mu_stream_destroy (&stream);
    }
  else
    {
      int status;
      size_t count;
      size_t i;

      status = mu_header_get_field_count (header, &count);
      if (status)
	{
	  mu_error (_("cannot get number of headers: %s"),
		    mu_strerror (status));
	  return;
	}
      
      for (i = 1; i <= count; i++)
	{
	  int j;
	  const char *name = NULL;
	  const char *value = NULL;

	  mu_header_sget_field_name (header, i, &name);
	  mu_header_sget_field_value (header, i, &value);
	  for (j = 0; j < weedc; j++)
	    {
	      if (weedv[j][0] == '!')
		{
		  if (string_starts_with (name, weedv[j]+1))
		    break;
		}
	      else if (string_starts_with (name, weedv[j]))
		{
		  print_header_field (name, value);
		}
	    }
	}
      mu_printf ("\n");
    }
}

static void
print_body_simple (mu_message_t message)
{
  int status;
  mu_body_t body = NULL;
  mu_stream_t stream = NULL;

  mu_message_get_body (message, &body);

  status = mu_body_get_streamref (body, &stream);
  if (status)
    {
      mu_error (_("cannot get body stream: %s"), mu_strerror (status));
      return;
    }
  mu_stream_copy (mu_strout, stream, 0, NULL);
  mu_stream_destroy (&stream);
}

char *
msgpart_str (size_t *mpart)
{
  size_t len = 0;
  size_t i;
  char *result, *p;
  
  for (i = 1; i < mpart[0]; i++)
    {
      size_t n = mpart[i];
      do
	len++;
      while (n /= 10);
      len++;
    }

  result = malloc (len);
  p = result;
  
  for (i = 1; i < mpart[0]; i++)
    {
      size_t n = mpart[i];
      if (i > 1)
	*p++ = '.';
      do
	{
	  unsigned x = n % 10;
	  *p++ = x + '0';
	}
      while (n /= 10);
    }
  *p = 0;

  return result;
}

static void
print_body_decode (mu_message_t message)
{
  int rc;
  mu_iterator_t itr;
  
  rc = mu_message_get_iterator (message, &itr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_iterator", NULL, rc);
      exit (2);
    }
  
  for (mu_iterator_first (itr); !mu_iterator_is_done (itr);
       mu_iterator_next (itr))
    {
      mu_message_t partmsg;
      mu_stream_t str;
      size_t *p;
      
      rc = mu_iterator_current_kv (itr, (const void**)&p, (void**)&partmsg);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_iterator_current", NULL, rc);
	  continue;
	}

      rc = message_body_stream (partmsg, charset, &str);
      if (rc == 0)
	{
	  mu_stream_copy (mu_strout, str, 0, NULL);
	  mu_stream_destroy (&str);
	}
      else if (rc == MU_ERR_USER0)
	{
	  char *s = msgpart_str (p);
	  mu_stream_printf (mu_strout,
			    "[part %s is a binary attachment: not shown]\n",
			    s);
	  free (s);
	}
      free (p);
    }
  mu_iterator_destroy (&itr);
}

static void
print_body (mu_message_t message)
{
  if (mime_decode)
    print_body_decode (message);
  else
    print_body_simple (message);
}

static void
define_charset (void)
{
  struct mu_lc_all lc_all = { .flags = 0 };
  char *ep = getenv ("LC_ALL");
  if (!ep)
    ep = getenv ("LANG");

  if (ep && mu_parse_lc_all (ep, &lc_all, MU_LC_CSET) == 0)
    {
      charset = mu_strdup (lc_all.charset);
      mu_lc_all_free (&lc_all);
    }
  else
    charset = mu_strdup ("us-ascii");
}

int
main (int argc, char **argv)
{
  int status;
  int *set = NULL;
  int n = 0;
  int i;
  mu_mailbox_t mbox = NULL;
  struct mu_wordsplit ws;
  char **weedv;
  int weedc;
  int unix_header = 0;
  
  /* Native Language Support */
  MU_APP_INIT_NLS ();

  /* register the formats.  */
  mu_register_all_mbox_formats ();
  mu_register_extra_formats ();

  mu_auth_register_module (&mu_auth_tls_module);

  mu_cli (argc, argv, &cli, readmsg_capa, NULL, &argc, &argv);

  if (argc == 0)
    {
      mu_error (_("not enough arguments"));
      exit (1);
    }

  define_charset ();
  
  status = mu_mailbox_create_default (&mbox, mailbox_name);
  if (status != 0)
    {
      if (mailbox_name)
	mu_error (_("could not create mailbox `%s': %s"),
		  mailbox_name,
		  mu_strerror(status));
      else
	mu_error (_("could not create default mailbox: %s"),
		  mu_strerror(status));
      exit (2);
    }

  /* Debuging Trace.  */
  if (dbug)
    {
      mu_debug_set_category_level (MU_DEBCAT_MAILBOX,
                                   MU_DEBUG_LEVEL_UPTO (MU_DEBUG_PROT));
    }

  status = mu_mailbox_open (mbox, MU_STREAM_READ);
  if (status != 0)
    {
      mu_url_t url = NULL;

      mu_mailbox_get_url (mbox, &url);
      mu_error (_("could not open mailbox `%s': %s"),
		mu_url_to_string (url), mu_strerror (status));
      exit (2);
    }

  if (weedlist == NULL)
    weedlist = "Date To Cc Subject From Apparently-";

  ws.ws_delim = WEEDLIST_SEPARATOR;
  status = mu_wordsplit (weedlist, &ws, MU_WRDSF_DEFFLAGS | MU_WRDSF_DELIM);
  if (status)
    {
      mu_error (_("cannot parse weedlist: %s"), mu_wordsplit_strerror (&ws));
      exit (2);
    }

  if (ws.ws_wordc)
    {
      for (i = 0; i < ws.ws_wordc; i++)
	{
	  if (mu_c_strcasecmp (ws.ws_wordv[i], "From_") == 0)
	    {
	      int j;
	      unix_header = 1;
	      free (ws.ws_wordv[i]);
	      for (j = i; j < ws.ws_wordc; j++)
		ws.ws_wordv[j] = ws.ws_wordv[j+1];
	      ws.ws_wordc--;
	      if (ws.ws_wordc == 0 && !all_header)
		no_header = 1;
	    }
	}
      weedc = ws.ws_wordc;
      weedv = ws.ws_wordv;
    }
  
  if (all_header)
    {
      unix_header = 1;
      weedc = 0;
      weedv = NULL;
    }

  /* Build an array containing the message number.  */
  msglist (mbox, show_all, argc, argv, &set, &n);

  for (i = 0; i < n; ++i)
    {
      mu_message_t msg = NULL;

      status = mu_mailbox_get_message (mbox, set[i], &msg);
      if (status != 0)
	{
	  mu_error ("mu_mailbox_get_message: %s",
		    mu_strerror (status));
	  exit (2);
	}

      if (unix_header)
	print_unix_header (msg);
      
      if (!no_header)
	print_header (msg, unix_header, weedc, weedv);
      
      print_body (msg);
      mu_printf (form_feed ? "\f" : "\n");
    }

  mu_printf ("\n");

  mu_mailbox_close (mbox);
  mu_mailbox_destroy (&mbox);
  return 0;
}
