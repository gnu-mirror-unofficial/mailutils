/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999, 2001, 2005, 2007-2012, 2014-2017 Free Software
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
#include <mailutils/assoc.h>

struct namespace namespace[NS_MAX] = {
  [NS_PRIVATE] = { "private" },
  [NS_OTHER]   = { "other" },
  [NS_SHARED]  = { "shared" }
};
  
static mu_assoc_t prefixes;

struct namespace *
namespace_lookup (char const *name)
{
  size_t i;
  
  for (i = 0; i < NS_MAX; i++)
    if (strcmp (namespace[i].name, name) == 0)
      {
	if (!namespace[i].prefixes)
	  {
	    int rc = mu_list_create (&namespace[i].prefixes);
	    if (rc)
	      {
		mu_diag_funcall (MU_DIAG_ERROR, "mu_list_create", NULL, rc);
		abort ();
	      }
	  }
	return &namespace[i];
      }
  return NULL;
}

static int
cmplen (const char *aname, const void *adata,
	const char *bname, const void *bdata,
	void *closure)
{
  return strlen (bname) - strlen (aname);
}

void
translate_delim (char *dst, char const *src, int dst_delim, int src_delim)
{
  do
    *dst++ = *src == src_delim ? dst_delim : *src;
  while (*src++);
}  

static void
trim_delim (char *str, int delim)
{
  size_t len = strlen (str);
  while (len && str[len-1] == '/')
    len--;
  str[len] = 0;
}

void
namespace_init (void)
{
  int i;
  int rc;
  struct namespace_prefix *pfx;
  
  if (mu_assoc_create (&prefixes, 0))
    imap4d_bye (ERR_NO_MEM);
  for (i = 0; i < NS_MAX; i++)
    {
      mu_iterator_t itr;
      
      if (mu_list_is_empty (namespace[i].prefixes))
	continue;
      
      if (mu_list_get_iterator (namespace[i].prefixes, &itr))
	imap4d_bye (ERR_NO_MEM);
      
	
      for (mu_iterator_first (itr);
	   !mu_iterator_is_done (itr); mu_iterator_next (itr))
	{
	  rc = mu_iterator_current (itr, (void **)&pfx);
	  if (rc)
	    {
	      mu_diag_funcall (MU_DIAG_ERROR, "mu_iterator_current", NULL, rc);
	      continue;
	    }
	  pfx->ns = i;

	  trim_delim (pfx->prefix, pfx->delim);
	  trim_delim (pfx->dir, '/');
	  
	  rc = mu_assoc_install (prefixes, pfx->prefix, pfx);
	  if (rc == MU_ERR_EXISTS)
	    {
	      mu_error (_("%s: namespace prefix appears twice"), pfx->prefix);
	      exit (EX_CONFIG);
	    }
	  else if (rc)
	    {
	      mu_error (_("%s: can't install prefix: %s"),
			pfx->prefix, mu_strerror (rc));
	      exit (EX_CONFIG);
	    }
	}
    }

  pfx = mu_assoc_get (prefixes, "");
  if (pfx)
    {
      if (pfx->ns != NS_PRIVATE)
	{
	  mu_error (_("empty prefix not allowed in the namespace %s"),
		    namespace[pfx->ns].name);
	  exit (EX_CONFIG);
	}
    }
  else
    {
      struct namespace *priv;
      
      pfx = mu_zalloc (sizeof (*pfx));
      pfx->prefix = mu_strdup ("");
      pfx->dir = mu_strdup ("$home");
      pfx->delim = '/';
      priv = namespace_lookup ("private");
      mu_list_prepend (priv->prefixes, pfx);
      rc = mu_assoc_install (prefixes, pfx->prefix, pfx);
      if (rc)
	{
	  mu_error (_("%s: can't install prefix: %s"),
		    pfx->prefix, mu_strerror (rc));
	  exit (EX_CONFIG);
	}
    }
  
  mu_assoc_sort_r (prefixes, cmplen, NULL);
}
      
static int
expand_vars (char **env, char const *input, char **output)
{
  struct mu_wordsplit ws;
  size_t wordc;
  char **wordv;
  
  ws.ws_env = (const char **) env;
  if (mu_wordsplit (input, &ws,
		    MU_WRDSF_NOSPLIT | MU_WRDSF_NOCMD |
		    MU_WRDSF_ENV | MU_WRDSF_ENV_KV))
    {
      mu_error (_("cannot expand line `%s': %s"), input,
		mu_wordsplit_strerror (&ws));
      return 1;
    }
  mu_wordsplit_get_words (&ws, &wordc, &wordv);
  *output = wordv[0];
  mu_wordsplit_free (&ws);
  return 0;
}

static char *
prefix_translate_name (struct namespace_prefix const *pfx, char const *name,
		       size_t namelen, int url)
{
  size_t pfxlen = strlen (pfx->prefix);

  if (pfxlen <= namelen
      && memcmp (pfx->prefix, name, pfxlen) == 0
      && (pfxlen == 0 || pfxlen == namelen || name[pfxlen] == pfx->delim))
    {
      char *tmpl, *p;

      if (!pfx->scheme)
	url = 0;
      name += pfxlen;

      if (pfx->ns == NS_PRIVATE && strcmp (name, "INBOX") == 0)
	{
	  tmpl = mu_strdup (auth_data->mailbox);
	  return tmpl;//FIXME
	}
      
      tmpl = mu_alloc (namelen - pfxlen + strlen (pfx->dir)
		       + (url ? strlen (pfx->scheme) + 3 : 0)
		       + 2);
      if (url)
	{
	  p = mu_stpcpy (tmpl, pfx->scheme);
	  p = mu_stpcpy (p, "://");
	}
      else
	p = tmpl;
      
      p = mu_stpcpy (p, pfx->dir);
      if (*name)
	{
	  *p++ = '/';
	  translate_delim (p, name, '/', pfx->delim);
	}

      return tmpl;
    }
  return NULL;
}

static char *
i_translate_name (char const *name, int url, int ns,
		  struct namespace_prefix const **return_pfx)
{
  mu_iterator_t itr;
  int rc;
  char *res = NULL;
  size_t namelen = strlen (name);

#if 0
  FIXME
   if (namelen >= 5
      && strcmp (name + namelen - 5, "INBOX") == 0
      && (namelen == 5 && name[namelen - 6] == '/'))
    {
    } 
#endif
  
  rc = mu_assoc_get_iterator (prefixes, &itr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_assoc_get_iterator", NULL, rc);
      return NULL;
    }
  for (mu_iterator_first (itr);
       !mu_iterator_is_done (itr); mu_iterator_next (itr))
    {
      struct namespace_prefix *pfx;
      
      rc = mu_iterator_current (itr, (void **)&pfx);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_iterator_current", NULL, rc);
	  continue;
	}

      if (ns != NS_MAX && ns != pfx->ns)
	continue;
      
      res = prefix_translate_name (pfx, name, namelen, url);
      if (res)
	{
	  if (return_pfx)
	    *return_pfx = pfx;
	  break;
	}
    }
  mu_iterator_destroy (&itr);

  return res;
}

static char *
extract_username (char const *name, struct namespace_prefix const *pfx)
{
  char const *p = name + strlen (pfx->prefix);
  char *end = strchr (p, pfx->delim);
  char *user;
  size_t len;
  
  if (end)
    len = end - p;
  else
    len = strlen (p);
  
  user = mu_alloc (len + 1);
  memcpy (user, p, len);
  user[len] = 0;
  return user;
}

enum
  {
    ENV_USER = 1,
    ENV_HOME = 3,
    ENV_NULL = 4
  };

#define ENV_INITIALIZER \
  { [ENV_USER-1] = "user", [ENV_HOME-1] = "home", [ENV_NULL] = NULL }

char *
namespace_translate_name (char const *name, int url,
			  struct namespace_prefix const **return_pfx)
{
  char *res = NULL;
  struct namespace_prefix const *pfx;
  char *env[] = ENV_INITIALIZER;
  
  if (name[0] == '~')
    {
      if (name[1] == 0 || name[1] == '/')
	{
	  if (mu_c_strcasecmp (name, "INBOX") == 0 && auth_data->change_uid)
	    res = mu_strdup (auth_data->mailbox);
	  else
	    res = i_translate_name (name, url, NS_PRIVATE, &pfx);
	}
      else 
	res = i_translate_name (name, url, NS_OTHER, &pfx);
    }
  else
    res = i_translate_name (name, url, NS_MAX, &pfx);

  if (res)
    {
      char *dir;
      
      switch (pfx->ns)
	{
	case NS_PRIVATE:
	  env[ENV_USER] = auth_data->name;
	  env[ENV_HOME] = real_homedir;
	  break;

	case NS_SHARED:
	  {
	    struct mu_auth_data *adata;
	    env[ENV_USER] = extract_username (name, pfx);
	    adata = mu_get_auth_by_name (env[ENV_USER]);
	    if (adata)
	      {
		env[ENV_HOME] = mu_strdup (adata->dir);
		mu_auth_data_free (adata);
	      }
	  }
	  break;

	case NS_OTHER:
	  break;
	}
      
      if (expand_vars (env, res, &dir))
	imap4d_bye (ERR_NO_MEM);
      free (res);
      res = dir;
      trim_delim (res, '/');
      
      if (pfx->ns == NS_OTHER)
	{
	  free (env[ENV_USER]);
	  free (env[ENV_HOME]);
	}
      
      if (return_pfx)
	*return_pfx = pfx;
    }
  return res;
}

char *
namespace_get_url (char const *name, int *mode)
{
  struct namespace_prefix const *pfx;
  char *path = namespace_translate_name (name, 1, &pfx);

  if (path && pfx->scheme)
    {
      char *p = mu_alloc (strlen (pfx->scheme) + 3 + strlen (path) + 1);
      strcpy (p, pfx->scheme);
      strcat (p, "://");
      strcat (p, path);
      free (path);
      path = p;
    }
  if (mode)
    *mode = namespace[pfx->ns].mode;
  return path;
}

static int
prefix_printer(void *item, void *data)
{
  struct namespace_prefix *pfx = item;
  int *first = data;

  if (*first)
    *first = 0;
  else
    io_sendf (" ");
  io_sendf ("(\"%s\" \"%c\")", pfx->prefix, pfx->delim);
  return 0;
}

static void
print_namespace (int ns)
{
  if (mu_list_is_empty (namespace[ns].prefixes))
    io_sendf ("NIL");
  else
    {
      int first = 1;
      io_sendf ("(");
      mu_list_foreach (namespace[ns].prefixes, prefix_printer, &first);
      io_sendf (")");
    }
}

/*
5. NAMESPACE Command

   Arguments: none

   Response:  an untagged NAMESPACE response that contains the prefix
                 and hierarchy delimiter to the server's Personal
                 Namespace(s), Other Users' Namespace(s), and Shared
                 Namespace(s) that the server wishes to expose. The
                 response will contain a NIL for any namespace class
                 that is not available. Namespace_Response_Extensions
                 MAY be included in the response.
                 Namespace_Response_Extensions which are not on the IETF
                 standards track, MUST be prefixed with an "X-".
*/

int
imap4d_namespace (struct imap4d_session *session,
                  struct imap4d_command *command, imap4d_tokbuf_t tok)
{
  if (imap4d_tokbuf_argc (tok) != 2)
    return io_completion_response (command, RESP_BAD, "Invalid arguments");

  io_sendf ("* NAMESPACE ");

  print_namespace (NS_PRIVATE);
  io_sendf (" ");
  print_namespace (NS_OTHER);
  io_sendf (" ");
  print_namespace (NS_SHARED);
  io_sendf ("\n");

  return io_completion_response (command, RESP_OK, "Completed");
}
