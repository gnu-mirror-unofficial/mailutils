/* Content-Type (RFC 2045) parser for GNU Mailutils
   Copyright (C) 2016-2020 Free Software Foundation, Inc.

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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <mailutils/types.h>
#include <mailutils/mime.h>
#include <mailutils/assoc.h>
#include <mailutils/util.h>
#include <mailutils/errno.h>
#include <mailutils/opool.h>
#include <mailutils/cctype.h>
#include <mailutils/cstr.h>

/* Parse the content type header value in INPUT.  If CHARSET is not
   NULL, convert textual parameters to this charset.

   Store the result in CT.

   In case of error, CT is left partially constructed.  The caller
   must free it.
   
   Parsing of the type/subtype value is relaxed: any characters are
   allowed in either part (except for "/", which can't appear in type).
   Although RFC 2045 forbids that, mails with such content types reportedly
   exist (see conversation with Karl Berry on 2020-07-21, particularly
   <202007220115.06M1FuTh001462@freefriends.org> and my reply
   <20200722133251.8412@ulysses.gnu.org.ua>).

   Type must not be empty, but empty subtype is allowed.
*/   
static int
content_type_parse (const char *input, const char *charset,
		    mu_content_type_t ct)
{
  int rc;
  char *value, *p;

  rc = mu_mime_header_parse (input, charset, &value, &ct->param);
  if (rc)
    return rc;

  p = strchr (value, '/');
  if (p)
    {
      size_t len = p - value;
      while (len > 0 && mu_isspace (value[len-1]))
	len--;
      if (len == 0)
	{
	  rc = MU_ERR_PARSE;
	  goto end;
	}
      
      p = mu_str_skip_class (p + 1, MU_CTYPE_SPACE);
      
      ct->type = malloc (len + 1);
      if (!ct->type)
	{
	  rc = errno;
	  goto end;
	}
      
      memcpy (ct->type, value, len);
      ct->type[len] = 0;

      ct->subtype = strdup (p);
      if (!ct->subtype)
	rc = errno;
    }
  else
    rc = MU_ERR_PARSE;
 end:
  free (value);
  return rc;
}

int
mu_content_type_parse (const char *input, const char *charset,
		       mu_content_type_t *retct)
{
  int rc;
  mu_content_type_t ct;

  if (!input)
    return EINVAL;
  if (!retct)
    return MU_ERR_OUT_PTR_NULL;
  
  ct = calloc (1, sizeof (*ct));
  if (!ct)
    return errno;
  rc = content_type_parse (input, charset, ct);
  if (rc)
    mu_content_type_destroy (&ct);
  else
    *retct = ct;

  return rc;
}

void
mu_content_type_destroy (mu_content_type_t *pptr)
{
  if (pptr && *pptr)
    {
      mu_content_type_t ct = *pptr;
      free (ct->type);
      free (ct->subtype);
      free (ct->trailer);
      mu_assoc_destroy (&ct->param);
      free (ct);
      *pptr = NULL;
    }
}

static int
format_param (char const *name, void *data, void *call_data)
{
  struct mu_mime_param *param = data;
  char *value = param->value;
  mu_opool_t pool = call_data;
  char *cp;

  /* Content-Type parameters don't use lang and cset fields of
     struct mu_mime_param, so these are ignored. */
  mu_opool_append (pool, "; ", 2);
  mu_opool_appendz (pool, name);
  mu_opool_append_char (pool, '=');
  if (*(cp = mu_str_skip_class_comp (value, MU_CTYPE_TSPEC)))
    {
      mu_opool_append_char (pool, '"');
      while (*(cp = mu_str_skip_cset_comp (value, "\\\"")))
	{
	  mu_opool_append (pool, value, cp - value);
	  mu_opool_append_char (pool, '\\');
	  mu_opool_append_char (pool, *cp);
	  value = cp + 1;
	}
      if (*value)
	mu_opool_appendz (pool, value);
      mu_opool_append_char (pool, '"');
    }
  else
    mu_opool_appendz (pool, value);
  return 0;
}

int
mu_content_type_format (mu_content_type_t ct, char **return_ptr)
{
  int rc;
  mu_opool_t pool;
  mu_nonlocal_jmp_t jmp;

  if (!ct)
    return EINVAL;
  if (!return_ptr)
    return MU_ERR_OUT_PTR_NULL;

  rc = mu_opool_create (&pool, MU_OPOOL_DEFAULT);
  if (rc)
    return rc;

  if ((rc = setjmp (jmp.buf)) != 0)
    {
      mu_opool_destroy (&pool);
      return rc;
    }
  mu_opool_setjmp (pool, &jmp);

  mu_opool_appendz (pool, ct->type);
  mu_opool_append_char (pool, '/');
  mu_opool_appendz (pool, ct->subtype);
  if (!mu_assoc_is_empty (ct->param))
    mu_assoc_foreach (ct->param, format_param, pool);
  mu_opool_append_char (pool, 0);
  *return_ptr = mu_opool_detach (pool, NULL);
  mu_opool_clrjmp (pool);
  mu_opool_destroy (&pool);
  return 0;
}
