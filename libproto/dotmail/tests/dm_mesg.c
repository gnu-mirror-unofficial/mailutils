/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019-2020 Free Software Foundation, Inc.

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
  mu_mailbox_t mbx;
  mu_message_t msg;
  size_t msgno;
};

int
dm_env_date (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_env_sender (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_header_lines (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_header_count (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_header_size (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_header_field (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_header_value (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_headers (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_body_lines (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_body_size (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_body_text (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_attr (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_uid (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t uid;

  MU_ASSERT (mu_message_get_uid (ienv->msg, &uid));
  mu_printf ("%lu", (unsigned long) uid);
  return 0;
}

#define __cat2__(a,b) a ## b
#define __cat3__(a,b,c) a ## b ## c
#define __cat4__(a,b,c,d) a ## b ## c ## d
#define ATTR_FUN(op,attr)				      \
static int						      \
__cat4__(dm_,op,_,attr) (int argc, char **argv, mu_assoc_t options, void *env)    \
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
dm_append (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  mu_stream_t str;
  mu_message_t newmsg;

  MU_ASSERT (mu_file_stream_create (&str, argv[1], MU_STREAM_READ));
  MU_ASSERT (mu_stream_to_message (str, &newmsg));
  MU_ASSERT (mu_mailbox_append_message (ienv->mbx, newmsg));
  mu_stream_destroy (&str);
  mu_printf ("OK");
  return 0;
}

int
dm_expunge (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;

  MU_ASSERT (mu_mailbox_expunge (ienv->mbx));
  mu_printf ("OK");
  return 0;
}

int
dm_sync (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;

  MU_ASSERT (mu_mailbox_sync (ienv->mbx));
  mu_printf ("OK");
  return 0;
}

int
dm_count (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t n;

  MU_ASSERT (mu_mailbox_messages_count (ienv->mbx, &n));
  mu_printf ("%lu", (unsigned long) n);
  return 0;
}

int
dm_uidvalidity (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  unsigned long v;

  MU_ASSERT (mu_mailbox_uidvalidity (ienv->mbx, &v));
  mu_printf ("%lu", v);
  return 0;
}

int
dm_uidnext (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t n;

  MU_ASSERT (mu_mailbox_uidnext (ienv->mbx, &n));
  mu_printf ("%lu", (unsigned long) n);
  return 0;
}

int
dm_recent (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct interp_env *ienv = env;
  size_t n;

  MU_ASSERT (mu_mailbox_messages_recent (ienv->mbx, &n));
  mu_printf ("%lu", (unsigned long) n);
  return 0;
}

int
dm_unseen (int argc, char **argv, mu_assoc_t options, void *env)
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
  "uidnext",
  "count",
  "recent",
  "unseen",
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
dm_envinit (int argc, char **argv, mu_assoc_t options, void *env)
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
dm_envfini (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_printf ("\n");
  return 0;
}

int
dm_nocmd (int argc, char **argv, mu_assoc_t options, void *env)
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

struct mu_tesh_command commands[] = {
  { "__ENVINIT__",    "",    dm_envinit  },
  { "__ENVFINI__",    "",    dm_envfini  },
  { "__NOCMD__",      "",    dm_nocmd    },
  { "env_date",       "", dm_env_date },
  { "env_sender",     "", dm_env_sender     },
  { "header_lines",   "", dm_header_lines   },
  { "header_size",    "", dm_header_size    },
  { "header_count",   "", dm_header_count   },
  { "header_field",   "NUMBER", dm_header_field   },
  { "header_value",   "NAME", dm_header_value   },
  { "headers",        "", dm_headers        },
  { "body_lines",     "", dm_body_lines     },
  { "body_size",      "", dm_body_size      },
  { "body_text",      "", dm_body_text      },
  { "attr",           "", dm_attr           },
  { "uid",            "", dm_uid            },
  { "set_seen",       "", dm_set_seen       },
  { "set_answered",   "", dm_set_answered   },
  { "set_flagged",    "", dm_set_flagged    },
  { "set_deleted",    "", dm_set_deleted    },
  { "set_draft",      "", dm_set_draft      },
  { "set_recent",     "", dm_set_recent     },
  { "set_read",       "", dm_set_read       },
  { "unset_seen",     "", dm_unset_seen     },
  { "unset_answered", "", dm_unset_answered },
  { "unset_flagged",  "", dm_unset_flagged  },
  { "unset_deleted",  "", dm_unset_deleted  },
  { "unset_draft",    "", dm_unset_draft    },
  { "unset_recent",   "", dm_unset_recent   },
  { "unset_read",     "", dm_unset_read     },
  { "expunge",        "", dm_expunge },
  { "sync",           "", dm_sync },
  { "append",         "FILE", dm_append },
  { "uidvalidity",    "", dm_uidvalidity },
  { "uidnext",        "", dm_uidnext },
  { "count",          "", dm_count },
  { "recent",         "", dm_recent },
  { "unseen",         "", dm_unseen },
  { NULL }
};

int
main (int argc, char **argv)
{
  struct interp_env env = { NULL, NULL, 0 };
  char *mailbox_name = getenv ("MAIL");

  mu_tesh_init (argv[0]);
  mu_registrar_record (mu_dotmail_record);

  argc--;
  argv++;

  if (argc && strcmp (argv[0], "-d") == 0)
    {
      mu_debug_enable_category ("mailbox", 7,
				MU_DEBUG_LEVEL_UPTO (MU_DEBUG_PROT));
      argc--;
      argv++;
    }

  MU_ASSERT (mu_mailbox_create_default (&env.mbx, mailbox_name));
  MU_ASSERT (mu_mailbox_open (env.mbx, MU_STREAM_RDWR));

  mu_tesh_read_and_eval (argc, argv, commands, &env);
  return 0;
}
