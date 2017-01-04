/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999, 2001-2002, 2005-2012, 2014-2017 Free Software
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

#include "imap4d.h"
#include <dirent.h>
#include <pwd.h>

struct refinfo
{
  char *refptr;   /* Original reference */
  size_t reflen;  /* Length of the original reference */
  struct namespace_prefix const *pfx;
  size_t pfxlen;
  size_t dirlen;  /* Length of the current directory prefix */
  char *buf;
  size_t bufsize;
};

static int
list_fun (mu_folder_t folder, struct mu_list_response *resp, void *data)
{
  char *name;
  struct refinfo *refinfo = data;
  size_t size;
  char *p;

  if (refinfo->pfx->record && refinfo->pfx->record != resp->format)
    return 0;
  
  name = resp->name;
  size = strlen (name);
  if (size == refinfo->pfxlen + 6
      && memcmp (name + refinfo->pfxlen + 1, "INBOX", 5) == 0)
    return 0;
     
  io_sendf ("* %s", "LIST (");
  if ((resp->type & (MU_FOLDER_ATTRIBUTE_FILE|MU_FOLDER_ATTRIBUTE_DIRECTORY))
       == (MU_FOLDER_ATTRIBUTE_FILE|MU_FOLDER_ATTRIBUTE_DIRECTORY))
    /* nothing */;
  else if (resp->type & MU_FOLDER_ATTRIBUTE_FILE)
    io_sendf ("\\NoInferiors");
  else if (resp->type & MU_FOLDER_ATTRIBUTE_DIRECTORY)
    io_sendf ("\\NoSelect");
  
  io_sendf (") \"%c\" ", refinfo->pfx->delim);

  name = resp->name + refinfo->dirlen + 1;
  size = strlen (name) + refinfo->pfxlen + 2;
  if (size > refinfo->bufsize)
    {
      if (refinfo->buf == NULL)
	{
	  refinfo->bufsize = size;
	  refinfo->buf = malloc (refinfo->bufsize);
	  if (!refinfo->buf)
	    {
	      mu_error ("%s", mu_strerror (errno));
	      return 1;
	    }
	  memcpy (refinfo->buf, refinfo->refptr, refinfo->reflen);
	}
      else
	{
	  char *p = realloc (refinfo->buf, size);
	  if (!p)
	    {
	      mu_error ("%s", mu_strerror (errno));
	      return 1;
	    }
	  refinfo->buf = p;
	  refinfo->bufsize = size;
	}
    }

  if (refinfo->pfxlen)
    {
      p = mu_stpcpy (refinfo->buf, refinfo->pfx->prefix);
      if (*name)
	*p++ = refinfo->pfx->delim;
    }
  else
    p = refinfo->buf;
  if (*name)
    translate_delim (p, name, refinfo->pfx->delim, resp->separator);

  name = refinfo->buf;
  
  if (strpbrk (name, "\"{}"))
    io_sendf ("{%lu}\n%s\n", (unsigned long) strlen (name), name);
  else if (is_atom (name))
    io_sendf ("%s\n", name);
  else
    io_sendf ("\"%s\"\n", name);
  return 0;
}

/*
6.3.8.  LIST Command

   Arguments:  reference name
               mailbox name with possible wildcards

   Responses:  untagged responses: LIST

   Result:     OK - list completed
               NO - list failure: can't list that reference or name
               BAD - command unknown or arguments invalid
*/

/*
  1- IMAP4 insists: the reference argument present in the
  interpreted form SHOULD prefix the interpreted form.  It SHOULD
  also be in the same form as the reference name argument.  This
  rule permits the client to determine if the returned mailbox name
  is in the context of the reference argument, or if something about
  the mailbox argument overrode the reference argument.
  
  ex:
  Reference         Mailbox         -->  Interpretation
  ~smith/Mail        foo.*          -->  ~smith/Mail/foo.*
  archive            %              --> archive/%
  #news              comp.mail.*     --> #news.comp.mail.*
  ~smith/Mail        /usr/doc/foo   --> /usr/doc/foo
  archive            ~fred/Mail     --> ~fred/Mail/ *

  2- The character "*" is a wildcard, and matches zero or more characters
  at this position.  The character "%" is similar to "*",
  but it does not match the hierarchy delimiter.  */

int
imap4d_list (struct imap4d_session *session,
             struct imap4d_command *command, imap4d_tokbuf_t tok)
{
  char *ref;
  char *wcard;

  if (imap4d_tokbuf_argc (tok) != 4)
    return io_completion_response (command, RESP_BAD, "Invalid arguments");
  
  ref = imap4d_tokbuf_getarg (tok, IMAP4_ARG_1);
  wcard = imap4d_tokbuf_getarg (tok, IMAP4_ARG_2);

  /* If wildcard is empty, it is a special case: we have to
     return the hierarchy.  */
  if (*wcard == '\0')
    {
      if (*ref)
	io_untagged_response (RESP_NONE,
			      "LIST (\\NoSelect) \"%c\" \"%c\"",
			      MU_HIERARCHY_DELIMITER,
			      MU_HIERARCHY_DELIMITER);
      else
	io_untagged_response (RESP_NONE,
			      "LIST (\\NoSelect) \"%c\" \"\"",
			      MU_HIERARCHY_DELIMITER);
    }
  
  /* There is only one mailbox in the "INBOX" hierarchy ... INBOX.  */
  else if (mu_c_strcasecmp (ref, "INBOX") == 0
      || (ref[0] == 0 && mu_c_strcasecmp (wcard, "INBOX") == 0))
    {
      io_untagged_response (RESP_NONE, "LIST (\\NoInferiors) NIL INBOX");
    }
  else
    {
      int status;
      mu_folder_t folder;
      mu_url_t url;
      char *cwd;
      char const *dir;
      struct refinfo refinfo;
      size_t i;
      struct namespace_prefix const *pfx;

      if (*wcard == '~')
	{
	  for (i = 1;
	       mu_c_is_class (wcard[i], MU_CTYPE_ALPHA|MU_CTYPE_DIGIT)
		 || wcard[i] == '_'; i++)
	    ;
	  ref = mu_alloc (i + 1);
	  memcpy (ref, wcard, i);
	  ref[i] = 0;
	  wcard += i;
	}
      else
	ref = mu_strdup (ref);
      
      cwd = namespace_translate_name (ref, 0, &pfx);
      if (!cwd)
	{
	  free (ref);
	  return io_completion_response (command, RESP_NO,
				      "The requested item could not be found.");
	}
      free (cwd);
      
      if (*wcard == pfx->delim && ref[0] != '~')
	{
	  /* Absolute Path in wcard, dump the old ref.  */
	  ref[0] = 0;
	}
      
      /* Find the longest directory prefix */
      i = strcspn (wcard, "%*");
      while (i > 0 && wcard[i - 1] != pfx->delim)
	i--;
      /* Append it to the reference */
      if (i)
	{
	  size_t reflen = strlen (ref);
	  int addslash = (reflen > 0
			  && ref[reflen-1] != pfx->delim
			  && *wcard != pfx->delim); 
	  size_t len = i + reflen + addslash;

	  ref = mu_realloc (ref, len);
	  if (addslash)
	    ref[reflen++] = pfx->delim;
	  memcpy (ref + reflen, wcard, i - 1); /* omit the trailing / */
	  ref[len-1] = 0;

	  wcard += i;
	}

      cwd = namespace_translate_name (ref, 0, &pfx);
      if (!cwd)
	{
	  free (ref);
	  return io_completion_response (command, RESP_NO,
				      "The requested item could not be found.");
	}
      
      if (pfx->ns == NS_OTHER
	  && strcmp (ref, pfx->prefix) == 0
	  && *mu_str_skip_cset_comp (wcard, "*%"))
	{
	  /* [A] server MAY return NO to such a LIST command, requiring that a
	     user name be included with the Other Users' Namespace prefix
	     before listing any other user's mailboxes */
	  free (ref);
	  return io_completion_response (command, RESP_NO,
			              "The requested item could not be found.");
	}	  
	
      status = mu_folder_create (&folder, cwd);
      if (status)
	{
	  free (ref);
	  free (cwd);
	  return io_completion_response (command, RESP_NO,
			              "The requested item could not be found.");
	}
      /* Force the right matcher */
      mu_folder_set_match (folder, mu_folder_imap_match);

      memset (&refinfo, 0, sizeof refinfo);

      refinfo.refptr = ref;
      refinfo.reflen = strlen (ref);
      refinfo.pfx = pfx;
      
      mu_folder_get_url (folder, &url);
      mu_url_sget_path (url, &dir);
      refinfo.dirlen = strlen (dir);
	     
      refinfo.pfxlen = strlen (pfx->prefix);

      /* The special name INBOX is included in the output from LIST, if
	 INBOX is supported by this server for this user and if the
	 uppercase string "INBOX" matches the interpreted reference and
	 mailbox name arguments with wildcards as described above.  The
	 criteria for omitting INBOX is whether SELECT INBOX will return
	 failure; it is not relevant whether the user's real INBOX resides
	 on this or some other server. */

      if (!*ref &&
	  (mu_imap_wildmatch (wcard, "INBOX", MU_HIERARCHY_DELIMITER) == 0
	   || mu_imap_wildmatch (wcard, "inbox", MU_HIERARCHY_DELIMITER) == 0))
	io_untagged_response (RESP_NONE, "LIST (\\NoInferiors) NIL INBOX");

      mu_folder_enumerate (folder, NULL, wcard, 0, 0, NULL,
			   list_fun, &refinfo);
      mu_folder_destroy (&folder);
      free (refinfo.buf);
      free (cwd);
      free (ref);
    }

  return io_completion_response (command, RESP_OK, "Completed");
}

