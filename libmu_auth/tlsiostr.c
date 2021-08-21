/* TLS I/O streams */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <gnutls/gnutls.h>
#include <mailutils/stream.h>
#include <mailutils/errno.h>
#include <mailutils/property.h>
#include <mailutils/sys/tls-stream.h>

static int
_tls_io_close (mu_stream_t stream)
{
  struct _mu_tls_io_stream *sp = (struct _mu_tls_io_stream *) stream;
  
  return mu_stream_close (sp->transport);
}

static void
_tls_io_done (struct _mu_stream *stream)
{
  struct _mu_tls_io_stream *sp = (struct _mu_tls_io_stream *) stream;
  mu_stream_unref (sp->transport);
}

static int
_tls_io_flush (struct _mu_stream *stream)
{
  struct _mu_tls_io_stream *sp = (struct _mu_tls_io_stream *) stream;
  return mu_stream_flush (sp->transport);
}

static int
_tls_io_read (struct _mu_stream *stream, char *buf, size_t bufsize,
	      size_t *pnread)
{
  struct _mu_tls_io_stream *sp = (struct _mu_tls_io_stream *) stream;
  ssize_t rc;
  mu_transport_t t[2];
  
  if (sp->up->state != state_open)
    return EINVAL;
  do
    rc = gnutls_record_recv (sp->up->session, buf, bufsize);
  while (rc == GNUTLS_E_AGAIN || rc == GNUTLS_E_INTERRUPTED);
  if (rc >= 0)
    {
      *pnread = rc;
      return 0;
    }

  switch (rc)
    {
    case GNUTLS_E_PUSH_ERROR:
      if (mu_stream_ioctl (sp->up->transport[1],
			   MU_IOCTL_TRANSPORT, MU_IOCTL_OP_GET,
			   &t) == 0 &&
	  mu_stream_err (t[1]))
	rc = mu_stream_last_error (t[1]);
      else
	rc = MU_ERR_WRITE;
      return rc;
      
    case GNUTLS_E_PULL_ERROR:
      if (mu_stream_ioctl (sp->up->transport[0],
			   MU_IOCTL_TRANSPORT, MU_IOCTL_OP_GET,
			   &t) == 0 &&
	  mu_stream_err (t[0]))
	rc = mu_stream_last_error (t[0]);
      else
	rc = MU_ERR_READ;
      return rc;
    }

  sp->up->tls_err = rc;
  return EIO;
}

static int
_tls_io_write (struct _mu_stream *stream, const char *buf, size_t bufsize,
	    size_t *pnwrite)
{
  struct _mu_tls_io_stream *sp = (struct _mu_tls_io_stream *) stream;
  ssize_t rc;
  
  if (sp->up->state != state_open)
    return EINVAL;

  /* gnutls_record_send() docs say:
       If the EINTR is returned by the internal push function (write())
       then GNUTLS_E_INTERRUPTED, will be returned. If GNUTLS_E_INTERRUPTED or
       GNUTLS_E_AGAIN is returned you must call this function again, with the
       same parameters. Otherwise the write operation will be
       corrupted and the connection will be terminated. */
    
  do
    rc = gnutls_record_send (sp->up->session, buf, bufsize);
  while (rc == GNUTLS_E_INTERRUPTED || rc == GNUTLS_E_AGAIN);

  if (rc < 0)
    {
      sp->up->tls_err = rc;
      return EIO;
    }

  *pnwrite = rc;

  return 0;
}

static int
_tls_rd_wait (struct _mu_stream *stream, int *pflags, struct timeval *tvp)
{
  struct _mu_tls_io_stream *sp = (struct _mu_tls_io_stream *) stream;
  int rc = EINVAL;
  
  if (*pflags == MU_STREAM_READY_RD)
    rc = mu_stream_wait (sp->transport, pflags, tvp);
  return rc;
}

static int
_tls_wr_wait (struct _mu_stream *stream, int *pflags, struct timeval *tvp)
{
  struct _mu_tls_io_stream *sp = (struct _mu_tls_io_stream *) stream;
  int rc = EINVAL;
  
  if (*pflags == MU_STREAM_READY_WR)
    rc = mu_stream_wait (sp->transport, pflags, tvp);
  return rc;
}

static int
get_cipher_info (gnutls_session_t session, mu_property_t *pprop)
{
  mu_property_t prop;
  const char *s;
  int rc;

  if (!pprop)
    return EINVAL;

  rc = mu_property_create_init (&prop, mu_assoc_property_init, NULL);
  if (rc)
    return rc;

  s = gnutls_protocol_get_name (gnutls_protocol_get_version (session));
  mu_property_set_value (prop, "protocol", s, 1);

  s = gnutls_cipher_get_name (gnutls_cipher_get (session));
  mu_property_set_value (prop, "cipher", s, 1);

  s = gnutls_mac_get_name (gnutls_mac_get (session));
  mu_property_set_value (prop, "mac", s, 1);

  *pprop = prop;

  return 0;
}

static int
_tls_io_ioctl (struct _mu_stream *stream, int code, int opcode, void *arg)
{
  struct _mu_tls_io_stream *sp = (struct _mu_tls_io_stream *) stream;

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
	      ptrans[0] = (mu_transport_t) sp->transport;
	      ptrans[1] = NULL;
	      break;

	    case MU_IOCTL_OP_SET:
	      return ENOSYS;

	    default:
	      return EINVAL;
	    }
	}
      break;

    case MU_IOCTL_TLSSTREAM:
      switch (opcode)
	{
	case MU_IOCTL_TLS_GET_CIPHER_INFO:
	  return get_cipher_info (sp->up->session, arg);

	default:
	  return EINVAL;
	}
      break;
      
    default:
      return mu_stream_ioctl (sp->transport, code, opcode, arg);
    }
  return 0;
}

int
mu_tls_io_stream_create (mu_stream_t *pstream,
			 mu_stream_t transport, int flags,
			 struct _mu_tls_stream *master)
{
  struct _mu_tls_io_stream *sp;

  sp = (struct _mu_tls_io_stream *)
    _mu_stream_create (sizeof (*sp), (flags & MU_STREAM_RDWR) | _MU_STR_OPEN);
  if (!sp)
    return ENOMEM;

  if (flags & MU_STREAM_READ)
    {
      sp->stream.read = _tls_io_read; 
      sp->stream.wait = _tls_rd_wait;
      mu_stream_set_buffer ((mu_stream_t) sp, mu_buffer_full, 0);
    }
  else
    {
      sp->stream.write = _tls_io_write;
      sp->stream.wait = _tls_wr_wait;
      mu_stream_set_buffer ((mu_stream_t) sp, mu_buffer_line, 0);
    }
  sp->stream.flush = _tls_io_flush;
  sp->stream.close = _tls_io_close;
  sp->stream.done = _tls_io_done; 
  sp->stream.ctl = _tls_io_ioctl;

  mu_stream_ref (transport);
  sp->transport = transport;
  sp->up = master;
  *pstream = (mu_stream_t) sp;
  return 0;
}
