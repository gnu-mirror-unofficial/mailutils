/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2011-2020 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library.  If not, see
   <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <mailutils/types.h>
#include <mailutils/assoc.h>
#include <mailutils/list.h>
#include <mailutils/message.h>
#include <mailutils/mime.h>
#include <mailutils/header.h>
#include <mailutils/sys/message.h>
#include <mailutils/errno.h>
#include <mailutils/debug.h>
#include <mailutils/nls.h>
#include <mailutils/cstr.h>
#include <mailutils/body.h>
#include <mailutils/util.h>

void
mu_list_free_bodystructure (void *item)
{
  mu_bodystructure_free (item);
}

void
mu_bodystructure_free (struct mu_bodystructure *bs)
{
  if (!bs)
    return;
  free (bs->body_type);
  free (bs->body_subtype);
  mu_assoc_destroy (&bs->body_param);
  free (bs->body_id);
  free (bs->body_descr);
  free (bs->body_encoding);
  free (bs->body_md5);
  free (bs->body_disposition);
  mu_assoc_destroy (&bs->body_disp_param);
  free (bs->body_language);
  free (bs->body_location);
  switch (bs->body_message_type)
    {
    case mu_message_other:
    case mu_message_text:
      break;
      
    case mu_message_rfc822:
      mu_message_imapenvelope_free (bs->v.rfc822.body_env);
      mu_bodystructure_free (bs->v.rfc822.body_struct);
      break;
      
    case mu_message_multipart:
      mu_list_destroy (&bs->v.multipart.body_parts);
    }

  free (bs);
}

static int bodystructure_fill (mu_message_t msg,
			       struct mu_bodystructure *bs);

static int
bodystructure_init (mu_message_t msg, struct mu_bodystructure **pbs)
{
  int rc;
  struct mu_bodystructure *bs = calloc (1, sizeof (*bs));
  if (!bs)
    return ENOMEM;
  rc = bodystructure_fill (msg, bs);
  if (rc)
    mu_bodystructure_free (bs);
  else
    *pbs = bs;
  return rc;
}

static int
bodystructure_fill (mu_message_t msg, struct mu_bodystructure *bs)
{
  mu_header_t header = NULL;
  char *buffer = NULL;
  mu_body_t body = NULL;
  int is_multipart = 0;
  int rc;

  rc = mu_message_get_header (msg, &header);
  if (rc)
    return rc;
  
  if (mu_header_aget_value_unfold (header, MU_HEADER_CONTENT_TYPE, &buffer) == 0)
    {
      mu_content_type_t ct;

      rc = mu_content_type_parse (buffer, "UTF-8", &ct);
      if (rc == 0)
	{
	  if (mu_c_strcasecmp (ct->type, "MESSAGE") == 0 &&
	      mu_c_strcasecmp (ct->subtype, "RFC822") == 0)
	    bs->body_message_type = mu_message_rfc822;
	  else if (mu_c_strcasecmp (ct->type, "TEXT") == 0)
	    bs->body_message_type = mu_message_text;
	  
	  bs->body_type = ct->type;
	  ct->type = NULL;
	  mu_strupper (bs->body_type);
	  bs->body_subtype = ct->subtype;
	  ct->subtype = NULL;
	  mu_strupper (bs->body_subtype);
	  bs->body_param = ct->param;
	  ct->param = NULL;
	  mu_content_type_destroy (&ct);
      
	  /* body parameter parenthesized list: Content-type attributes */
	  mu_message_is_multipart (msg, &is_multipart);
	  if (is_multipart)
	    bs->body_message_type = mu_message_multipart;
	}
      free (buffer);
    }
  else
    {
      struct mu_mime_param *param;
      
      /* Default? If Content-Type is not present consider as text/plain.  */
      bs->body_type = strdup ("TEXT");
      if (!bs->body_type)
	return ENOMEM;
      bs->body_subtype = strdup ("PLAIN");
      if (!bs->body_subtype)
        {
          free (bs->body_type);
	  return ENOMEM;
	}
      rc = mu_mime_param_assoc_create (&bs->body_param);
      if (rc)
	return rc;
      param = calloc (1, sizeof (*param));
      if (param && (param->value = strdup ("US-ASCII")) != NULL)
	{
	  rc = mu_assoc_install (bs->body_param, "CHARSET", param);
	  if (rc)
	    {
	      mu_mime_param_free (param);
	      return rc;
	    }
	  bs->body_message_type = mu_message_text;
	}
      else
	{
	  free (param);
	  return ENOMEM;
	} 
    }

  if (is_multipart)
    {
      size_t i, nparts;

      rc = mu_message_get_num_parts (msg, &nparts);
      if (rc)
	return rc;

      rc = mu_list_create (&bs->v.multipart.body_parts);
      if (rc)
	return rc;

      mu_list_set_destroy_item (bs->v.multipart.body_parts,
				mu_list_free_bodystructure);
      
      for (i = 1; i <= nparts; i++)
        {
          mu_message_t partmsg;
	  struct mu_bodystructure *partbs;

	  rc = mu_message_get_part (msg, i, &partmsg);
	  if (rc)
	    return rc;

	  rc = bodystructure_init (partmsg, &partbs);
	  if (rc)
	    return rc;

	  rc = mu_list_append (bs->v.multipart.body_parts, partbs);
	  if (rc)
	    {
	      mu_bodystructure_free (partbs);
	      return rc;
	    }
	}
    }
  else
    {
      /* body id: Content-ID. */
      rc = mu_header_aget_value_unfold (header, MU_HEADER_CONTENT_ID,
					&bs->body_id);
      if (rc && rc != MU_ERR_NOENT)
	return rc;
      /* body description: Content-Description. */
      rc = mu_header_aget_value_unfold (header, MU_HEADER_CONTENT_DESCRIPTION,
					&bs->body_descr);
      if (rc && rc != MU_ERR_NOENT)
	return rc;
      
      /* body encoding: Content-Transfer-Encoding. */
      rc = mu_header_aget_value_unfold (header,
					MU_HEADER_CONTENT_TRANSFER_ENCODING,
					&bs->body_encoding);
      if (rc == MU_ERR_NOENT)
	{
	  bs->body_encoding = strdup ("7BIT");
	  if (!bs->body_encoding)
	    return ENOMEM;
	}
      else if (rc)
	return rc;

      /* body size RFC822 format.  */
      rc = mu_message_get_body (msg, &body);
      if (rc)
	return rc;
      rc = mu_body_size (body, &bs->body_size);
      if (rc)
	return rc;
      
      /* If the mime type was text.  */
      if (bs->body_message_type == mu_message_text)
	{
	  rc = mu_body_lines (body, &bs->v.text.body_lines);
	  if (rc)
	    return rc;
	}
      else if (bs->body_message_type == mu_message_rfc822)
	{
	  mu_message_t emsg = NULL;

	  /* Add envelope structure of the encapsulated message.  */
	  rc = mu_message_unencapsulate  (msg, &emsg, NULL);
	  if (rc)
	    return rc;
	  rc = mu_message_get_imapenvelope (emsg, &bs->v.rfc822.body_env);
	  if (rc)
	    return rc;
	  /* Add body structure of the encapsulated message.  */
	  rc = bodystructure_init (emsg, &bs->v.rfc822.body_struct);
	  if (rc)
	    return rc;
	  /* Size in text lines of the encapsulated message.  */
	  rc = mu_message_lines (emsg, &bs->v.rfc822.body_lines);
	  mu_message_destroy (&emsg, NULL);
	}
    }
  
  /* body MD5: Content-MD5.  */
  rc = mu_header_aget_value_unfold (header, MU_HEADER_CONTENT_MD5,
				    &bs->body_md5);
  if (rc && rc != MU_ERR_NOENT)
    return rc;
  
  /* body disposition: Content-Disposition.  */
  rc = mu_header_aget_value_unfold (header, MU_HEADER_CONTENT_DISPOSITION,
				    &buffer);
  if (rc == 0)
    {
      rc = mu_mime_header_parse (buffer, "UTF-8", &bs->body_disposition,
				 &bs->body_disp_param);
      free (buffer);
      if (rc)
	return rc;
    }
  else if (rc != MU_ERR_NOENT)
    return rc;
  /* body language: Content-Language.  */
  rc = mu_header_aget_value_unfold (header, MU_HEADER_CONTENT_LANGUAGE,
				    &bs->body_language);
  if (rc && rc != MU_ERR_NOENT)
    return rc;
  rc = mu_header_aget_value_unfold (header, MU_HEADER_CONTENT_LOCATION,
				    &bs->body_location);
  if (rc && rc != MU_ERR_NOENT)
    return rc;

  return 0;
}

int
mu_message_get_bodystructure (mu_message_t msg,
			      struct mu_bodystructure **pbs)
{
  if (msg == NULL)
    return EINVAL;
  if (pbs == NULL)
    return MU_ERR_OUT_PTR_NULL;
  if (msg->_bodystructure)
    return msg->_bodystructure (msg, pbs);
  return bodystructure_init (msg, pbs);
}

int
mu_message_set_bodystructure (mu_message_t msg,
      int (*_bodystructure) (mu_message_t, struct mu_bodystructure **),
      void *owner)
{
  if (msg == NULL)
    return EINVAL;
  if (msg->owner != owner)
    return EACCES;
  msg->_bodystructure = _bodystructure;
  return 0;
}
