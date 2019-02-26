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
dm_env_date (mu_message_t msg, char **argv)
{
  mu_envelope_t env;
  char *str;

  MU_ASSERT (mu_message_get_envelope (msg, &env));
  MU_ASSERT (mu_envelope_aget_date (env, &str));
  mu_printf ("%s", str);
  free (str);
}

void
dm_env_sender (mu_message_t msg, char **argv)
{
  mu_envelope_t env;
  char *str;

  MU_ASSERT (mu_message_get_envelope (msg, &env));
  MU_ASSERT (mu_envelope_aget_sender (env, &str));
  mu_printf ("%s", str);
  free (str);
}

void
dm_header_lines (mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  size_t lines;
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_lines (hdr, &lines));
  mu_printf ("%lu", (unsigned long) lines);
}

void
dm_header_count (mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  size_t n;
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_get_field_count (hdr, &n));
  mu_printf ("%lu", (unsigned long) n);
}

void
dm_header_size (mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  size_t s;
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_size (hdr, &s));
  mu_printf ("%lu", (unsigned long) s);
}

void
dm_header_field (mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  size_t n = get_num (argv[0]);
  char const *s;
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_sget_field_name (hdr, n, &s));
  mu_printf ("%s", s);
}

void
dm_header_value (mu_message_t msg, char **argv)
{
  mu_header_t hdr;
  size_t n = get_num (argv[0]);
  char const *s;
  MU_ASSERT (mu_message_get_header (msg, &hdr));
  MU_ASSERT (mu_header_sget_field_value (hdr, n, &s));
  mu_printf ("%s", s);
}

void
dm_headers (mu_message_t msg, char **argv)
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
dm_body_lines (mu_message_t msg, char **argv)
{
  mu_body_t body;
  size_t lines;
  MU_ASSERT (mu_message_get_body (msg, &body));
  MU_ASSERT (mu_body_lines (body, &lines));
  mu_printf ("%lu", (unsigned long) lines);
}

void
dm_body_size (mu_message_t msg, char **argv)
{
  mu_body_t body;
  size_t s;
  MU_ASSERT (mu_message_get_body (msg, &body));
  MU_ASSERT (mu_body_size (body, &s));
  mu_printf ("%lu", (unsigned long) s);
}

void
dm_body_text (mu_message_t msg, char **argv)
{
  mu_body_t body;
  mu_stream_t str;
  MU_ASSERT (mu_message_get_body (msg, &body));
  MU_ASSERT (mu_body_get_streamref (body, &str));
  MU_ASSERT (mu_stream_copy (mu_strout, str, 0, NULL));
  mu_stream_unref (str);
}

void
dm_attr (mu_message_t msg, char **argv)
{
  mu_attribute_t attr;
  char abuf[MU_STATUS_BUF_SIZE];
  MU_ASSERT (mu_message_get_attribute (msg, &attr));
  MU_ASSERT (mu_attribute_to_string (attr, abuf, sizeof (abuf), NULL));
  mu_printf ("%s", abuf[0] ? abuf : "-");
}

void
dm_uid (mu_message_t msg, char **argv)
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
__cat4__(dm_,op,_,attr) (mu_message_t msg, char **argv)	\
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
dmbox_append (mu_mailbox_t mbx, char *filename)
{
  mu_stream_t str;
  mu_message_t msg;
  MU_ASSERT (mu_file_stream_create (&str, filename, MU_STREAM_READ));
  MU_ASSERT (mu_stream_to_message (str, &msg));
  MU_ASSERT (mu_mailbox_append_message (mbx, msg));
  mu_stream_destroy (&str);
  mu_printf ("append: OK\n");
}


typedef void (*dm_action) (mu_message_t, char **);

static struct
{
  char *name;
  dm_action act;
  int narg;
} actions[] = {
  { "env_date", dm_env_date },
  { "env_sender", dm_env_sender },
  { "header_lines", dm_header_lines },
  { "header_size", dm_header_size },
  { "header_count", dm_header_count },
  { "header_field", dm_header_field, 1 },
  { "header_value", dm_header_value, 1 },
  { "headers", dm_headers },
  { "body_lines", dm_body_lines },
  { "body_size", dm_body_size },
  { "body_text", dm_body_text },
  { "attr", dm_attr },
  { "uid", dm_uid },
  { "set_seen", dm_set_seen },
  { "set_answered", dm_set_answered },
  { "set_flagged", dm_set_flagged },
  { "set_deleted", dm_set_deleted },
  { "set_draft", dm_set_draft },
  { "set_recent", dm_set_recent },
  { "set_read", dm_set_read },
  { "unset_seen", dm_unset_seen },
  { "unset_answered", dm_unset_answered },
  { "unset_flagged", dm_unset_flagged },
  { "unset_deleted", dm_unset_deleted },
  { "unset_draft", dm_unset_draft },
  { "unset_recent", dm_unset_recent },
  { "unset_read", dm_unset_read },
  { NULL }
};

static dm_action
get_action (char const *s, size_t *narg)
{
  size_t i;

  for (i = 0; actions[i].name; i++)
    if (strcmp (actions[i].name, s) == 0)
      {
	*narg = actions[i].narg;
	return actions[i].act;
      }

  return NULL;
}    

int
main (int argc, char **argv)
{
  mu_mailbox_t mbx;
  mu_message_t msg = NULL;
  char *mailbox_name = getenv ("MAIL");
  size_t n;
  
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
 
  while (argc--)
    {
      char *a = *argv++;
      dm_action f;
      size_t narg;
      
      if (mu_isdigit (*a))
	{
	  n = get_num (a);
	  MU_ASSERT (mu_mailbox_get_message (mbx, n, &msg));
	  continue;
	}

      if (strcmp (a, "expunge") == 0)
	{
	  MU_ASSERT (mu_mailbox_expunge (mbx));
	  mu_printf ("expunge: OK\n");
	  continue;
	}

      if (strcmp (a, "sync") == 0)
	{
	  MU_ASSERT (mu_mailbox_sync (mbx));
	  mu_printf ("sync: OK\n");
	  continue;
	}

      if (strcmp (a, "append") == 0)
	{
	  if (argc < 1)
	    {
	      mu_error ("not enough arguments for %s", a);
	      return 1;
	    }
	  dmbox_append (mbx, argv[0]);
	  argc--;
	  argv++;
	  continue;
	}
      
      f = get_action (a, &narg);
      if (!f)
	{
	  mu_error ("%s: unrecognized action", a);
	  return 1;
	}

      if (!msg)
	{
	  mu_error ("no message selected");
	  return 1;
	}

      if (narg > argc)
	{
	  mu_error ("not enough arguments for %s", a);
	  return 1;
	}
      
      mu_printf ("%lu %s: ", (unsigned long) n, a);
      f (msg, argv);
      mu_printf ("\n");

      argc -= narg;
      argv += narg;
    }
  mu_mailbox_close (mbx);
  mu_mailbox_destroy (&mbx);
  return 0;
}
