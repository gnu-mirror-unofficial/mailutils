/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2003, 2005, 2006, 2007, 2009, 2010 Free Software
   Foundation, Inc.

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

#include <mh.h>

void
mh_whatnow_env_from_environ (struct mh_whatnow_env *wh)
{
  char *folder = getenv ("mhfolder");

  memset (wh, 0, sizeof (*wh));
  
  wh->file = getenv ("mhdraft");
  wh->msg = getenv ("mhaltmsg");
  wh->draftfile = wh->file;
  wh->editor = getenv ("mheditor");
  wh->prompt = getenv ("mhprompt"); /* extension */
  if (folder)
    {
      wh->anno_field = getenv ("mhannotate");
      if (wh->anno_field)
	{
	  char *p = getenv ("mhmessages");
	  if (!p)
	    wh->anno_field = NULL;
	  else
	    {
	      size_t i;
	      struct mu_wordsplit ws;
	      mh_msgset_t msgset;
	      mu_mailbox_t mbox = mh_open_folder (folder, 0);
	      
	      if (mu_wordsplit (p, &ws,
				MU_WRDSF_DEFFLAGS & ~MU_WRDSF_CESCAPES))
		{
		  mu_error (_("cannot parse mhmessages (%s): %s"), p,
			    mu_wordsplit_strerror (&ws));
		  exit (1);
		}
	      mh_msgset_parse (mbox, &msgset, ws.ws_wordc, ws.ws_wordv, "cur");
	      mu_wordsplit_free (&ws);

	      wh->mbox = mbox;
	      mu_list_create (&wh->anno_list);
	      for (i = 0; i < msgset.count; i++)
		{
		  mu_message_t msg;
		  int rc = mu_mailbox_get_message (mbox, msgset.list[i], &msg);
		  if (rc)
		    {
		      mu_error (_("cannot get message %lu from %s: %s"),
				(unsigned long) msgset.list[i],
				folder,
				mu_strerror (rc));
		      continue;
		    }
		  mu_list_append (wh->anno_list, msg);
		}
	      mh_msgset_free (&msgset);
	      /* FIXME:
		 wh->anno_inplace = getenv ("mhinplace");
	      */
	    }
	}
    }
}

void
mh_whatnow_env_to_environ (struct mh_whatnow_env *wh)
{
  if (wh->file)
    setenv ("mhdraft", wh->file, 1);
  if (wh->msg)
    setenv ("mhaltmsg", wh->msg, 1);
  if (wh->editor)
    setenv ("mheditor", wh->editor, 1);
  if (wh->prompt)
    setenv ("mhprompt", wh->prompt, 1);
  if (wh->anno_field)
    setenv ("mhannotate", wh->anno_field, 1);
  if (wh->anno_list)
    {
      mu_opool_t opool;
      mu_iterator_t itr;
      size_t prev_uid = 0;
      int mrange = 0;
      const char *s;
      
      mu_opool_create (&opool, 1);
      mu_list_get_iterator (wh->anno_list, &itr);
      for (mu_iterator_first (itr); !mu_iterator_is_done (itr);
	   mu_iterator_next (itr))
	{
	  mu_message_t msg;
	  size_t uid;
	  
	  mu_iterator_current (itr, (void**)&msg);
	  mu_message_get_uid (msg, &uid);
	  if (prev_uid == 0)
	    {
	      s = mu_umaxtostr (0, uid);
	      mu_opool_append (opool, s, strlen (s));
	      mrange = 0;
	    }
	  else if (uid == prev_uid + 1)
	    mrange = 1;
	  else
	    {
	      if (mrange)
		{
		  mu_opool_append_char (opool, '-');
		  s = mu_umaxtostr (0, prev_uid);
		  mu_opool_append (opool, s, strlen (s));
		}
	      mu_opool_append_char (opool, ' ');
	      s = mu_umaxtostr (0, uid);
	      mu_opool_append (opool, s, strlen (s));
	      mrange = 0;
	    }
	}

      if (mrange)
	{
	  mu_opool_append_char (opool, '-');
	  s = mu_umaxtostr (0, prev_uid);
	  mu_opool_append (opool, s, strlen (s));
	}
      mu_opool_append_char (opool, 0);
      s = mu_opool_finish (opool, NULL);
      setenv ("mhmessages", s, 1);
      mu_opool_destroy (&opool);
    }
}