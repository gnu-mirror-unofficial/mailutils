/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2021 Free Software Foundation, Inc.

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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <mailutils/cctype.h>
#include <mailutils/cstr.h>
#include <mailutils/message.h>
#include <mailutils/stream.h>
#include <mailutils/body.h>
#include <mailutils/header.h>
#include <mailutils/errno.h>
#include <mailutils/util.h>
#include <mailutils/assoc.h>
#include <mailutils/io.h>

#include <mailutils/sys/mime.h>
#include <mailutils/sys/message.h>
#include <mailutils/sys/stream.h>

static char default_content_type[] = "text/plain; charset=us-ascii";

/* TODO:
 *  Need to prevent re-entry into mime lib, but allow non-blocking re-entry
 *  into lib.
 */

static int
_mime_append_part (mu_mime_t mime, mu_message_t msg,
		   size_t offset, size_t len, size_t lines)
{
  struct _mime_part *mime_part, **part_arr;
  int ret;
  size_t size;
  mu_header_t hdr;

  if ((mime_part = calloc (1, sizeof (*mime_part))) == NULL)
    return ENOMEM;

  if (mime->nmtp_parts >= mime->tparts)
    {
      if ((part_arr =
	   realloc (mime->mtp_parts,
		    (mime->tparts + 5) * sizeof (mime_part))) == NULL)
	{
	  free (mime_part);
	  return ENOMEM;
	}
      mime->mtp_parts = part_arr;
      mime->tparts += 5;
    }
  mime->mtp_parts[mime->nmtp_parts++] = mime_part;
  if (msg == NULL)
    {
      if ((ret = mu_message_create (&mime_part->msg, mime_part)) == 0)
	{
	  if ((ret =
	       mu_header_create (&hdr, mime->header_buf,
				 mime->header_length)) != 0)
	    {
	      mu_message_destroy (&mime_part->msg, mime_part);
	      free (mime_part);
	      return ret;
	    }
	  mu_message_set_header (mime_part->msg, hdr, mime_part);
	}
      else
	{
	  free (mime_part);
	  return ret;
	}
      mime->header_length = 0;
      if ((ret =
	   mu_header_get_value (hdr, MU_HEADER_CONTENT_TYPE, NULL,
				0, &size)) != 0 || size == 0)
	{
	  if (mu_c_strcasecmp (mime->content_type->subtype, "digest") == 0)
	    mu_header_set_value (hdr,
				 MU_HEADER_CONTENT_TYPE, "message/rfc822", 0);
	  else
	    mu_header_set_value (hdr, MU_HEADER_CONTENT_TYPE, "text/plain",
				 0);
	}
      mime_part->len = len;
      mime_part->lines = lines;
      mime_part->offset = offset;
    }
  else
    {
      mu_message_ref (msg);
      mu_message_size (msg, &mime_part->len);
      mu_message_lines (msg, &mime_part->lines);
      if (mime->stream && mime->nmtp_parts > 1)
	mime_part->offset = mime->mtp_parts[mime->nmtp_parts - 2]->len;
      mime_part->msg = msg;
    }
  mime_part->mime = mime;
  return 0;
}

static void
_mime_append_header_line (mu_mime_t mime)
{
  if (mime->header_length + mime->line_length > mime->header_buf_size)
    {
      char *nhb;

      if ((nhb = realloc (mime->header_buf,
			  mime->header_length + mime->line_length + 128)) == NULL)
	return;
      mime->header_buf = nhb;
      mime->header_buf_size = mime->header_length + mime->line_length + 128;
    }
  memcpy (mime->header_buf + mime->header_length, mime->cur_line,
	  mime->line_length);
  mime->header_length += mime->line_length;
}

int
mu_mime_sget_content_type (mu_mime_t mime, const char **value)
{
  if (!mime)
    return EINVAL;
  if (!mime->content_type)
    return MU_ERR_NOENT;
  if (value)
    *value = mime->content_type->type;
  return 0;
}

int
mu_mime_aget_content_type (mu_mime_t mime, char **value)
{
  char const *s;
  int rc = mu_mime_sget_content_type (mime, &s);
  if (rc == 0 && value)
    {
      if ((*value = strdup (s)) == NULL)
	return errno;
    }
  return 0;
}

int
mu_mime_sget_content_subtype (mu_mime_t mime, const char **value)
{
  if (!mime)
    return EINVAL;
  if (!mime->content_type)
    return MU_ERR_NOENT;
  if (value)
    *value = mime->content_type->subtype;
  return 0;
}

int
mu_mime_aget_content_subtype (mu_mime_t mime, char **value)
{
  char const *s;
  int rc = mu_mime_sget_content_subtype (mime, &s);
  if (rc == 0 && value)
    {
      if ((*value = strdup (s)) == NULL)
	return errno;
    }
  return 0;
}

int
mu_mime_content_type_get_param (mu_mime_t mime, char const *name,
				const char **value)
{
  struct mu_mime_param *p;
  int rc;

  if (!mime || !name)
    return EINVAL;

  if (!mime->content_type)
    return MU_ERR_NOENT;
  
  rc = mu_assoc_lookup (mime->content_type->param, name, &p);
  if (rc == 0 && value)
    *value = p->value;
  return rc;
}

int
mu_mime_content_type_set_param (mu_mime_t mime, char const *name,
				const char *value)
{
  int rc;
  struct mu_mime_param **pparam;
  char *vcopy;
  
  if (!mime || !mime->content_type || !name)
    return EINVAL;

  if (!value)
    return mu_assoc_remove (mime->content_type->param, name);
  
  if ((vcopy = strdup (value)) == NULL)
    return ENOMEM;
  
  rc = mu_assoc_install_ref (mime->content_type->param, name, &pparam);
  switch (rc)
    {
    case 0:
      if ((*pparam = malloc (sizeof **pparam)) == NULL)
	{
	  free (vcopy);
	  return ENOMEM;
	}
      break;

    case MU_ERR_EXISTS:
      free ((*pparam)->value);
      break;

    default:
      return rc;
    }
  (*pparam)->value = vcopy;
  return 0;
}

enum
  {
    BOUNDARY_NO_MATCH,
    BOUNDARY_MATCH,
    BOUNDARY_CLOSE
  };

static int
match_boundary (char const *line, size_t llen,
		char const *boundary, size_t blen)
{
  if (line[llen-1] == '\n')
    llen--;
  if (llen >= blen + 2 &&
      memcmp (line, "--", 2) == 0 &&
      memcmp (line + 2, boundary, blen) == 0)
    {
      if (llen == blen + 2)
	return BOUNDARY_MATCH;
      if (llen == blen + 4 && memcmp (line + blen + 2, "--", 2) == 0)
	return BOUNDARY_CLOSE;
    }
  return BOUNDARY_NO_MATCH;
}  

static int
_mime_parse_mpart_message (mu_mime_t mime)
{
  size_t blength, mb_length, mb_offset, mb_lines;
  int ret;
  int match;
  
  if (!(mime->flags & MIME_PARSER_ACTIVE))
    {
      ret = mu_mime_content_type_get_param (mime, "boundary", &mime->boundary);
      if (ret)
	return ret;
      mime->cur_offset = 0;
      mime->line_length = 0;
      mime->parser_state = MIME_STATE_SCAN_BOUNDARY;
      mime->flags |= MIME_PARSER_ACTIVE;
    }
  mb_length = mime->body_length;
  mb_offset = mime->body_offset;
  mb_lines = mime->body_lines;
  blength = strlen (mime->boundary);

  mu_stream_seek (mime->stream, mime->cur_offset, MU_SEEK_SET, NULL);
  while (mime->parser_state != MIME_STATE_END &&
	 (ret = mu_stream_getline (mime->stream,
				   &mime->cur_line, &mime->line_size,
				   &mime->line_length)) == 0 && mime->line_length)
    {
      switch (mime->parser_state)
	{
	case MIME_STATE_SCAN_BOUNDARY:
	  if ((match = match_boundary (mime->cur_line, mime->line_length,
				       mime->boundary, blength))
	      != BOUNDARY_NO_MATCH)
	    {
	      mime->parser_state = MIME_STATE_HEADERS;
	      if (mime->cur_offset == mb_offset)
		mb_length = 0;
	      else
		mb_length = mime->cur_offset - mb_offset - 1;
	      if (mime->header_length)
		/* this skips the preamble */
		{
		  /* RFC 1521 [Page 30]:
		     NOTE: The CRLF preceding the encapsulation
		     line is conceptually attached to the boundary
		     so that it is possible to have a part that
		     does not end with a CRLF (line break). Body
		     parts that must be considered to end with line
		     breaks, therefore, must have two CRLFs
		     preceding the encapsulation line, the first
		     of which is part of the preceding body part,
		     and the second of which is part of the
		     encapsulation boundary. */
		  
		  if (mb_lines)
		    /* to prevent negative values in case of a
		       malformed message */
		    mb_lines--;
		  
		  _mime_append_part (mime, NULL,
				     mb_offset, mb_length, mb_lines);
		}

	      if (match == BOUNDARY_CLOSE)
		{	/* last boundary */
		  mime->parser_state = MIME_STATE_END;
		  mime->header_length = 0;
		}
	    }
	  else
	    mb_lines++;
	  break;
	  
	case MIME_STATE_HEADERS:
	  _mime_append_header_line (mime);
	  if (mime->line_length == 1)
	    {
	      mime->parser_state = MIME_STATE_SCAN_BOUNDARY;
	      mb_offset = mime->cur_offset + 1;
	      mb_lines = 0;
	    }
	}
      mime->cur_offset += mime->line_length;
    }
  mime->body_lines = mb_lines;
  mime->body_length = mb_length;
  mime->body_offset = mb_offset;
  if (ret != EAGAIN)
    {				/* finished cleanup */
      if (mime->header_length)	/* this skips the preamble */
	_mime_append_part (mime, NULL, mb_offset, mb_length, mb_lines);
      mime->flags &= ~MIME_PARSER_ACTIVE;
      mime->body_offset = mime->body_length =
	mime->header_length = mime->body_lines = 0;
    }
  return ret;
}

/*------ Mime message functions for READING a multipart message -----*/

static int
_mimepart_body_size (mu_body_t body, size_t *psize)
{
  mu_message_t msg = mu_body_get_owner (body);
  struct _mime_part *mime_part = mu_message_get_owner (msg);

  if (mime_part == NULL)
    return EINVAL;
  if (psize)
    *psize = mime_part->len;
  return 0;
}

static int
_mimepart_body_lines (mu_body_t body, size_t *plines)
{
  mu_message_t msg = mu_body_get_owner (body);
  struct _mime_part *mime_part = mu_message_get_owner (msg);

  if (mime_part == NULL)
    return EINVAL;
  if (plines)
    *plines = mime_part->lines;
  return 0;
}

/*------ Mime message/header functions for CREATING multipart message -----*/
		 
static int
_mime_set_content_type (mu_mime_t mime)
{
  const char  *content_type;
  mu_header_t hdr = NULL;
  int ret;

  /* Delayed the creation of the header 'til they create the final message via
     mu_mime_get_message()  */
  if (mime->hdrs == NULL)
    return 0;
  if (mime->nmtp_parts > 1)
    {
      char const *subtype;
      char *cstr;

      if (mime->flags & MIME_ADDED_MULTIPART_CT)
	return 0;

      ret = mu_mime_sget_content_subtype (mime, &subtype);
      switch (ret)
	{
	case 0:
	  break;
	  
	case MU_ERR_NOENT:
	  subtype = MU_MIME_CONTENT_SUBTYPE_MIXED;
	  break;

	default:
	  return ret;
	}

      if (mu_c_strcasecmp (subtype, MU_MIME_CONTENT_SUBTYPE_ALTERNATIVE) == 0)
	{
	  size_t i;

	  /* Make sure content disposition is not set for alternative
	     parts */
	  for (i = 0; i < mime->nmtp_parts; i++)
	    {
	      mu_header_t hdr;
	      char *val;
	      int rc;
	      
	      mu_message_get_header (mime->mtp_parts[i]->msg, &hdr);
	      mu_header_remove (hdr, MU_HEADER_CONTENT_DISPOSITION, 1);

	      rc = mu_header_aget_value_unfold (hdr, MU_HEADER_CONTENT_TYPE,
						&val);
	      if (rc == 0)
		{
		  mu_content_type_t ct;
		  rc = mu_content_type_parse (val, NULL, &ct);
		  if (rc == 0)
		    {
		      char *type;
		      
		      rc = mu_asprintf (&type, "%s/%s", ct->type, ct->subtype);
		      if (rc == 0)
			{
			  mu_mime_header_set (hdr,
					      MU_HEADER_CONTENT_TYPE, type,
					      ct->param);
			  free (type);
			}
		      mu_content_type_destroy (&ct);
		    }
		  free (val);
		}
	    }
	}

      ret = mu_content_type_format (mime->content_type, &cstr);
      if (ret == 0)
	{
	  ret = mu_header_set_value (mime->hdrs, MU_HEADER_CONTENT_TYPE,
				     cstr, 1);	  
	  free (cstr);
	  if (ret == 0)
	    mime->flags |= MIME_ADDED_MULTIPART_CT;
	}
    }
  else
    {
      if ((mime->flags & (MIME_ADDED_CT | MIME_ADDED_MULTIPART_CT))
	  == MIME_ADDED_CT)
	return 0;
      mime->flags &= ~MIME_ADDED_MULTIPART_CT;
      if (mime->nmtp_parts)
	mu_message_get_header (mime->mtp_parts[0]->msg, &hdr);

      if (hdr == NULL
	  || mu_header_sget_value (hdr, MU_HEADER_CONTENT_TYPE,
				   &content_type))
	content_type = default_content_type;

      ret = mu_header_set_value (mime->hdrs, MU_HEADER_CONTENT_TYPE,
				 content_type, 1);
      if (ret)
	return ret;

      if (hdr)
	{
	  const char *content_te;
	  
	  /* if the only part contains a transfer-encoding
	     field, set it on the message header too */
	  if (mu_header_sget_value (hdr,
				    MU_HEADER_CONTENT_TRANSFER_ENCODING,
				    &content_te) == 0)
	    ret = mu_header_set_value (mime->hdrs,
				       MU_HEADER_CONTENT_TRANSFER_ENCODING,
				       content_te, 1);

	  if (ret == 0
	      && mu_header_sget_value (hdr,
				       MU_HEADER_CONTENT_DESCRIPTION,
				       &content_te) == 0)
	    ret = mu_header_set_value (mime->hdrs,
				       MU_HEADER_CONTENT_DESCRIPTION,
				       content_te, 1);

	}
    }
  mime->flags |= MIME_ADDED_CT;
  return ret;
}


static int
_mime_part_size (mu_mime_t mime, size_t *psize)
{
  size_t total;
  int ret;

  if (mime->nmtp_parts == 0)
    {
      *psize = 0;
      return 0;
    }
  
  if ((ret = _mime_set_content_type (mime)) != 0)
    return ret;
  if (mime->nmtp_parts == 1)
    {
      ret = mu_message_size (mime->mtp_parts[0]->msg, &total);
    }
  else
    {
      size_t i, size, boundary_len;

      boundary_len = strlen (mime->boundary);
      total = boundary_len + 3;

      for (i = 0; i < mime->nmtp_parts; i++)
	{
	  ret = mu_message_size (mime->mtp_parts[i]->msg, &size);
	  if (ret)
	    break;
	  total += size + boundary_len + 4;
	}
      total += 2; /* ending boundary line */
    }
  *psize = total;
  return ret;
}


struct _mime_body_stream
{
  struct _mu_stream stream;
  mu_mime_t mime;
};

static int
_mime_body_stream_size (mu_stream_t stream, mu_off_t *psize)
{
  struct _mime_body_stream *mstr = (struct _mime_body_stream *)stream;
  mu_mime_t mime = mstr->mime;
  size_t sz;
  int rc = _mime_part_size (mime, &sz);
  if (rc == 0)
    *psize = sz;
  return rc;
}

static void
mime_reset_state (mu_mime_t mime)
{				/* reset message */
  mime->cur_offset = 0;
  mime->cur_part = 0;
  mime->part_offset = 0;
  
  if (mime->nmtp_parts > 1)
    mime->flags |= MIME_INSERT_BOUNDARY;
}

/* FIXME: The seek method is defective */
static int
_mime_body_stream_seek (mu_stream_t stream, mu_off_t off, mu_off_t *presult)
{
  struct _mime_body_stream *mstr = (struct _mime_body_stream *)stream;
  mu_mime_t mime = mstr->mime;

  if (off == 0)
    mime_reset_state (mime);

  if (off != mime->cur_offset)
    {
      int rc;
      mu_stream_t nullstr;

      if (mime->flags & MIME_SEEK_ACTIVE)
	return ESPIPE;
      mime->flags |= MIME_SEEK_ACTIVE;
      
      rc = mu_nullstream_create (&nullstr, MU_STREAM_WRITE);
      if (rc == 0)
	{
	  mu_off_t total;
	  rc = mu_stream_copy (nullstr, stream, off, &total);
	  mu_stream_destroy (&nullstr);
	  if (rc == 0 && total != off)
	    rc = ESPIPE;
	}
      mime->flags &= ~MIME_SEEK_ACTIVE;
      if (rc)
	return rc;
    }
  *presult = off;
  return 0;
}

#define ADD_CHAR(buf, c, offset, buflen, total, nbytes)	\
  do							\
    {							\
      *(buf)++ = c;					\
      (offset)++;					\
      (total)++;				        \
      if (--(buflen) == 0)			        \
	{						\
	  *(nbytes) = total;				\
	  return 0;					\
	}						\
    }							\
  while (0)

static int
_mime_body_stream_read (mu_stream_t stream, char *buf, size_t buflen,
			size_t *nbytes)
{
  struct _mime_body_stream *mstr = (struct _mime_body_stream *)stream;
  mu_mime_t mime = mstr->mime;
  int ret = 0;
  size_t total = 0;
  
  if (mime->nmtp_parts == 0)
    {
      *nbytes = 0;
      return 0;
    }
  
  if ((ret = _mime_set_content_type (mime)) == 0)
    {
      do
	{
	  size_t part_nbytes = 0;

	  if (buflen == 0)
	    break;
	  if (mime->nmtp_parts > 1)
	    {
	      size_t len;
	      
	      if (mime->flags & MIME_INSERT_BOUNDARY)
		{
		  if ((mime->flags & MIME_ADDING_BOUNDARY) == 0)
		    {
		      mime->boundary_len = strlen (mime->boundary);
		      mime->preamble = 2;
		      if (mime->cur_part == mime->nmtp_parts)
			mime->postamble = 2;
		      mime->flags |= MIME_ADDING_BOUNDARY;
		    }
		  while (mime->preamble)
		    {
		      mime->preamble--;
		      ADD_CHAR (buf, '-', mime->cur_offset, buflen,
				total, nbytes);
		    }
		  len = strlen (mime->boundary) - mime->boundary_len;
		  while (mime->boundary_len)
		    {
		      mime->boundary_len--;
		      ADD_CHAR (buf,
				mime->boundary[len++],
				mime->cur_offset, buflen,
				total, nbytes);
		    }
		  while (mime->postamble)
		    {
		      mime->postamble--;
		      ADD_CHAR (buf, '-', mime->cur_offset, buflen,
				total, nbytes);
		    }
		  mime->flags &=
		    ~(MIME_INSERT_BOUNDARY | MIME_ADDING_BOUNDARY);
		  mime->part_offset = 0;
		  ADD_CHAR (buf, '\n', mime->cur_offset, buflen,
			    total, nbytes);
		}

	      if (!mime->part_stream)
		{
		  if (mime->cur_part >= mime->nmtp_parts)
		    {
		      *nbytes = total;
		      return 0;
		    }
		  ret = mu_message_get_streamref (mime->mtp_parts[mime->cur_part]->msg,
						  &mime->part_stream);
		}
	    }
	  else if (!mime->part_stream)
	    {
	      mu_body_t part_body;

	      if (mime->cur_part >= mime->nmtp_parts)
		{
		  *nbytes = total;
		  return 0;
		}
	      mu_message_get_body (mime->mtp_parts[mime->cur_part]->msg,
				   &part_body);
	      ret = mu_body_get_streamref (part_body, &mime->part_stream);
	    }
	  if (ret)
	    break;
	  ret = mu_stream_seek (mime->part_stream, mime->part_offset,
				MU_SEEK_SET, NULL);
	  if (ret)
	    {
	      mu_stream_destroy (&mime->part_stream);
	      break;
	    }
	  while (buflen > 0 &&
		 (ret = mu_stream_read (mime->part_stream, buf, buflen,
					&part_nbytes)) == 0)
	    {
	      if (part_nbytes)
		{
		  mime->part_offset += part_nbytes;
		  mime->cur_offset += part_nbytes;
		  total += part_nbytes;
		  buflen -= part_nbytes;
		  buf += part_nbytes;
		}
	      else 
		{
		  mu_stream_destroy (&mime->part_stream);
		  mime->flags |= MIME_INSERT_BOUNDARY;
		  mime->cur_part++;
		  ADD_CHAR (buf, '\n', mime->cur_offset, buflen,
			    total, nbytes);
		  break;
		}
	    }
	}
      while (ret == 0 && mime->cur_part <= mime->nmtp_parts);
    }
  if (ret)
    mu_stream_destroy (&mime->part_stream);
  
  *nbytes = total;
  return ret;
}

static int
_mime_body_stream_ioctl (mu_stream_t stream, int code, int opcode, void *arg)
{
  struct _mime_body_stream *mstr = (struct _mime_body_stream *)stream;
  mu_mime_t mime = mstr->mime;
  mu_stream_t msg_stream;
  int rc;
  
  switch (code)
    {
    case MU_IOCTL_TRANSPORT:
      if (!arg)
	return EINVAL;
      switch (opcode)
        {
        case MU_IOCTL_OP_GET:
          if (mime->nmtp_parts == 0 || mime->cur_offset == 0)
	    return EINVAL;
          rc = mu_message_get_streamref (mime->mtp_parts[mime->cur_part]->msg,
				         &msg_stream);
          if (rc)
	    break;
          rc = mu_stream_ioctl (msg_stream, code, opcode, arg);
          mu_stream_destroy (&msg_stream);
          break;
	  
	case MU_IOCTL_OP_SET:
	  return ENOSYS;
	  
	default:
	  return EINVAL;
	}
      break;

    default:
      rc = ENOSYS;
    }
  return rc;
}

static int
create_mime_body_stream (mu_stream_t *pstr, mu_mime_t mime)
{
  struct _mime_body_stream *sp =
    (struct _mime_body_stream *)_mu_stream_create (sizeof (*sp),
						   MU_STREAM_READ | MU_STREAM_SEEK);
  if (!sp)
    return ENOMEM;
  sp->stream.read = _mime_body_stream_read;
  sp->stream.seek = _mime_body_stream_seek;
  sp->stream.ctl = _mime_body_stream_ioctl;
  sp->stream.size = _mime_body_stream_size;
  sp->mime = mime;
  mime_reset_state (mime);
  *pstr = (mu_stream_t) sp;
  return 0;
}


static int
_mime_body_size (mu_body_t body, size_t *psize)
{
  mu_message_t msg = mu_body_get_owner (body);
  mu_mime_t mime = mu_message_get_owner (msg);
  return _mime_part_size (mime, psize);
}

static int
_mime_body_lines (mu_body_t body, size_t *plines)
{
  mu_message_t msg = mu_body_get_owner (body);
  mu_mime_t mime = mu_message_get_owner (msg);
  int i, ret;
  size_t total = 0;

  if (mime->nmtp_parts == 0)
    {
      *plines = 0;
      return 0;
    }
  
  if ((ret = _mime_set_content_type (mime)) != 0)
    return ret;
  for (i = 0; i < mime->nmtp_parts; i++)
    {
      size_t lines;
      
      mu_message_lines (mime->mtp_parts[i]->msg, &lines);
      total += lines;
      if (mime->nmtp_parts > 1)	/* boundary line */
	total++;
    }
  *plines = total;
  return 0;
}

#define MIME_MULTIPART_FLAGS \
  (MU_MIME_MULTIPART_MIXED|MU_MIME_MULTIPART_ALT)

int
mu_mime_create (mu_mime_t *pmime, mu_message_t msg, int flags)
{
  int ret = 0;
  
  if (pmime == NULL)
    return EINVAL;
  
  if ((flags & MIME_MULTIPART_FLAGS) == MIME_MULTIPART_FLAGS
      || (flags & ~MIME_MULTIPART_FLAGS) != 0)
    return EINVAL;

  if (msg)
    {
      mu_mime_t mime = NULL;
      mu_body_t body;
      
      *pmime = NULL;
      if ((mime = calloc (1, sizeof (*mime))) == NULL)
	return ENOMEM;
      if (msg)
	{
	  if ((ret = mu_message_get_header (msg, &mime->hdrs)) == 0)
	    {
	      char *buf;
	      if ((ret = mu_header_aget_value_unfold (mime->hdrs,
						      MU_HEADER_CONTENT_TYPE,
						      &buf)) == 0)
		{
		  ret = mu_content_type_parse (buf, NULL, &mime->content_type);
		  free (buf);
		}
	      else if (ret == MU_ERR_NOENT)
		{
		  ret = mu_content_type_parse (default_content_type,
					       NULL, &mime->content_type);
		}
	      if (ret == 0)
		{
		  mime->msg = msg;
		  mu_message_get_body (msg, &body);
		  mu_body_get_streamref (body, &mime->stream);
		}
	    }
	}
      if (ret != 0)
	{
	  mu_content_type_destroy (&mime->content_type);
	  free (mime);
	}
      else
	{
	  mime->flags = 0;
	  mime->ref_count = 1;
	  *pmime = mime;
	}
    }
  else
    {
      ret = mu_mime_create_multipart (pmime,
				      flags == 0
				        || (flags & MU_MIME_MULTIPART_MIXED)
				       ? MU_MIME_CONTENT_SUBTYPE_MIXED
				       : MU_MIME_CONTENT_SUBTYPE_ALTERNATIVE,
				      NULL);
    }

  return ret;
}

static int
param_copy (char const *name, void *vptr, void *call_data)
{
  mu_assoc_t a = call_data;
  struct mu_mime_param *val = vptr;
  struct mu_mime_param *p, **pptr;
  int rc;
  
  rc = mu_assoc_install_ref2 (a, name, &pptr, NULL);
  if (rc == MU_ERR_EXISTS)
    return 0; /* Ignore duplicates */
  if (rc)
    return rc;
  
  if ((p = malloc (sizeof *p)) == NULL)
    return ENOMEM;
  if (val->lang)
    {
      if ((p->lang = strdup (val->lang)) == NULL)
	{
	  mu_mime_param_free (p);
	  return ENOMEM;
	}
    }
  else
    p->lang = NULL;

  if (val->cset)
    {
      if ((p->cset = strdup (val->cset)) == NULL)
	{
	  mu_mime_param_free (p);
	  return ENOMEM;
	}
    }
  else
    p->cset = NULL;
  
  if ((p->value = strdup (val->value)) == NULL)
    {
      mu_mime_param_free (p);
      return ENOMEM;
    }
  *pptr = p;
  return 0;
}

int
mu_mime_create_multipart (mu_mime_t *pmime, char const *subtype,
			  mu_assoc_t param)
{
  int rc;
  mu_mime_t mime;
  char boundary[128];
  struct mu_mime_param *p;
  
  if (pmime == NULL)
    return MU_ERR_OUT_PTR_NULL;
  if ((mime = calloc (1, sizeof (*mime))) == NULL)
    return ENOMEM;
  mime->flags |= MIME_NEW_MESSAGE;
  mime->ref_count = 1;
	  
  rc = mu_content_type_parse ("multipart/mixed", NULL, &mime->content_type);
  if (rc)
    {
      free (mime);
      return rc;
    }
  if (subtype)
    {
      free (mime->content_type->subtype);
      if ((mime->content_type->subtype = strdup (subtype)) == NULL)
	{
	  mu_mime_destroy (&mime);
	  return errno;
	}
    }

  snprintf (boundary, sizeof boundary, "%ld-%ld=:%ld",
	    (long) random (), (long) time (0), (long) getpid ());
  
  p = calloc (1, sizeof (*p));
  if (!p)
    {
      mu_mime_destroy (&mime);
      return rc;
    }

  if ((p->value = strdup (boundary)) == NULL)
    {
      free (p);
      mu_mime_destroy (&mime);
      return errno;
    }
  rc = mu_assoc_install (mime->content_type->param, "boundary", p);
  if (rc)
    {
      free (p->value);
      free (p);
      mu_mime_destroy (&mime);
      return rc;
    }
  mime->boundary = p->value;

  if (param)
    {
      rc = mu_assoc_foreach (param, param_copy, mime->content_type->param);
      if (rc)
	{
	  mu_mime_destroy (&mime);
	  return rc;
	}
    }
  
  *pmime = mime;
  return 0;
}

void
mu_mime_ref (mu_mime_t mime)
{
  mime->ref_count++;
}

static void
_mu_mime_free (mu_mime_t mime)
{
  struct _mime_part *mime_part;
  int i;

  if (mime->mtp_parts)
    {
      for (i = 0; i < mime->nmtp_parts; i++)
	{
	  mime_part = mime->mtp_parts[i];
	  mu_message_unref (mime_part->msg);
	  free (mime_part);
	}
      free (mime->mtp_parts);
    }
  mu_stream_destroy (&mime->stream);
  mu_stream_destroy (&mime->part_stream);
  if (mime->msg && mime->flags & MIME_NEW_MESSAGE)
    mu_message_destroy (&mime->msg, mime);
  mu_content_type_destroy (&mime->content_type);
  free (mime->cur_line);
  free (mime->header_buf);
  free (mime);
}

void
mu_mime_unref (mu_mime_t mime)
{
  if (--mime->ref_count == 0)
    _mu_mime_free (mime);
}

void
mu_mime_destroy (mu_mime_t *pmime)
{
  if (pmime && *pmime)
    {
      mu_mime_unref (*pmime);
      *pmime = NULL;
    }
}

int
mu_mime_get_part (mu_mime_t mime, size_t part, mu_message_t *msg)
{
  size_t          nmtp_parts;
  int             ret = 0, flags = 0;
  mu_stream_t        stream;
  mu_body_t          body;
  struct _mime_part *mime_part;

  if ((ret = mu_mime_get_num_parts (mime, &nmtp_parts)) == 0)
    {
      if (part < 1 || part > nmtp_parts)
	return MU_ERR_NOENT;
      if (nmtp_parts == 1 && mime->mtp_parts == NULL)
	*msg = mime->msg;
      else
	{
	  mime_part = mime->mtp_parts[part - 1];
	  if (mime->stream &&
	      !mime_part->body_created &&
	      (ret = mu_body_create (&body, mime_part->msg)) == 0)
	    {
	      mu_body_set_size (body, _mimepart_body_size, mime_part->msg);
	      mu_body_set_lines (body, _mimepart_body_lines, mime_part->msg);
	      mu_stream_get_flags (mime->stream, &flags);
	      ret = mu_streamref_create_abridged (&stream, mime->stream,
						  mime_part->offset,
						  mime_part->offset +
						    mime_part->len - 1);
	      if (ret == 0)
		{
		  mu_stream_set_flags (stream,
				       MU_STREAM_READ
				       | (flags &
					  (MU_STREAM_SEEK
					   | MU_STREAM_NONBLOCK)));
		  mu_body_set_stream (body, stream, mime_part->msg);
		  mu_message_set_body (mime_part->msg, body, mime_part);
		  mime_part->body_created = 1;
		}
	    }
	  *msg = mime_part->msg;
	}
    }
  return ret;
}

int
mu_mime_get_num_parts (mu_mime_t mime, size_t *nmtp_parts)
{
  int             ret = 0;

  if ((mime->nmtp_parts == 0 && !mime->boundary)
      || mime->flags & MIME_PARSER_ACTIVE)
    {
      if (mu_mime_is_multipart (mime))
	{
	  if ((ret = _mime_parse_mpart_message (mime)) != 0)
	    return (ret);
	}
      else
	{
	  *nmtp_parts = 1;
	  return 0;
	}
    }
  *nmtp_parts = mime->nmtp_parts;
  return (ret);
}

int
mu_mime_add_part (mu_mime_t mime, mu_message_t msg)
{
  int ret;
  
  if (mime == NULL || msg == NULL || (mime->flags & MIME_NEW_MESSAGE) == 0)
    return EINVAL;
  if ((ret = _mime_append_part (mime, msg, 0, 0, 0)) == 0)
    ret = _mime_set_content_type (mime);
  return ret;
}

int
mu_mime_get_message (mu_mime_t mime, mu_message_t *msg)
{
  mu_stream_t body_stream;
  mu_body_t body;
  int ret = 0;

  if (mime == NULL || msg == NULL)
    return EINVAL;
  if (mime->msg == NULL)
    {
      if ((mime->flags & MIME_NEW_MESSAGE) == 0)
	return EINVAL;
      if ((ret = mu_message_create (&mime->msg, mime)) == 0)
	{
	  if ((ret = mu_header_create (&mime->hdrs, NULL, 0)) == 0)
	    {
	      mu_message_set_header (mime->msg, mime->hdrs, mime);
	      mu_header_set_value (mime->hdrs, MU_HEADER_MIME_VERSION, "1.0",
				   1);
	      if ((ret = _mime_set_content_type (mime)) == 0)
		{
		  if ((ret = mu_body_create (&body, mime->msg)) == 0)
		    {
		      mu_message_set_body (mime->msg, body, mime);
		      mu_body_set_size (body, _mime_body_size, mime->msg);
		      mu_body_set_lines (body, _mime_body_lines, mime->msg);
		      ret = create_mime_body_stream (&body_stream, mime);
		      if (ret == 0)
			{
			  mu_body_set_stream (body, body_stream, mime->msg);
			  mime->msg->mime = mime;
			  mu_message_ref (mime->msg);
			  *msg = mime->msg;
			  return 0;
			}
		    }
		}
	    }
	  mu_message_destroy (&mime->msg, mime);
	  mime->msg = NULL;
	}
    }
  if (ret == 0)
    {
      mu_message_ref (mime->msg);
      *msg = mime->msg;
    }
  return ret;
}


int
mu_mime_to_message (mu_mime_t mime, mu_message_t *pmsg)
{
  mu_message_t msg;
  int rc = mu_mime_get_message (mime, &msg);
  if (rc == 0)
    {
      mu_message_unref (msg);
      mime->msg = NULL;
      mu_mime_ref (mime);
      *pmsg = msg;
    }
  return rc;
}

int
mu_mime_is_multipart (mu_mime_t mime)
{
  return (mime->content_type
	  && mu_c_strcasecmp (mime->content_type->type, MU_MIME_CONTENT_TYPE_MULTIPART) == 0);
}
