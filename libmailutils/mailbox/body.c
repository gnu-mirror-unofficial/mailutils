/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2020 Free Software Foundation, Inc.

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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mailutils/stream.h>
#include <mailutils/util.h>
#include <mailutils/errno.h>
#include <mailutils/sys/stream.h>
#include <mailutils/sys/body.h>

#define BODY_MODIFIED         0x10000


struct _mu_body_stream
{
  struct _mu_stream stream;
  mu_body_t body;
  mu_stream_t transport;
};

/* Body stream.  */
#define BODY_RDONLY 0
#define BODY_RDWR 1

static int
init_tmp_stream (mu_body_t body)
{
  int rc;
  mu_off_t off;

  rc = mu_stream_seek (body->data_stream, 0, MU_SEEK_CUR, &off);
  if (rc)
    return rc;

  rc = mu_stream_seek (body->data_stream, 0, MU_SEEK_SET, NULL);
  if (rc)
    return rc;
  
  rc = mu_stream_copy (body->temp_stream, body->data_stream, 0, NULL);
  if (rc)
    return rc;
  
  mu_stream_seek (body->data_stream, off, MU_SEEK_SET, NULL);

  return mu_stream_seek (body->temp_stream, off, MU_SEEK_SET, NULL);
}

static int
body_get_transport (mu_body_t body, int mode, mu_stream_t *pstr)
{
  if (!body->data_stream && body->_get_stream)
    {
      int status = body->_get_stream (body, &body->data_stream);
      if (status)
	return status;
    }
  if (mode == BODY_RDWR || !body->data_stream)
    {
      /* Create the temporary file.  */
      if (!body->temp_stream)
	{
	  int rc;
	  
	  rc = mu_temp_file_stream_create (&body->temp_stream, NULL, 0);
	  if (rc)
	    return rc;
	  mu_stream_set_buffer (body->temp_stream, mu_buffer_full, 0);
	  if (body->data_stream)
	    {
	      rc = init_tmp_stream (body);
	      if (rc)
		{
		  mu_stream_destroy (&body->temp_stream);
		  return rc;
		}
	    }
	}
      body->flags |= BODY_MODIFIED;
    }
  *pstr = body->temp_stream ? body->temp_stream : body->data_stream;
  return 0;
}
    
static int
bstr_close (struct _mu_stream *stream)
{
  return 0;
}

void
bstr_done (struct _mu_stream *stream)
{
  struct _mu_body_stream *str = (struct _mu_body_stream*) stream;
  mu_stream_unref (str->transport);
  mu_body_unref (str->body);
}

static int
bstr_seek (mu_stream_t stream, mu_off_t off, mu_off_t *presult)
{
  struct _mu_body_stream *str = (struct _mu_body_stream*) stream;
  return mu_stream_seek (str->transport, off, MU_SEEK_SET, presult);
}

static int
bstr_ioctl (mu_stream_t stream, int code, int opcode, void *ptr)
{
  struct _mu_body_stream *str = (struct _mu_body_stream*) stream;
  return mu_stream_ioctl (str->transport, code, opcode, ptr);
}

static int
bstr_read (mu_stream_t stream, char *buf, size_t size, size_t *pret)
{
  struct _mu_body_stream *str = (struct _mu_body_stream*) stream;
  return mu_stream_read (str->transport, buf, size, pret);
}

static int
bstr_write (mu_stream_t stream, const char *buf, size_t size, size_t *pret)
{
  struct _mu_body_stream *str = (struct _mu_body_stream*) stream;
  if (!str->body->temp_stream)
    {
      int rc;
      mu_off_t off;
      mu_stream_t tmp, transport;
      
      rc = mu_stream_seek (str->transport, 0, MU_SEEK_CUR, &off);
      if (rc)
	return rc;
      rc = body_get_transport (str->body, BODY_RDWR, &tmp);
      if (rc)
	return rc;
      rc = mu_streamref_create (&transport, tmp);
      if (rc)
	return rc;
      mu_stream_destroy (&str->transport);
      str->transport = transport;
      rc = mu_stream_seek (str->transport, off, MU_SEEK_SET, NULL);      
      if (rc)
	return rc;
    }
  return mu_stream_write (str->transport, buf, size, pret);
}

static int
bstr_truncate (mu_stream_t stream, mu_off_t n)
{
  struct _mu_body_stream *str = (struct _mu_body_stream*) stream;
  return mu_stream_truncate (str->transport, n);
}

static int
bstr_size (mu_stream_t stream, mu_off_t *size)
{
  struct _mu_body_stream *str = (struct _mu_body_stream*) stream;
  return mu_stream_size (str->transport, size);
}

static int
bstr_flush (mu_stream_t stream)
{
  struct _mu_body_stream *str = (struct _mu_body_stream*) stream;
  return mu_stream_flush (str->transport);
}

/* Default function for the body.  */
static int
body_get_lines (mu_body_t body, size_t *plines)
{
  mu_stream_t transport, null;
  int status;
  mu_off_t off;
  mu_stream_stat_buffer stat;
  
  status = body_get_transport (body, BODY_RDONLY, &transport);
  if (status)
    return status;
  
  status = mu_stream_flush (transport);
  if (status)
    return status;
  status = mu_stream_seek (transport, 0, MU_SEEK_CUR, &off);
  if (status)
    return status;
  status = mu_stream_seek (transport, 0, MU_SEEK_SET, NULL);
  if (status)
    return status;
  status = mu_nullstream_create (&null, MU_STREAM_WRITE);
  if (status)
    return status;
  mu_stream_set_stat (null, MU_STREAM_STAT_MASK (MU_STREAM_STAT_OUTLN),
		      stat);
  status = mu_stream_copy (null, transport, 0, NULL);
  mu_stream_destroy (&null);
  mu_stream_seek (transport, off, MU_SEEK_SET, NULL);
  if (status == 0)
    *plines = stat[MU_STREAM_STAT_OUTLN];
  return status;
}

static int
body_get_size (mu_body_t body, size_t *psize)
{
  mu_stream_t transport;
  mu_off_t off = 0;
  int rc;
  
  rc = body_get_transport (body, BODY_RDONLY, &transport);
  if (rc)
    return rc;
  rc = mu_stream_size (transport, &off);
  if (rc == 0)
    *psize = off;
  return 0;
}


static int
body_stream_create (mu_body_t body, mu_stream_t *return_stream)
{
  int rc;
  mu_stream_t stream, transport;
  struct _mu_body_stream *str;

  rc = body_get_transport (body, BODY_RDONLY, &stream);
  if (rc)
    return rc;
  rc = mu_streamref_create (&transport, stream);
  if (rc)
    return rc;

  str = (struct _mu_body_stream *)
	    _mu_stream_create (sizeof (*str),
			       MU_STREAM_RDWR|MU_STREAM_SEEK|_MU_STR_OPEN);
  if (!str)
    return ENOMEM;
  str->transport = transport;
  str->body = body;
  str->stream.ctl = bstr_ioctl;
  str->stream.read = bstr_read;
  str->stream.write = bstr_write;
  str->stream.truncate = bstr_truncate;
  str->stream.size = bstr_size;
  str->stream.seek = bstr_seek;
  str->stream.flush = bstr_flush;
  str->stream.close = bstr_close;
  str->stream.done = bstr_done;
  /* Override the defaults.  */
  body->_lines = body_get_lines;
  body->_size = body_get_size;
  mu_body_ref (body);
  *return_stream = (mu_stream_t) str;
  return 0;
}

int
mu_body_get_streamref (mu_body_t body, mu_stream_t *pstream)
{
  if (body == NULL)
    return EINVAL;
  if (pstream == NULL)
    return MU_ERR_OUT_PTR_NULL;
  return body_stream_create (body, pstream);
}

int
mu_body_set_stream (mu_body_t body, mu_stream_t stream, void *owner)
{
  if (body == NULL)
   return EINVAL;
  if (body->owner != owner)
    return EACCES;
  /* make sure we destroy the old one if it is owned by the body */
  mu_stream_destroy (&body->temp_stream);
  mu_stream_destroy (&body->data_stream);
  body->data_stream = stream;
  body->flags |= BODY_MODIFIED;
  return 0;
}

int
mu_body_set_get_stream (mu_body_t body,
			int (*_getstr) (mu_body_t, mu_stream_t *),
			void *owner)
{
  if (body == NULL)
    return EINVAL;
  if (body->owner != owner)
    return EACCES;
  body->_get_stream = _getstr;
  return 0;
}

int
mu_body_set_lines (mu_body_t body, int (*_lines) (mu_body_t, size_t *),
		   void *owner)
{
  if (body == NULL)
    return EINVAL;
  if (body->owner != owner)
    return EACCES;
  body->_lines = _lines;
  return 0;
}

int
mu_body_lines (mu_body_t body, size_t *plines)
{
  if (body == NULL)
    return EINVAL;
  if (plines == NULL)
    return MU_ERR_OUT_PTR_NULL;
  if (body->_lines)
    return body->_lines (body, plines);
  /* Fall back on the stream.  */
  return body_get_lines (body, plines);
}

int
mu_body_size (mu_body_t body, size_t *psize)
{
  int rc;
  mu_stream_t str;
  mu_off_t s;
  
  if (body == NULL)
    return EINVAL;
  if (psize == NULL)
    return MU_ERR_OUT_PTR_NULL;
  if (body->_size)
    return body->_size (body, psize);
  /* Fall on the stream.  */
  rc = body_get_transport (body, BODY_RDONLY, &str);
  if (rc)
    return rc;
  rc = mu_stream_size (str, &s);
  mu_stream_unref (str);
  *psize = s;
  return 0;
}

int
mu_body_set_size (mu_body_t body, int (*_size)(mu_body_t, size_t*),
		  void *owner)
{
  if (body == NULL)
    return EINVAL;
  if (body->owner != owner)
    return EACCES;
  body->_size = _size;
  return 0;
}


int
mu_body_create (mu_body_t *pbody, void *owner)
{
  mu_body_t body;

  if (pbody == NULL)
    return MU_ERR_OUT_PTR_NULL;
  if (owner == NULL)
    return EINVAL;

  body = calloc (1, sizeof (*body));
  if (body == NULL)
    return ENOMEM;

  body->owner = owner;
  mu_body_ref (body);
  *pbody = body;
  return 0;
}

static void
_mu_body_free (mu_body_t body)
{
  mu_stream_destroy (&body->data_stream);
  mu_stream_destroy (&body->temp_stream);
  free (body);
}

void
mu_body_ref (mu_body_t body)
{
  if (body)
    body->ref_count++;
}

void
mu_body_unref (mu_body_t body)
{
  if (body && --body->ref_count == 0)
    _mu_body_free (body);
}

void
mu_body_destroy (mu_body_t *pbody, void *owner)
{
  if (pbody && *pbody)
    {
      mu_body_t body = *pbody;
      if (body->owner == owner && --body->ref_count == 0)
	{
	  _mu_body_free (body);
	  *pbody = NULL;
	}
    }
}

void *
mu_body_get_owner (mu_body_t body)
{
  return (body) ? body->owner : NULL;
}

int
mu_body_is_modified (mu_body_t body)
{
  return (body) ? (body->flags & BODY_MODIFIED) : 0;
}

int
mu_body_clear_modified (mu_body_t body)
{
  if (body)
    body->flags &= ~BODY_MODIFIED;
  return 0;
}



