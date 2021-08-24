/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2010-2021 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <mailutils/types.h>
#include <mailutils/alloc.h>
#include <mailutils/errno.h>

#include <mailutils/nls.h>
#include <mailutils/stream.h>
#include <mailutils/sys/stream.h>
#include <mailutils/sys/iostream.h>

static int
_iostream_read (struct _mu_stream *str, char *buf, size_t bufsize,
		size_t *pnread)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;
  int rc = mu_stream_read (sp->transport[_MU_STREAM_INPUT], buf, bufsize,
			   pnread);
  if (rc)
    sp->last_err_str = _MU_STREAM_INPUT;
  return rc;
}

static int
_iostream_write (struct _mu_stream *str, const char *buf, size_t bufsize,
		  size_t *pnwrite)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;
  int rc = mu_stream_write (sp->transport[_MU_STREAM_OUTPUT], buf, bufsize,
			    pnwrite);
  if (rc)
    sp->last_err_str = _MU_STREAM_OUTPUT;
  return rc;
}

static int
_iostream_flush (struct _mu_stream *str)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;
  int rc = mu_stream_flush (sp->transport[_MU_STREAM_INPUT]);
  if (rc)
    {
      sp->last_err_str = _MU_STREAM_INPUT;
      return rc;
    }
  if (sp->transport[_MU_STREAM_INPUT] != sp->transport[_MU_STREAM_OUTPUT])
    {
      rc = mu_stream_flush (sp->transport[_MU_STREAM_OUTPUT]);
      if (rc)
	sp->last_err_str = _MU_STREAM_OUTPUT;
    }
  return rc;
}

static int
_iostream_open (struct _mu_stream *str)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;
  int rc;
  rc = mu_stream_open (sp->transport[_MU_STREAM_INPUT]);
  if (rc)
    {
      sp->last_err_str = _MU_STREAM_INPUT;
      return rc;
    }
  if (sp->transport[_MU_STREAM_INPUT] != sp->transport[_MU_STREAM_OUTPUT])
    {
      rc = mu_stream_open (sp->transport[_MU_STREAM_OUTPUT]);
      if (rc)
	{
	  sp->last_err_str = _MU_STREAM_OUTPUT;
	  mu_stream_close (sp->transport[_MU_STREAM_INPUT]);
	}
    }
  return rc;
}

static int
_iostream_close (struct _mu_stream *str)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;
  mu_stream_close (sp->transport[_MU_STREAM_INPUT]);
  mu_stream_close (sp->transport[_MU_STREAM_OUTPUT]);
  return 0;
}

static void
_iostream_done (struct _mu_stream *str)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;
  mu_stream_unref (sp->transport[_MU_STREAM_INPUT]);
  mu_stream_unref (sp->transport[_MU_STREAM_OUTPUT]);
}

static int
_iostream_get_substream (struct _mu_iostream *sp, mu_stream_t *pstr)
{
  mu_stream_t tmp[2], instream;
  int status;
  
  status = mu_stream_ioctl (sp->transport[MU_TRANSPORT_INPUT],
			    MU_IOCTL_SUBSTREAM, MU_IOCTL_OP_GET, tmp);
  if (status == ENOSYS)
    {
      instream = sp->transport[MU_TRANSPORT_INPUT];
      mu_stream_ref (instream);
      status = 0;
    }
  else if (status == 0)
    {
      mu_stream_unref (tmp[MU_TRANSPORT_OUTPUT]);
      instream = tmp[MU_TRANSPORT_INPUT];
    }

  if (status == 0)
    {
      status = mu_stream_ioctl (sp->transport[MU_TRANSPORT_OUTPUT],
				MU_IOCTL_SUBSTREAM, MU_IOCTL_OP_GET, tmp);
      if (status == 0)
	{
	  mu_stream_unref (tmp[MU_TRANSPORT_INPUT]);
	  pstr[MU_TRANSPORT_INPUT] = instream;
	  pstr[MU_TRANSPORT_OUTPUT] = tmp[MU_TRANSPORT_OUTPUT];
	}
      else if (status == ENOSYS)
	{
	  pstr[MU_TRANSPORT_INPUT] = instream;
	  pstr[MU_TRANSPORT_OUTPUT] = sp->transport[MU_TRANSPORT_OUTPUT];
	  mu_stream_ref (pstr[MU_TRANSPORT_OUTPUT]);
	  status = 0;
	}
      else
	mu_stream_unref (instream);
    }
  return status;
}

static int
_iostream_set_substream (struct _mu_iostream *sp, mu_stream_t *newstr)
{
  mu_stream_t saved[2], tmp[2];
  int status;

  status = _iostream_get_substream (sp, tmp);
  if (status)
    return status;
  saved[MU_TRANSPORT_INPUT] = tmp[MU_TRANSPORT_INPUT];
  saved[MU_TRANSPORT_OUTPUT] = tmp[MU_TRANSPORT_OUTPUT];
  
  tmp[MU_TRANSPORT_INPUT] = newstr[MU_TRANSPORT_INPUT];
  tmp[MU_TRANSPORT_OUTPUT] = NULL;
  status = mu_stream_ioctl (sp->transport[MU_TRANSPORT_INPUT],
			    MU_IOCTL_SUBSTREAM, MU_IOCTL_OP_SET, tmp);
  if (status == ENOSYS)
    {
      mu_stream_unref (sp->transport[MU_TRANSPORT_INPUT]);
      sp->transport[MU_TRANSPORT_INPUT] = newstr[MU_TRANSPORT_INPUT];
      mu_stream_ref (sp->transport[MU_TRANSPORT_INPUT]);
      status = 0;
    }

  if (status == 0)
    {
      tmp[MU_TRANSPORT_INPUT] = NULL;
      tmp[MU_TRANSPORT_OUTPUT] = newstr[MU_TRANSPORT_OUTPUT];
      status = mu_stream_ioctl (sp->transport[MU_TRANSPORT_OUTPUT],
				MU_IOCTL_SUBSTREAM, MU_IOCTL_OP_SET, tmp);
      if (status == ENOSYS)
	{
	  mu_stream_unref (sp->transport[MU_TRANSPORT_OUTPUT]);
	  sp->transport[MU_TRANSPORT_OUTPUT] = newstr[MU_TRANSPORT_OUTPUT];
	  mu_stream_ref (sp->transport[MU_TRANSPORT_OUTPUT]);
	  status = 0;
	}
      else
	{
	  tmp[MU_TRANSPORT_INPUT] = saved[MU_TRANSPORT_INPUT];
	  tmp[MU_TRANSPORT_OUTPUT] = NULL;
	  status = mu_stream_ioctl (sp->transport[MU_TRANSPORT_INPUT],
				    MU_IOCTL_SUBSTREAM, MU_IOCTL_OP_SET, tmp);
	}
    }

  mu_stream_unref (saved[MU_TRANSPORT_INPUT]);
  mu_stream_unref (saved[MU_TRANSPORT_OUTPUT]);
  return status;
}

static int
_iostream_ctl (struct _mu_stream *str, int code, int opcode, void *arg)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;

  switch (code)
    {
    case MU_IOCTL_TRANSPORT:
      if (!arg)
	return EINVAL;
      else
	{
	  mu_transport_t *ptrans = arg;

	  switch (opcode)
	    {
	    case MU_IOCTL_OP_GET:
	      ptrans[0] = (mu_transport_t) sp->transport[_MU_STREAM_INPUT];
	      ptrans[1] = (mu_transport_t) sp->transport[_MU_STREAM_OUTPUT];
	      break;

	    case MU_IOCTL_OP_SET:
	      ptrans = arg;
	      sp->transport[_MU_STREAM_INPUT] = (mu_stream_t) ptrans[0];
	      sp->transport[_MU_STREAM_OUTPUT] = (mu_stream_t) ptrans[1];
	      break;

	    default:
	      return EINVAL;
	    }
	}
      break;

    case MU_IOCTL_SUBSTREAM:
      if (!arg)
	return EINVAL;
      else
	{
	  switch (opcode)
	    {
	    case MU_IOCTL_OP_GET:
	      return _iostream_get_substream (sp, arg);

	    case MU_IOCTL_OP_SET:
	      return _iostream_set_substream (sp, arg);

	    default:
	      return EINVAL;
	    }
	}
      break;
	  
    case MU_IOCTL_TOPSTREAM:
      if (!arg)
	return EINVAL;
      else
	{
	  mu_stream_t *pstr = arg;
	  switch (opcode)
	    {
	    case MU_IOCTL_OP_GET:
	      pstr[0] = sp->transport[0];
	      mu_stream_ref (pstr[0]);
	      pstr[1] = sp->transport[1];
	      mu_stream_ref (pstr[1]);
	      break;

	    case MU_IOCTL_OP_SET:
	      mu_stream_unref (sp->transport[0]);
	      sp->transport[0] = pstr[0];
	      mu_stream_ref (sp->transport[0]);
	      
	      mu_stream_unref (sp->transport[1]);
	      sp->transport[1] = pstr[1];
	      mu_stream_ref (sp->transport[1]);
	      break;

	    default:
	      return EINVAL;
	    }
	}
      break;

    case MU_IOCTL_TRANSPORT_BUFFER:
      if (!arg)
	return EINVAL;
      else
	{
	  struct mu_buffer_query *qp = arg;
	  if (!MU_TRANSPORT_VALID_TYPE (qp->type) || !sp->transport[qp->type])
	    return EINVAL;
	  return mu_stream_ioctl (sp->transport[qp->type], code, opcode, arg);
	}
      
    case MU_IOCTL_TCPSTREAM:
    case MU_IOCTL_TIMEOUT:
      //FIXME: what about output stream?
      return mu_stream_ioctl (sp->transport[_MU_STREAM_INPUT], code, opcode, arg);
      
    default:
      return ENOSYS;
    }
  return 0;
}

static int
_iostream_wait (struct _mu_stream *str, int *pflags, struct timeval *tvp)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;
  int rc = EINVAL;
  
  if (*pflags == MU_STREAM_READY_RD)
    {
      rc = mu_stream_wait (sp->transport[_MU_STREAM_INPUT], pflags, tvp);
      if (rc)
	sp->last_err_str = _MU_STREAM_INPUT;
    }
  else if (*pflags == MU_STREAM_READY_WR)
    {
      rc = mu_stream_wait (sp->transport[_MU_STREAM_OUTPUT], pflags, tvp);
      if (rc)
	sp->last_err_str = _MU_STREAM_OUTPUT;
    }
  return rc;
}

static int
_iostream_shutdown (struct _mu_stream *str, int how)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;
  int rc = EINVAL;
  switch (how)
    {
    case MU_STREAM_READ:
      rc = mu_stream_shutdown (sp->transport[_MU_STREAM_INPUT], how);
      if (rc)
	sp->last_err_str = _MU_STREAM_INPUT;
      break;
      
    case MU_STREAM_WRITE:
      rc = mu_stream_shutdown (sp->transport[_MU_STREAM_OUTPUT], how);
      if (rc)
	sp->last_err_str = _MU_STREAM_OUTPUT;
    }
  return rc;
}

static const char *
_iostream_error_string (struct _mu_stream *str, int rc)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;
  mu_stream_t transport = sp->transport[sp->last_err_str];
  if (transport)
    return mu_stream_strerror (transport, rc);
  return mu_strerror (rc);
}

static void
_iostream_event (struct _mu_stream *str, int code,
		 unsigned long lval, void *pval)
{
  struct _mu_iostream *sp = (struct _mu_iostream *)str;
  if (code == _MU_STR_EVENT_CLRFLAG && lval == _MU_STR_ERR)
    {
      if (sp->transport[_MU_STREAM_INPUT])
	mu_stream_clearerr (sp->transport[_MU_STREAM_INPUT]);
      if (sp->transport[_MU_STREAM_OUTPUT])
	mu_stream_clearerr (sp->transport[_MU_STREAM_OUTPUT]);
    }
}

int
mu_iostream_create (mu_stream_t *pref, mu_stream_t in, mu_stream_t out)
{
  struct _mu_iostream *sp;

  sp = (struct _mu_iostream *)
    _mu_stream_create (sizeof (*sp),
		       MU_STREAM_READ | MU_STREAM_WRITE);
  if (!sp)
    return ENOMEM;

  sp->stream.flags |= _MU_STR_OPEN;
  sp->stream.read = _iostream_read; 
  sp->stream.write = _iostream_write;
  sp->stream.flush = _iostream_flush;
  sp->stream.open = _iostream_open; 
  sp->stream.close = _iostream_close;
  sp->stream.done = _iostream_done; 
  sp->stream.ctl = _iostream_ctl;
  sp->stream.wait = _iostream_wait;
  sp->stream.shutdown = _iostream_shutdown;
  sp->stream.error_string = _iostream_error_string;
  sp->stream.event_cb = _iostream_event;
  sp->stream.event_mask = _MU_STR_EVMASK (_MU_STR_EVENT_CLRFLAG);
    
  mu_stream_ref (in);
  sp->transport[_MU_STREAM_INPUT] = in;
  mu_stream_ref (out);
  sp->transport[_MU_STREAM_OUTPUT] = out;

  mu_stream_set_buffer ((mu_stream_t) sp, mu_buffer_line, 0);
  *pref = (mu_stream_t) sp;
  return 0;
}


