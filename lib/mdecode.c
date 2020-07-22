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

#include <config.h>
#include <mailutils/mailutils.h>
#include <mailutils/imaputil.h>

static mu_list_t text_mime_list;

static char const *default_text_types[] = {
  "text/*",
  "application/*shell",
  "application/shellscript",
  "*/x-csrc",
  "*/x-csource",
  "*/x-diff",
  "*/x-patch",
  "*/x-perl",
  "*/x-php",
  "*/x-python",
  "*/x-sh",
  NULL
};

static void text_mime_add (char const *s);

struct mime_pattern
{
  char *pat_type;
  char *pat_subtype;
};

static int
text_mime_cmp (const void *item, const void *ptr)
{
  const struct mime_pattern *pat = item;
  mu_content_type_t ct = (mu_content_type_t) ptr;
  if (mu_imap_wildmatch_ci (pat->pat_type, ct->type, 0) == 0
      && (pat->pat_subtype == NULL
	  || mu_imap_wildmatch (pat->pat_subtype, ct->subtype, '/') == 0))
    return 0;
  return 1;
} 

static void
text_mime_init (void)
{
  if (!text_mime_list)
    {
      int i;
      int rc = mu_list_create (&text_mime_list);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_list_create", NULL, rc);
	  mu_alloc_die ();
	}
      mu_list_set_destroy_item (text_mime_list, mu_list_free_item);
      mu_list_set_comparator (text_mime_list, text_mime_cmp);
      
      for (i = 0; default_text_types[i]; i++)
	text_mime_add (default_text_types[i]);
    }
}

static void
text_mime_add (char const *s)
{
  int rc;
  struct mime_pattern *mp;
  char *p;
  
  text_mime_init ();

  mp = mu_alloc (sizeof *mp + strlen (s) + 1);
  mp->pat_type = (char*)(mp + 1);
  strcpy (mp->pat_type, s);
  p = strchr (mp->pat_type, '/');
  if (p)
    *p++ = 0;
  mp->pat_subtype = p;
  rc = mu_list_append (text_mime_list, mp);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_list_append", NULL, rc);
      mu_alloc_die ();
    }
}

static int
cb_text_type (void *data, mu_config_value_t *val)
{
  size_t i;
  
  switch (val->type)
    {
    case MU_CFG_STRING:
      text_mime_add (val->v.string);
      break;

    case MU_CFG_LIST:
      for (i = 0; i < val->v.arg.c; i++)
	{
	  if (mu_cfg_assert_value_type (&val->v.arg.v[i], MU_CFG_STRING))
	    return 1;
	  text_mime_add (val->v.arg.v[i].v.string);
	}
      break;

    default:
      mu_error ("%s", _("expected string or list"));
    }
  return 0;
}

static struct mu_cfg_param mime_param[] = {
  { "text-type", mu_cfg_callback, NULL, 0, cb_text_type,
    N_("Define textual mime types."),
    N_("arg: pattern or list of patterns") },
  { NULL }
};

struct mu_cli_capa mu_cli_capa_mime = {
  .name = "mime",
  .cfg = mime_param
};

static int
is_text_part (mu_content_type_t ct)
{
  text_mime_init ();
  return mu_list_locate (text_mime_list, ct, NULL) == 0;
}

static int
charset_setup (mu_stream_t *pstr, mu_content_type_t ct, char const *charset)
{
  struct mu_mime_param *param;
  if (charset
      && mu_assoc_lookup (ct->param, "charset", &param) == 0
      && mu_c_strcasecmp (param->value, charset))
    {
      mu_stream_t input = *pstr;
      char const *argv[] = { "iconv", NULL, NULL, NULL };
      mu_stream_t flt;
      int rc;
      
      argv[1] = param->value;
      argv[2] = charset;
      rc = mu_filter_chain_create (&flt, input,
				   MU_FILTER_ENCODE,
				   MU_STREAM_READ,
				   MU_ARRAY_SIZE (argv) - 1,
				   (char**) argv);
      if (rc)
	{
	  mu_error (_("can't convert from charset %s to %s: %s"),
		    param->value, charset, mu_strerror (rc));
	  return rc;
	}
      mu_stream_unref (input);
      *pstr = flt;
    }
  return 0;
}

int
message_body_stream (mu_message_t msg, int unix_header, char const *charset,
		     mu_stream_t *pstr)
{
  int rc;
  mu_header_t hdr;
  mu_body_t body;
  mu_stream_t d_stream;
  mu_stream_t stream = NULL;
  char *encoding = NULL;
  char *buf;
  mu_content_type_t ct;
  
  /* Get the headers. */
  rc = mu_message_get_header (msg, &hdr);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_header", NULL, rc);
      return rc;
    }

  /* Read and parse the Content-Type header. */
  rc = mu_header_aget_value_unfold (hdr, MU_HEADER_CONTENT_TYPE, &buf);
  if (rc == MU_ERR_NOENT)
    {
      buf = strdup ("text/plain");
      if (!buf)
	return errno;
    }
  else if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_header_aget_value_unfold", NULL, rc);
      return rc;
    }

  rc = mu_content_type_parse (buf, NULL, &ct);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_content_type_parse", buf, rc);
      free (buf);
      return rc;
    }
  free (buf);
  buf = NULL;

  if (is_text_part (ct))
    /* Process only textual parts */
    {
      /* Get the body stream. */
      rc = mu_message_get_body (msg, &body);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_message_get_body", NULL, rc);
	  goto err;
	}
  
      rc = mu_body_get_streamref (body, &stream);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_body_get_streamref", NULL, rc);
	  goto err;
	}

      /* Filter it through the appropriate decoder. */
      rc = mu_header_aget_value_unfold (hdr,
					MU_HEADER_CONTENT_TRANSFER_ENCODING,
					&encoding);
      if (rc == 0)
	mu_rtrim_class (encoding, MU_CTYPE_SPACE);
      else if (rc == MU_ERR_NOENT)
	encoding = NULL;
      else if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_header_aget_value_unfold",
			   NULL, rc);
	  goto err;
	}

      if (encoding == NULL || *encoding == '\0')
	/* No need to filter */;
      else if ((rc = mu_filter_create (&d_stream, stream, encoding,
				       MU_FILTER_DECODE, MU_STREAM_READ)) == 0)
	{
	  mu_stream_unref (stream);
	  stream = d_stream;
	}
      else
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_filter_create", encoding, rc);
	  /* FIXME: continue anyway? */
	}

      /* Convert the content to the requested charset. */
      rc = charset_setup (&stream, ct, charset);
      if (rc)
	goto err;

      if (unix_header)
	{
	  rc = mu_filter_create (&d_stream, stream, "FROM",
				 MU_FILTER_ENCODE, MU_STREAM_READ);
	  if (rc == 0)
	    {
	      mu_stream_unref (stream);
	      stream = d_stream;
	    }
	  else
	    {
	      mu_diag_funcall (MU_DIAG_ERROR, "mu_filter_create", "FROM", rc);
	      /* continue anyway */
	    }
	}
    }
  else
    rc = MU_ERR_USER0;
 err:
  free (buf);
  free (encoding);
  mu_content_type_destroy (&ct);
  if (rc == 0)
    *pstr = stream;
  else
    mu_stream_destroy (&stream);
  return rc;
}
