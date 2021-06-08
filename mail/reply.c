/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2021 Free Software Foundation, Inc.

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

void
make_in_reply_to (compose_env_t *env, mu_message_t msg)
{
  char *value = NULL;

  mu_rfc2822_in_reply_to (msg, &value);
  compose_header_set (env, MU_HEADER_IN_REPLY_TO, value,
		      COMPOSE_REPLACE);
  free (value);
}

void
make_references (compose_env_t *env, mu_message_t msg)
{
  char *value = NULL;

  mu_rfc2822_references (msg, &value);
  compose_header_set (env, MU_HEADER_REFERENCES, value, COMPOSE_REPLACE);
  free (value);
}
  
/*
 * r[eply] [message]
 * r[espond] [message]
 *   Reply to all recipients of the message.  Save to record.
 *   reply_all = 1, send_to = 0
 * R[eply] [msglist]
 * R[espond] [msglist]
 *   Reply to the sender of each message in msglist.  Save to record.
 *   reply_all = 0, send_to = 0
 * fo[llowup] message
 *   Reply to all recipients of the message.  Save by name.
 *   reply_all = 1, send_to = 1 
 * F[ollowup] msglist
 *   Reply to the sender of each message in msglist.  Save by name.
 *   reply_all = 0, send_to = 1
 */

static void
compose_set_address (compose_env_t *env, char const *hname, char const *input)
{
  struct mu_address hint = MU_ADDRESS_HINT_INITIALIZER;
  mu_address_t iaddr, oaddr = NULL, ap;
  char *result = NULL;
  
  if (mu_address_create_hint (&iaddr, input, &hint, 0))
    result = mu_strdup (input);
  else
    {
      for (ap = iaddr; ap; ap = ap->next)
	{
	  const char *email;
	  if (mu_address_sget_email (ap, 1, &email) || email == NULL)
	    continue;
	  if (!(mailvar_is_true (mailvar_name_metoo) &&
		mail_is_my_name (email)))
	    mu_address_union (&oaddr, ap);
	}
      mu_address_destroy (&iaddr);
      mu_address_aget_printable (oaddr, &result);
      mu_address_destroy (&oaddr);
    }
  if (result && result[0])
    {
      compose_header_set (env, hname, result, COMPOSE_SINGLE_LINE);
      free (result);
    }
}

/*
 * r[eply] [message]
 * r[espond] [message]
 * fo[llowup] message
 *
 * Reply to all recipients of a single message
 */
int
respond_all (int argc, char **argv, int record_sender)
{
  int status;
  compose_env_t env;
  mu_message_t msg;
  mu_header_t hdr;
  char const *str;
  char *p;

  msgset_t *msgset = NULL;

  if (msgset_parse (argc, argv, MSG_NODELETED, &msgset))
    return 1;
  
  if (msgset->next)
    {
      mu_error (_("Can't reply to multiple messages at once"));
      status = 1;
    }
  else if (util_get_message (mbox, msgset_msgno (msgset), &msg))
    {
      status = 1;
    }
  else
    {
      set_cursor (msgset_msgno (msgset));
      
      mu_message_get_header (msg, &hdr);
  
      compose_init (&env);

      p = util_message_sender (msg, 0);
      if (p)
	{
	  compose_set_address (&env, MU_HEADER_TO, p);
	  free (p);
	}
      
      /* Add the rest of recipients */
      if (mu_header_sget_value (hdr, MU_HEADER_TO, &str) == 0)
	{
	  compose_set_address (&env, MU_HEADER_TO, str);
	}
      
      if (mu_header_sget_value (hdr, MU_HEADER_CC, &str) == 0)
	{
	  compose_set_address (&env, MU_HEADER_CC, str);
	}
      
      /* Add header line */
      //FIXME: decode
      if (mu_header_aget_value (hdr, MU_HEADER_SUBJECT, &p) == 0)
	{
	  char *subj = NULL;
	  
	  if (mu_unre_subject (p, NULL))
	    util_strcat (&subj, util_reply_prefix ());
	  util_strcat (&subj, p);
	  free (p);
	  compose_header_set (&env, MU_HEADER_SUBJECT, subj, COMPOSE_REPLACE);
	  free (subj);
	}
      else
	compose_header_set (&env, MU_HEADER_SUBJECT, "", COMPOSE_REPLACE);

      mu_printf ("To: %s\n", compose_header_get (&env, MU_HEADER_TO, ""));
      str = compose_header_get (&env, MU_HEADER_CC, NULL);
      if (str)
	mu_printf ("Cc: %s\n", str);
      mu_printf ("Subject: %s\n\n",
		 compose_header_get (&env, MU_HEADER_SUBJECT, ""));
      
      make_in_reply_to (&env, msg);
      make_references (&env, msg);
      status = mail_compose_send (&env, record_sender);
      compose_destroy (&env);
      util_mark_read (msg);
    }
  msgset_free (msgset);

  return status;
}

/*
 * R[eply] [msglist]
 * R[espond] [msglist]
 * F[ollowup] msglist
 *
 * Reply to the sender of each message in msglist.
 */
int
respond_msg (int argc, char **argv, int record_sender)
{
  mu_message_t msg;
  mu_header_t hdr;
  compose_env_t env;
  int status;
  char *p;
  char const *str;

  msgset_t *msgset = NULL, *mp;

  if (msgset_parse (argc, argv, MSG_NODELETED, &msgset))
    return 1;
  
  if (util_get_message (mbox, msgset_msgno (msgset), &msg))
    {
      status = 1;
    }
  else
    {
      size_t last;

      set_cursor (msgset_msgno (msgset));

      mu_message_get_header (msg, &hdr);

      compose_init (&env);

      for (mp = msgset; mp; mp = mp->next)
	{
	  mu_message_t mesg;
	  last = msgset_msgno (mp);
	  if (util_get_message (mbox, last, &mesg) == 0)
	    {
	      p = util_message_sender (mesg, 0);
	      if (p)
		{
		  compose_set_address (&env, MU_HEADER_TO, p);
		  free (p);
		}
	      util_mark_read (mesg);
	    }
	}
      
      /* Add subject header */
      if (mu_header_aget_value (hdr, MU_HEADER_SUBJECT, &p) == 0)
	{
	  char *subj = NULL;
      
	  if (mu_unre_subject (p, NULL))
	    util_strcat (&subj, util_reply_prefix ());
	  util_strcat (&subj, p);
	  free (p);
	  compose_header_set (&env, MU_HEADER_SUBJECT, subj, COMPOSE_REPLACE);
	  free (subj);
	}
      else
	compose_header_set (&env, MU_HEADER_SUBJECT, "", COMPOSE_REPLACE);
      
      mu_printf ("To: %s\n", compose_header_get (&env, MU_HEADER_TO, ""));
      str = compose_header_get (&env, MU_HEADER_CC, NULL);
      if (str)
	mu_printf ("Cc: %s\n", str);
      mu_printf ("Subject: %s\n\n",
		 compose_header_get (&env, MU_HEADER_SUBJECT, ""));
  
      make_in_reply_to (&env, msg);
      make_references (&env, msg);
      status = mail_compose_send (&env, record_sender);
      compose_destroy (&env);

      set_cursor (last);
    }
  msgset_free (msgset);
  
  return status;  
}

int
mail_reply (int argc, char **argv)
{
  int all = mu_islower (argv[0][0]);
  if (mailvar_is_true (mailvar_name_flipr))
    all = !all;
  return (all ? respond_all : respond_msg) (argc, argv, 0);
}
  
int
mail_followup (int argc, char **argv)
{
  int all = mu_islower (argv[0][0]);
  return (all ? respond_all : respond_msg) (argc, argv, 1);
}  
