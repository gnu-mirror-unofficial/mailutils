/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019-2021 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. */

#include <mailutils/mailutils.h>
#include <mailutils/sys/envelope.h>
#include "tesh.h"

unsigned long
get_num (char const *s)
{
  unsigned long n;
  char *p;
  errno = 0;
  n = strtoul (s, &p, 10);
  if (errno || *p)
    {
      mu_error ("not a number: %s", s);
      abort ();
    }
  return n;
}

struct interp_env
{
  char const *mbxname;
  mu_mailbox_t mbx;
  mu_message_t msg;
  size_t msgno;
};

int
mbop_env_date (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_envelope_t msg_env;
  char *str;

  MU_ASSERT (mu_message_get_envelope (ienv->msg, &msg_env));
  MU_ASSERT (mu_envelope_aget_date (msg_env, &str));
  mu_printf ("%s", str);
  free (str);
  return 0;
}

int
mbop_env_sender (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_envelope_t msg_env;
  char *str;

  MU_ASSERT (mu_message_get_envelope (ienv->msg, &msg_env));
  MU_ASSERT (mu_envelope_aget_sender (msg_env, &str));
  mu_printf ("%s", str);
  free (str);
  return 0;
}

int
mbop_header_lines (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_header_t hdr;
  size_t lines;

  MU_ASSERT (mu_message_get_header (ienv->msg, &hdr));
  MU_ASSERT (mu_header_lines (hdr, &lines));
  mu_printf ("%lu", (unsigned long) lines);
  return 0;
}

int
mbop_header_count (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_header_t hdr;
  size_t n;

  MU_ASSERT (mu_message_get_header (ienv->msg, &hdr));
  MU_ASSERT (mu_header_get_field_count (hdr, &n));
  mu_printf ("%lu", (unsigned long) n);
  return 0;
}

int
mbop_header_size (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_header_t hdr;
  size_t s;

  MU_ASSERT (mu_message_get_header (ienv->msg, &hdr));
  MU_ASSERT (mu_header_size (hdr, &s));
  mu_printf ("%lu", (unsigned long) s);
  return 0;
}

int
mbop_header_field (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_header_t hdr;
  size_t n = get_num (argv[1]);
  char const *s;

  MU_ASSERT (mu_message_get_header (ienv->msg, &hdr));
  MU_ASSERT (mu_header_sget_field_name (hdr, n, &s));
  mu_printf ("%s", s);
  return 0;
}

int
mbop_header_value (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_header_t hdr;
  size_t n = get_num (argv[1]);
  char const *s;

  MU_ASSERT (mu_message_get_header (ienv->msg, &hdr));
  MU_ASSERT (mu_header_sget_field_value (hdr, n, &s));
  mu_printf ("%s", s);
  return 0;
}

int
mbop_headers (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_header_t hdr;
  char const *name;
  char *val;
  size_t i, n;

  MU_ASSERT (mu_message_get_header (ienv->msg, &hdr));
  MU_ASSERT (mu_header_get_field_count (hdr, &n));
  for (i = 1; i <= n; i++)
    {
      MU_ASSERT (mu_header_sget_field_name (hdr, i, &name));
      mu_printf ("%s:", name);
      MU_ASSERT (mu_header_aget_field_value_unfold (hdr, i, &val));
      mu_printf ("%s\n", val);
    }
  return 0;
}

int
mbop_body_lines (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_body_t body;
  size_t lines;

  MU_ASSERT (mu_message_get_body (ienv->msg, &body));
  MU_ASSERT (mu_body_lines (body, &lines));
  mu_printf ("%lu", (unsigned long) lines);
  return 0;
}

int
mbop_body_size (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_body_t body;
  size_t s;

  MU_ASSERT (mu_message_get_body (ienv->msg, &body));
  MU_ASSERT (mu_body_size (body, &s));
  mu_printf ("%lu", (unsigned long) s);
  return 0;
}

int
mbop_body_text (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_body_t body;
  mu_stream_t str;

  MU_ASSERT (mu_message_get_body (ienv->msg, &body));
  MU_ASSERT (mu_body_get_streamref (body, &str));
  MU_ASSERT (mu_stream_copy (mu_strout, str, 0, NULL));
  mu_stream_unref (str);
  return 0;
}

int
mbop_attr (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_attribute_t attr;
  char abuf[MU_STATUS_BUF_SIZE];

  MU_ASSERT (mu_message_get_attribute (ienv->msg, &attr));
  MU_ASSERT (mu_attribute_to_string (attr, abuf, sizeof (abuf), NULL));
  mu_printf ("%s", abuf[0] ? abuf : "-");
  return 0;
}

int
mbop_uid (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t uid;

  MU_ASSERT (mu_message_get_uid (ienv->msg, &uid));
  mu_printf ("%lu", (unsigned long) uid);
  return 0;
}

int
mbop_message_lines (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t lines;

  MU_ASSERT (mu_message_lines (ienv->msg, &lines));
  mu_printf ("%lu", (unsigned long) lines);
  return 0;
}

int
mbop_message_size (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t size;

  MU_ASSERT (mu_message_size (ienv->msg, &size));
  mu_printf ("%lu", (unsigned long) size);
  return 0;
}

#define __cat2__(a,b) a ## b
#define __cat3__(a,b,c) a ## b ## c
#define __cat4__(a,b,c,d) a ## b ## c ## d
#define ATTR_FUN(op,attr)				      \
static int						      \
__cat4__(mbop_,op,_,attr) (int argc, char **argv, mu_assoc_t options, void *env)    \
{							      \
  struct interp_env *ienv = env;			      \
  mu_attribute_t a;					      \
  MU_ASSERT (mu_message_get_attribute (ienv->msg, &a));       \
  MU_ASSERT (__cat4__(mu_attribute_,op,_,attr)(a));	      \
  mu_printf ("OK");					      \
  return 0;						      \
}

ATTR_FUN(set,seen)
ATTR_FUN(set,answered)
ATTR_FUN(set,flagged)
ATTR_FUN(set,deleted)
ATTR_FUN(set,draft)
ATTR_FUN(set,recent)
ATTR_FUN(set,read)

ATTR_FUN(unset,seen)
ATTR_FUN(unset,answered)
ATTR_FUN(unset,flagged)
ATTR_FUN(unset,deleted)
ATTR_FUN(unset,draft)
ATTR_FUN(unset,recent)
ATTR_FUN(unset,read)

int
mbop_append (int argc, char **argv, mu_assoc_t options, void *call_env)
{
  struct interp_env *ienv = call_env;
  mu_stream_t str;
  mu_message_t newmsg;
  mu_attribute_t atr = NULL;
  mu_envelope_t env = NULL;
  char *s;
  
  MU_ASSERT (mu_file_stream_create (&str, argv[1], MU_STREAM_READ));
  MU_ASSERT (mu_stream_to_message (str, &newmsg));
  
  if (mu_assoc_lookup (options, "attr", &s) == 0)
    {
      int flags = 0;

      MU_ASSERT (mu_attribute_create (&atr, NULL));
      MU_ASSERT (mu_attribute_string_to_flags (s, &flags));
      MU_ASSERT (mu_attribute_set_flags (atr, flags));
    }

  if (mu_assoc_lookup (options, "sender", &s) == 0)
    {
      MU_ASSERT (mu_envelope_create (&env, NULL));
      env->sender = mu_strdup (s);
    }

  if (mu_assoc_lookup (options, "date", &s) == 0)
    {
      struct tm tm;
      char datebuf[MU_DATETIME_FROM_LENGTH+1];
      
      if (!env)
	MU_ASSERT (mu_envelope_create (&env, NULL));
      MU_ASSERT (mu_parse_date_dtl (s, NULL, NULL, &tm, NULL, NULL));

      mu_strftime (datebuf, sizeof (datebuf), MU_DATETIME_FROM, &tm);
      env->date = mu_strdup (datebuf);
    }
  
  if (env || atr)
    {
      MU_ASSERT (mu_mailbox_append_message_ext (ienv->mbx, newmsg, env, atr));
      mu_envelope_destroy (&env, NULL);
      mu_attribute_destroy (&atr, NULL);
    }
  else
    MU_ASSERT (mu_mailbox_append_message (ienv->mbx, newmsg));
  
  mu_envelope_destroy (&env, NULL);
  mu_attribute_destroy (&atr, NULL);
  mu_stream_destroy (&str);
  mu_printf ("OK");
  return 0;
}

int
mbop_expunge (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;

  MU_ASSERT (mu_mailbox_expunge (ienv->mbx));
  mu_printf ("OK");
  return 0;
}

int
mbop_sync (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;

  MU_ASSERT (mu_mailbox_sync (ienv->mbx));
  mu_printf ("OK");
  return 0;
}

int
mbop_count (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t n;

  MU_ASSERT (mu_mailbox_messages_count (ienv->mbx, &n));
  mu_printf ("%lu", (unsigned long) n);
  return 0;
}

int
mbop_uidvalidity (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  unsigned long v;

  MU_ASSERT (mu_mailbox_uidvalidity (ienv->mbx, &v));
  mu_printf ("%lu", v);
  return 0;
}

int
mbop_uidnext (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t n;

  MU_ASSERT (mu_mailbox_uidnext (ienv->mbx, &n));
  mu_printf ("%lu", (unsigned long) n);
  return 0;
}

int
mbop_recent (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t n;

  MU_ASSERT (mu_mailbox_messages_recent (ienv->mbx, &n));
  mu_printf ("%lu", (unsigned long) n);
  return 0;
}

int
mbop_unseen (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t n;

  MU_ASSERT (mu_mailbox_message_unseen (ienv->mbx, &n));
  mu_printf ("%lu", (unsigned long) n);
  return 0;
}

static char const *mbox_actions[] = {
  "expunge",
  "sync",
  "append",
  "uidvalidity",
  "uidvalidity_reset",
  "uidnext",
  "count",
  "recent",
  "unseen",
  "qget",
  NULL
};

int
needs_message (char const *name)
{
  int i;
  for (i = 0; mbox_actions[i]; i++)
    if (strcmp (mbox_actions[i], name) == 0)
      return 0;
  return 1;
}
    
int
mbop_envinit (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  if (needs_message (argv[0]))
    {
      if (!ienv->msg)
	{
	  mu_error ("no message selected");
	  exit (1);
	}
      mu_printf ("%lu ", (unsigned long) ienv->msgno);
    }
  mu_printf ("%s: ", argv[0]);
  return 0;
}

int
mbop_envfini (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_printf ("\n");
  return 0;
}

int
mbop_nocmd (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  if (mu_isdigit (*argv[0]))
    {
      ienv->msgno = get_num (argv[0]);
      MU_ASSERT (mu_mailbox_get_message (ienv->mbx, ienv->msgno, &ienv->msg));
      mu_printf ("%lu current message\n", (unsigned long) ienv->msgno);
      return 0;
    }
  return MU_ERR_PARSE;
}

int
mbop_qget (int argc, char **argv, mu_assoc_t options, void *data)
{
  struct interp_env *env = data;
  mu_mailbox_t mbx;
  mu_message_qid_t qid;
  mu_message_t msg;
  mu_stream_t str;
  
  MU_ASSERT (mu_mailbox_create_default (&mbx, env->mbxname));
  MU_ASSERT (mu_mailbox_open (mbx, MU_STREAM_READ|MU_STREAM_QACCESS));
  qid = argv[1];
  MU_ASSERT (mu_mailbox_quick_get_message (mbx, qid, &msg));
  MU_ASSERT (mu_message_get_streamref (msg, &str));

  MU_ASSERT (mu_stream_copy (mu_strout, str, 0, NULL));
  mu_stream_destroy (&str);
  mu_mailbox_destroy (&mbx);
  return 0;
}

int
mbop_uidvalidity_reset (int argc, char **argv, mu_assoc_t options, void *data)
{
  struct interp_env *ienv = data;
  MU_ASSERT (mu_mailbox_uidvalidity_reset (ienv->mbx));
  mu_printf ("OK");
  return 0;
}


struct mu_tesh_command commands[] = {
  { "__ENVINIT__",    "", mbop_envinit  },
  { "__ENVFINI__",    "", mbop_envfini  },
  { "__NOCMD__",      "", mbop_nocmd    },
  { "env_date",       "", mbop_env_date },
  { "env_sender",     "", mbop_env_sender     },
  { "header_lines",   "", mbop_header_lines   },
  { "header_size",    "", mbop_header_size    },
  { "header_count",   "", mbop_header_count   },
  { "header_field",   "NUMBER", mbop_header_field   },
  { "header_value",   "NAME", mbop_header_value   },
  { "headers",        "", mbop_headers        },
  { "body_lines",     "", mbop_body_lines     },
  { "body_size",      "", mbop_body_size      },
  { "body_text",      "", mbop_body_text      },
  { "attr",           "", mbop_attr           },
  { "uid",            "", mbop_uid            },
  { "set_seen",       "", mbop_set_seen       },
  { "set_answered",   "", mbop_set_answered   },
  { "set_flagged",    "", mbop_set_flagged    },
  { "set_deleted",    "", mbop_set_deleted    },
  { "set_draft",      "", mbop_set_draft      },
  { "set_recent",     "", mbop_set_recent     },
  { "set_read",       "", mbop_set_read       },
  { "unset_seen",     "", mbop_unset_seen     },
  { "unset_answered", "", mbop_unset_answered },
  { "unset_flagged",  "", mbop_unset_flagged  },
  { "unset_deleted",  "", mbop_unset_deleted  },
  { "unset_draft",    "", mbop_unset_draft    },
  { "unset_recent",   "", mbop_unset_recent   },
  { "unset_read",     "", mbop_unset_read     },
  { "expunge",        "", mbop_expunge },
  { "sync",           "", mbop_sync },
  { "append",         "[-sender=EMAIL] [-date=DATE] [-attr=FLAGS] FILE", mbop_append },
  { "uidvalidity",    "", mbop_uidvalidity },
  { "uidnext",        "", mbop_uidnext },
  { "uidvalidity_reset", "", mbop_uidvalidity_reset },
  { "count",          "", mbop_count },
  { "recent",         "", mbop_recent },
  { "unseen",         "", mbop_unseen },
  { "qget",           "QID", mbop_qget },
  { "message_lines",  "", mbop_message_lines },
  { "message_size",  "", mbop_message_size },
  { NULL }
};

static int
test_notify (mu_observer_t obs, size_t type, void *data, void *action_data)
{
  struct interp_env *env = mu_observer_get_owner (obs);
  mu_mailbox_t mbx;
  mu_message_qid_t qid = data;
  int rc;
  mu_message_t msg;
  mu_header_t hdr;
  char const *from = "(NONE)", *subj = "(NONE)";

  MU_ASSERT (mu_mailbox_create_default (&mbx, env->mbxname));
  MU_ASSERT (mu_mailbox_open (mbx, MU_STREAM_READ|MU_STREAM_QACCESS));
  
  rc = mu_mailbox_quick_get_message (mbx, qid, &msg);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_mailbox_quick_get_message",
		       NULL, rc);
      goto err;
    }
  
  rc = mu_message_get_header (msg, &hdr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_header",
		       NULL, rc);
      goto err;
    }
  
  mu_header_sget_value (hdr, MU_HEADER_FROM, &from);
  mu_header_sget_value (hdr, MU_HEADER_SUBJECT, &subj);
  mu_stream_printf (mu_strerr, "new message: %s %s\n", from, subj);

 err:
  mu_mailbox_close (mbx);
  mu_mailbox_destroy (&mbx);
  
  return 0;
}


int
main (int argc, char **argv)
{
  struct interp_env env = { NULL, NULL, NULL, 0 };
  int debug_option = 0;
  int detect_option = 0;
  int notify_option = 0;
  int append_option = 0;
  int ro_option = 0;
  int mbox_flags;
  struct mu_option options[] = {
    { "debug", 'd', NULL, MU_OPTION_DEFAULT,
      "enable debugging",
      mu_c_incr, &debug_option },
    { "mailbox", 'm', "FILE", MU_OPTION_DEFAULT,
      "use this mailbox",
      mu_c_string, &env.mbxname },
    { "detect", 'D', NULL, MU_OPTION_DEFAULT,
      "detect mailbox format",
      mu_c_incr, &detect_option },
    { "notify", 'N', NULL, MU_OPTION_DEFAULT,
      "test notification code",
      mu_c_incr, &notify_option },
    { "append", 'a', NULL, MU_OPTION_DEFAULT,
      "open mailbox in append mode",
      mu_c_incr, &append_option },
    { "read-only", 'r', NULL, MU_OPTION_DEFAULT,
      "open mailbox in read-only mode",
      mu_c_incr, &ro_option },
    MU_OPTION_END
  };

  env.mbxname = getenv ("MAIL");
  mu_tesh_init (argv[0]);
  mu_registrar_record (MBOP_RECORD);
  mu_registrar_set_default_scheme (MBOP_SCHEME);

  mu_cli_simple (argc, argv,
                 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC, "test tool for " MBOP_SCHEME " mailboxes",
		 MU_CLI_OPTION_PROG_ARGS, "CMD [; CMD ;...]",
		 MU_CLI_OPTION_EX_USAGE, 2,
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
                 MU_CLI_OPTION_RETURN_ARGV, &argv,
		 MU_CLI_OPTION_END);

  if (debug_option)
    mu_debug_enable_category ("mailbox", 7,
			      MU_DEBUG_LEVEL_UPTO (MU_DEBUG_PROT));

  if (detect_option)
    {
      mu_url_t url;
      mu_record_t rec;
      int n;

      MU_ASSERT (mu_registrar_lookup_scheme (MBOP_SCHEME, &rec));
      MU_ASSERT (mu_url_create_hint (&url, env.mbxname,
				     MU_URL_PARSE_SLASH | MU_URL_PARSE_LOCAL,
				     NULL));
      n = mu_record_is_scheme (rec, url, MU_FOLDER_ATTRIBUTE_FILE);
      mu_printf ("%s: %d\n", env.mbxname, n);
      mu_url_destroy (&url);
      exit (n & MU_FOLDER_ATTRIBUTE_FILE ? 0 : 1);
    }

  if (append_option && ro_option)
    {
      mu_error ("conflicting options");
      exit (2);
    }
  if (append_option)
    mbox_flags = MU_STREAM_APPEND;
  else if (ro_option)
    mbox_flags = MU_STREAM_READ;
  else
    mbox_flags = MU_STREAM_RDWR;

#ifdef MBOP_PRE_OPEN_HOOK
  MBOP_PRE_OPEN_HOOK ();
#endif
  MU_ASSERT (mu_mailbox_create_default (&env.mbx, env.mbxname));
  MU_ASSERT (mu_mailbox_open (env.mbx, mbox_flags));

  if (notify_option)
    {
      mu_observer_t observer;
      mu_observable_t observable;

      mu_observer_create (&observer, &env);
      mu_observer_set_action (observer, test_notify, &env);
      mu_mailbox_get_observable (env.mbx, &observable);
      mu_observable_attach (observable, MU_EVT_MAILBOX_MESSAGE_APPEND, 
			    observer);
    }
  
  mu_tesh_read_and_eval (argc, argv, commands, &env);
  mu_mailbox_close (env.mbx);
  mu_mailbox_destroy (&env.mbx);
  return 0;
}
