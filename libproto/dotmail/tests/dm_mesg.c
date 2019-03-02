/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019 Free Software Foundation, Inc.

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

void
dm_env_date (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_envelope_t env;
  char *str;

  MU_ASSERT (mu_message_get_envelope (msg, &env));
  MU_ASSERT (mu_envelope_aget_date (env, &str));
  mu_printf ("%s", str);
  free (str);
}

void
dm_env_sender (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_envelope_t env;
  char *str;

  MU_ASSERT (mu_message_get_envelope (msg, &env));
  MU_ASSERT (mu_envelope_aget_sender (env, &str));
  mu_printf ("%s", str);
  free (str);
}

void
dm_header_lines (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  size_t lines;
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_lines (hdr, &lines));
  mu_printf ("%lu", (unsigned long) lines);
}

void
dm_header_count (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  size_t n;
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_get_field_count (hdr, &n));
  mu_printf ("%lu", (unsigned long) n);
}

void
dm_header_size (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  size_t s;
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_size (hdr, &s));
  mu_printf ("%lu", (unsigned long) s);
}

void
dm_header_field (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  size_t n = get_num (argv[0]);
  char const *s;
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_sget_field_name (hdr, n, &s));
  mu_printf ("%s", s);
}

void
dm_header_value (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  size_t n = get_num (argv[0]);
  char const *s;
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_sget_field_value (hdr, n, &s));
  mu_printf ("%s", s);
}

void
dm_headers (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  char const *name;
  char *val;
  size_t i, n;
  
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_get_field_count (hdr, &n));
  for (i = 1; i <= n; i++)
    {
      MU_ASSERT (mu_header_sget_field_name (hdr, i, &name));
      mu_printf ("%s:", name);
      MU_ASSERT (mu_header_aget_field_value_unfold (hdr, i, &val));
      mu_printf ("%s\n", val);
    }
}

void
dm_body_lines (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_body_t body;
  size_t lines;
  MU_ASSERT (mu_message_get_body (msg, &body));
  MU_ASSERT (mu_body_lines (body, &lines));
  mu_printf ("%lu", (unsigned long) lines);
}

void
dm_body_size (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_body_t body;
  size_t s;
  MU_ASSERT (mu_message_get_body (msg, &body));
  MU_ASSERT (mu_body_size (body, &s));
  mu_printf ("%lu", (unsigned long) s);
}

void
dm_body_text (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_body_t body;
  mu_stream_t str;
  MU_ASSERT (mu_message_get_body (msg, &body));
  MU_ASSERT (mu_body_get_streamref (body, &str));
  MU_ASSERT (mu_stream_copy (mu_strout, str, 0, NULL));
  mu_stream_unref (str);
}

void
dm_attr (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_attribute_t attr;
  char abuf[MU_STATUS_BUF_SIZE];
  MU_ASSERT (mu_message_get_attribute (msg, &attr));
  MU_ASSERT (mu_attribute_to_string (attr, abuf, sizeof (abuf), NULL));
  mu_printf ("%s", abuf[0] ? abuf : "-");
}

void
dm_uid (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  size_t uid;
  MU_ASSERT (mu_message_get_uid (msg, &uid));
  mu_printf ("%lu", (unsigned long) uid);
}

#define __cat2__(a,b) a ## b
#define __cat3__(a,b,c) a ## b ## c
#define __cat4__(a,b,c,d) a ## b ## c ## d
#define ATTR_FUN(op,attr)				\
static void						\
__cat4__(dm_,op,_,attr) (mu_mailbox_t mbx, mu_message_t msg, char **argv) \
{							\
  mu_attribute_t a;                                     \
  MU_ASSERT (mu_message_get_attribute (msg, &a));       \
  MU_ASSERT (__cat4__(mu_attribute_,op,_,attr)(a));	\
  mu_printf ("OK");                                     \
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

void
dm_append (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  mu_stream_t str;
  mu_message_t newmsg;
  MU_ASSERT (mu_file_stream_create (&str, argv[0], MU_STREAM_READ));
  MU_ASSERT (mu_stream_to_message (str, &newmsg));
  MU_ASSERT (mu_mailbox_append_message (mbx, newmsg));
  mu_stream_destroy (&str);
  mu_printf ("OK");
}

void
dm_expunge (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  MU_ASSERT (mu_mailbox_expunge (mbx));
  mu_printf ("OK");
}

void
dm_sync (mu_mailbox_t mbx, mu_message_t msg, char **argv)
{
  MU_ASSERT (mu_mailbox_sync (mbx));
  mu_printf ("OK");
}

typedef void (*dm_action_fn) (mu_mailbox_t, mu_message_t, char **);

struct dm_action
{
  char *name;
  dm_action_fn fn;
  int needs_message;
  int narg;
};

struct dm_action actions[] = {
  { "env_date",       dm_env_date,       1, 0 },
  { "env_sender",     dm_env_sender,     1, 0 },
  { "header_lines",   dm_header_lines,   1, 0 },
  { "header_size",    dm_header_size,    1, 0 },
  { "header_count",   dm_header_count,   1, 0 },
  { "header_field",   dm_header_field,   1, 1 },
  { "header_value",   dm_header_value,   1, 1 },
  { "headers",        dm_headers,        1, 0 },
  { "body_lines",     dm_body_lines,     1, 0 },
  { "body_size",      dm_body_size,      1, 0 },
  { "body_text",      dm_body_text,      1, 0 },
  { "attr",           dm_attr,           1, 0 },
  { "uid",            dm_uid,            1, 0 },
  { "set_seen",       dm_set_seen,       1, 0 },
  { "set_answered",   dm_set_answered,   1, 0 },
  { "set_flagged",    dm_set_flagged,    1, 0 },
  { "set_deleted",    dm_set_deleted,    1, 0 },
  { "set_draft",      dm_set_draft,      1, 0 },
  { "set_recent",     dm_set_recent,     1, 0 },
  { "set_read",       dm_set_read,       1, 0 },
  { "unset_seen",     dm_unset_seen,     1, 0 },
  { "unset_answered", dm_unset_answered, 1, 0 },
  { "unset_flagged",  dm_unset_flagged,  1, 0 },
  { "unset_deleted",  dm_unset_deleted,  1, 0 },
  { "unset_draft",    dm_unset_draft,    1, 0 },
  { "unset_recent",   dm_unset_recent,   1, 0 },
  { "unset_read",     dm_unset_read,     1, 0 },
  { "expunge",        dm_expunge,        0, 0 },
  { "sync",           dm_sync,           0, 0 },
  { "append",         dm_append,         0, 1 },
  
  { NULL }
};

static struct dm_action *
get_action (char const *s)
{
  size_t i;

  for (i = 0; actions[i].name; i++)
    if (strcmp (actions[i].name, s) == 0)
      return &actions[i];

  return NULL;
}    

int
main (int argc, char **argv)
{
  mu_mailbox_t mbx;
  mu_message_t msg = NULL;
  char *mailbox_name = getenv ("MAIL");
  size_t msgno;
  char *buf = NULL;
  size_t size = 0, n;
  struct mu_wordsplit ws;
  int wsflags;
  int rc;
  
  mu_set_program_name (argv[0]);
  mu_stdstream_setup (MU_STDSTREAM_RESET_NONE);
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

  MU_ASSERT (mu_mailbox_create_default (&mbx, mailbox_name));
  MU_ASSERT (mu_mailbox_open (mbx, MU_STREAM_RDWR));

  wsflags  = MU_WRDSF_DEFFLAGS
           | MU_WRDSF_COMMENT
           | MU_WRDSF_ALLOC_DIE
           | MU_WRDSF_SHOWERR;
  ws.ws_comment = "#";

  while ((rc = mu_stream_getline (mu_strin, &buf, &size, &n)) == 0 && n > 0)
    {
      struct dm_action *act;
      
      mu_ltrim_class (buf, MU_CTYPE_SPACE);
      mu_rtrim_class (buf, MU_CTYPE_SPACE);

      MU_ASSERT (mu_wordsplit (buf, &ws, wsflags));
      wsflags |= MU_WRDSF_REUSE;

      if (ws.ws_wordc == 0)
	continue;
      
      if (mu_isdigit (*ws.ws_wordv[0]))
	{
	  msgno = get_num (ws.ws_wordv[0]);
	  MU_ASSERT (mu_mailbox_get_message (mbx, msgno, &msg));
	  mu_printf ("%lu current message\n", (unsigned long) msgno);
	  continue;
	}

      act = get_action (ws.ws_wordv[0]);
      if (!act)
	{
	  mu_error ("%s: unrecognized action", ws.ws_wordv[0]);
	  return 1;
	}

      if (act->needs_message && !msg)
	{
	  mu_error ("no message selected");
	  return 1;
	}

      if (act->narg + 1 != ws.ws_wordc)
	{
	  mu_error ("bad number of arguments for %s", ws.ws_wordv[0]);
	  return 1;
	}

      if (act->needs_message)
	mu_printf ("%lu ", (unsigned long) msgno);
      mu_printf ("%s: ", ws.ws_wordv[0]);
      act->fn (mbx, msg, ws.ws_wordv + 1);
      mu_printf ("\n");
    }
  if (wsflags & MU_WRDSF_REUSE)
    mu_wordsplit_free (&ws);
  mu_mailbox_close (mbx);
  mu_mailbox_destroy (&mbx);
  return 0;
}
