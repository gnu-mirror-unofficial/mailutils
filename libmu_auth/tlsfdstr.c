#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <gnutls/gnutls.h>
#include <mailutils/nls.h>
#include <mailutils/stream.h>
#include <mailutils/errno.h>
#include <mailutils/property.h>
#include <mailutils/sockaddr.h>
#include <mailutils/sys/tls-stream.h>

struct _mu_tlsfd_stream
{
  struct _mu_stream stream;
  enum _mu_tls_stream_state state;
  int session_type; /* Either GNUTLS_CLIENT or GNUTLS_SERVER */
  gnutls_session_t session;
  int tls_err;
  int fd[2];
  int fd_borrowed;
  unsigned io_timeout;
  struct mu_tls_config conf;
  gnutls_certificate_credentials_t cred;
};

static char default_priority_string[] = "NORMAL";

static int
prep_session (mu_stream_t stream, unsigned handshake_timeout)
{
  struct _mu_tlsfd_stream *sp = (struct _mu_tlsfd_stream *) stream;
  gnutls_certificate_credentials_t cred = NULL;
  int rc;
  const char *errp;

  if (!sp->cred)
    {
      rc = gnutls_certificate_allocate_credentials (&cred);
      if (rc)
	{
	  mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		    ("gnutls_certificate_allocate_credentials: %s",
		     gnutls_strerror (rc)));
	  sp->tls_err = rc;
	  return MU_ERR_TLS;
	}
  
      if (sp->conf.ca_file)
	{
	  rc = gnutls_certificate_set_x509_trust_file (cred, sp->conf.ca_file,
						       GNUTLS_X509_FMT_PEM);
	  if (rc < 0)
	    {
	      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
			("can't use X509 CA file %s: %s",
			 sp->conf.ca_file,
			 gnutls_strerror (rc)));
	      goto cred_err;
	    }
	}
  
      if (sp->conf.cert_file && sp->conf.key_file)
	{
	  rc = gnutls_certificate_set_x509_key_file (cred,
						     sp->conf.cert_file, 
						     sp->conf.key_file,
						     GNUTLS_X509_FMT_PEM);
	  if (rc != GNUTLS_E_SUCCESS)
	    {
	      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
			("can't use X509 cert/key pair (%s,%s): %s",
			 sp->conf.cert_file,
			 sp->conf.key_file,
			 gnutls_strerror (rc)));
	      goto cred_err;
	    }
	}
      sp->cred = cred;
    }
  
  rc = gnutls_init (&sp->session, sp->session_type);
  if (rc != GNUTLS_E_SUCCESS)
    {
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		("failed to initialize session: %s", gnutls_strerror (rc)));
      goto cred_err;
    }

  rc = gnutls_priority_set_direct (sp->session,
				   sp->conf.priorities
				     ? sp->conf.priorities
				     : default_priority_string,
				   &errp);
  if (rc != GNUTLS_E_SUCCESS)
    {
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		("error setting priorities near %s: %s",
		 errp, gnutls_strerror (rc)));
      goto cred_err;
    }
  
  rc = gnutls_credentials_set (sp->session, GNUTLS_CRD_CERTIFICATE, sp->cred);
  if (rc)
    {
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		("gnutls_credentials_set: %s", gnutls_strerror (rc)));
      goto sess_err;
    }
  
  if (sp->session_type == GNUTLS_SERVER)
    gnutls_certificate_server_set_request (sp->session, GNUTLS_CERT_REQUEST);

  gnutls_transport_set_int2 (sp->session,
			     sp->fd[MU_TRANSPORT_INPUT],
			     sp->fd[MU_TRANSPORT_OUTPUT]);
  if (handshake_timeout)
    gnutls_handshake_set_timeout (sp->session, handshake_timeout*1000);

  return 0;
  
 sess_err:
  gnutls_deinit (sp->session);
 cred_err:
  if (sp->cred)
    {
      gnutls_certificate_free_credentials (sp->cred);
      sp->cred = NULL;
    }
  sp->tls_err = rc;
  return MU_ERR_TLS;
}

static void
free_conf (struct mu_tls_config *conf)
{
  free (conf->cert_file);
  free (conf->key_file);
  free (conf->ca_file);
  free (conf->priorities);
}

static int
copy_conf (struct mu_tls_config *dst, struct mu_tls_config const *src)
{
  int rc;
  
  dst->cert_file = NULL;
  dst->key_file = NULL;
  dst->ca_file = NULL;
  dst->priorities = NULL;
  dst->handshake_timeout = 0;
  
  if (src)
    {
      if (src->cert_file)
	{
	  dst->cert_file = strdup (src->cert_file);
	  if (!dst->cert_file)
	    goto err;
	}
      else
	dst->cert_file = NULL;

      if (src->key_file)
	{
	  dst->key_file = strdup (src->key_file);
	  if (!dst->cert_file)
	    goto err;
	}
      else
	dst->key_file = NULL;
  
      if (src->ca_file)
	{
	  dst->ca_file = strdup (src->ca_file);
	  if (!dst->ca_file)
	    goto err;
	}
      else
	dst->ca_file = NULL;

      if (src->priorities)
	{
	  dst->priorities = strdup (src->priorities);
	  if (!dst->priorities)
	    goto err;
	}
      else
	dst->priorities = NULL;

      dst->handshake_timeout = src->handshake_timeout;
    }
  
  return 0;
  
 err:
  rc = errno;
  free_conf (dst);
  return rc;
}

static int
_tlsfd_open (mu_stream_t stream)
{
  struct _mu_tlsfd_stream *sp = (struct _mu_tlsfd_stream *) stream;
  int rc = 0;
  
  switch (sp->state)
    {
    case state_closed:
      if (sp->session)
	{
	  gnutls_deinit (sp->session);
	  sp->session = NULL;
	}
      /* FALLTHROUGH */

    case state_init:
      rc = prep_session (stream, sp->conf.handshake_timeout);
      if (rc)
	break;
      if ((sp->tls_err = gnutls_handshake (sp->session)) != GNUTLS_E_SUCCESS)
	{
	  if (sp->tls_err == GNUTLS_E_TIMEDOUT)
	    rc = MU_ERR_TIMEOUT;
	  else
	    rc = MU_ERR_TLS;
	  gnutls_deinit (sp->session);
	  sp->session = NULL;
	  sp->state = state_init;
	}
      else
	{
	  /* FIXME: if (ssl_cafile) verify_certificate (s->session); */
	  sp->state = state_open;
	  rc = 0;
	}
      break;

    default:
      rc = MU_ERR_BADOP;
    }
  return rc;
}

static int
_tlsfd_close (mu_stream_t stream)
{
  struct _mu_tlsfd_stream *sp = (struct _mu_tlsfd_stream *) stream;
  
  if (sp->session && sp->state == state_open)
    {
      gnutls_bye (sp->session, GNUTLS_SHUT_RDWR);
      sp->state = state_closed;
    }

  if (!sp->fd_borrowed)
    {
      close (sp->fd[MU_TRANSPORT_INPUT]);
      close (sp->fd[MU_TRANSPORT_OUTPUT]);
    }
  sp->fd[MU_TRANSPORT_INPUT] = -1;
  sp->fd[MU_TRANSPORT_OUTPUT] = -1;
  
  return 0;
}

static void
_tlsfd_done (struct _mu_stream *stream)
{
  struct _mu_tlsfd_stream *sp = (struct _mu_tlsfd_stream *) stream;
  
  if (sp->session)
    gnutls_deinit (sp->session);
  if (sp->cred)
    gnutls_certificate_free_credentials (sp->cred);

  free_conf (&sp->conf);

  if (sp->fd[MU_TRANSPORT_INPUT] >= 0)
    close (sp->fd[MU_TRANSPORT_INPUT]);
  if (sp->fd[MU_TRANSPORT_OUTPUT] >= 0)
    close (sp->fd[MU_TRANSPORT_OUTPUT]);
}


static int
_tlsfd_read (struct _mu_stream *stream, char *buf, size_t bufsize,
	     size_t *pnread)
{
  struct _mu_tlsfd_stream *sp = (struct _mu_tlsfd_stream *) stream;
  ssize_t rc;
  
  if (sp->state != state_open)
    return EINVAL;
  
  do
    rc = gnutls_record_recv (sp->session, buf, bufsize);
  while (rc == GNUTLS_E_AGAIN || rc == GNUTLS_E_INTERRUPTED);
  if (rc >= 0)
    {
      *pnread = rc;
      return 0;
    }
  sp->tls_err = rc;
  if (sp->tls_err == GNUTLS_E_TIMEDOUT)
    return MU_ERR_TIMEOUT;
  return MU_ERR_TLS;
}

static int
_tlsfd_write (struct _mu_stream *stream, const char *buf, size_t bufsize,
	       size_t *pnwrite)
{
  struct _mu_tlsfd_stream *sp = (struct _mu_tlsfd_stream *) stream;
  ssize_t rc;
  
  if (sp->state != state_open)
    return EINVAL;

  do
    rc = gnutls_record_send (sp->session, buf, bufsize);
  while (rc == GNUTLS_E_INTERRUPTED || rc == GNUTLS_E_AGAIN);

  if (rc >= 0)
    {
      *pnwrite = rc;
      return 0;
    }
  sp->tls_err = rc;
  if (sp->tls_err == GNUTLS_E_TIMEDOUT)
    return MU_ERR_TIMEOUT;
  return MU_ERR_TLS;
}

int
_tlsfd_wait (mu_stream_t stream, int *pflags, struct timeval *tvp)
{
  struct _mu_tlsfd_stream *sp = (struct _mu_tlsfd_stream *) stream;
  int n = 0;
  fd_set rdset, wrset, exset;
  int rc;
  
  if (sp->fd[MU_TRANSPORT_INPUT] == sp->fd[MU_TRANSPORT_OUTPUT])
    return mu_fd_wait (sp->fd[MU_TRANSPORT_INPUT], pflags, tvp);

  FD_ZERO (&rdset);
  FD_ZERO (&wrset);
  FD_ZERO (&exset);
  if (*pflags & MU_STREAM_READY_RD)
    {
      FD_SET (sp->fd[MU_TRANSPORT_INPUT], &rdset);
      n = sp->fd[MU_TRANSPORT_INPUT];
    }
  if (*pflags & MU_STREAM_READY_WR)
    {
      FD_SET (sp->fd[MU_TRANSPORT_OUTPUT], &wrset);
      if (sp->fd[MU_TRANSPORT_OUTPUT] > n)
	n = sp->fd[MU_TRANSPORT_OUTPUT];
    }  
  if (*pflags & MU_STREAM_READY_EX)
    {
      FD_SET (sp->fd[MU_TRANSPORT_INPUT], &exset);
      FD_SET (sp->fd[MU_TRANSPORT_OUTPUT], &exset);
      n = sp->fd[MU_TRANSPORT_INPUT] > sp->fd[MU_TRANSPORT_OUTPUT]
	    ? sp->fd[MU_TRANSPORT_INPUT]
	    : sp->fd[MU_TRANSPORT_OUTPUT];
    }
  
  do
    {
      if (tvp)
	{
	  struct timeval tv = *tvp; 
	  rc = select (n + 1, &rdset, &wrset, &exset, &tv);
	}
      else
	rc = select (n+ 1, &rdset, &wrset, &exset, NULL);
    }
  while (rc == -1 && errno == EINTR);

  if (rc < 0)
    return errno;

  *pflags = 0;
  if (rc > 0)
    {
      if (FD_ISSET (sp->fd[MU_TRANSPORT_INPUT], &rdset))
	*pflags |= MU_STREAM_READY_RD;
      if (FD_ISSET (sp->fd[MU_TRANSPORT_OUTPUT], &wrset))
	*pflags |= MU_STREAM_READY_WR;
      if (FD_ISSET (sp->fd[MU_TRANSPORT_INPUT], &exset) ||
	  FD_ISSET (sp->fd[MU_TRANSPORT_OUTPUT], &exset))
	*pflags |= MU_STREAM_READY_EX;
    }
  return 0;
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
_tlsfd_ioctl (struct _mu_stream *stream, int code, int opcode, void *ptr)
{
  struct _mu_tlsfd_stream *sp = (struct _mu_tlsfd_stream *) stream;

  switch (code)
    {
    case MU_IOCTL_TRANSPORT:
      if (!ptr)
	return EINVAL;
      else
	{
	  mu_transport_t *ptrans = ptr;

	  switch (opcode)
	    {
	    case MU_IOCTL_OP_GET:
	      ptrans[MU_TRANSPORT_INPUT] =
		(mu_transport_t) (intptr_t) sp->fd[MU_TRANSPORT_INPUT];
	      ptrans[MU_TRANSPORT_OUTPUT] = 
		(mu_transport_t) (intptr_t) sp->fd[MU_TRANSPORT_OUTPUT];
	      break;

	    case MU_IOCTL_OP_SET:
	      sp->fd[MU_TRANSPORT_INPUT] =
		(int) (intptr_t) ptrans[MU_TRANSPORT_INPUT];
	      sp->fd[MU_TRANSPORT_OUTPUT] =
		(int) (intptr_t) ptrans[MU_TRANSPORT_OUTPUT];
	      break;

	    default:
	      return EINVAL;
	    }
	}
      break;

    case MU_IOCTL_TRANSPORT_BUFFER:
      if (!ptr)
	return EINVAL;
      else
	{
	  struct mu_buffer_query *qp = ptr;
	  switch (opcode)
	    {
	    case MU_IOCTL_OP_GET:
	      return mu_stream_get_buffer (stream, qp);
	      
	    case MU_IOCTL_OP_SET:
	      return mu_stream_set_buffer (stream, qp->buftype, qp->bufsize);
	      
	    default:
	      return EINVAL;
	    }
	}
      break;

    case MU_IOCTL_FD:
      if (!ptr)
	return EINVAL;
      switch (opcode)
	{
	case MU_IOCTL_FD_GET_BORROW:
	  *(int*) ptr = sp->fd_borrowed;
	  break;

	case MU_IOCTL_FD_SET_BORROW:
	  sp->fd_borrowed = *(int*) ptr;
	  break;

	default:
	  return EINVAL;
	}
      break;

    case MU_IOCTL_TIMEOUT:
      if (!ptr)
	return EINVAL;
      else
	{
	  unsigned n;
	  struct timeval *tv = ptr;
# define MSEC_PER_SECOND 1000

	  switch (opcode)
	    {
	    case MU_IOCTL_OP_GET:
	      tv->tv_sec = sp->io_timeout / MSEC_PER_SECOND;
	      tv->tv_usec = (sp->io_timeout % MSEC_PER_SECOND) * 1000;
	      break;

	    case MU_IOCTL_OP_SET:
	      if (tv->tv_sec > UINT_MAX / MSEC_PER_SECOND)
		return ERANGE;
	      n = tv->tv_sec * MSEC_PER_SECOND;
	      if (UINT_MAX - n < (unsigned long) tv->tv_usec / 1000)
		return ERANGE;
	      n += tv->tv_usec / 1000;
	      sp->io_timeout = n;
	      gnutls_record_set_timeout (sp->session, n);
	      break;
	      
	    default:
	      return EINVAL;
	    }
	  
/*
  Timeouts:

    gnutls_transport_set_pull_timeout_function (sp->session,
                                                gnutls_system_recv_timeout);

    gnutls_record_set_timeout (sp->session, MS);
    gnutls_handshake_set_timeout (sp->session, MS);
    
 */
	  
	}
      break;

    case MU_IOCTL_TLSSTREAM:
      switch (opcode)
	{
	case MU_IOCTL_TLS_GET_CIPHER_INFO:
	  return get_cipher_info (sp->session, ptr);

	default:
	  return EINVAL;
	}
      break;

    case MU_IOCTL_TCPSTREAM:
      switch (opcode)
	{
	case MU_IOCTL_TCP_GETSOCKNAME:
	  if (!ptr)
	    return EINVAL;
	  else
	    {
	      /* FIXME: Check this */
	      struct mu_sockaddr *addr;
	      int rc = mu_sockaddr_from_socket (&addr, sp->fd[MU_TRANSPORT_INPUT]);
	      if (rc)
		return rc;
	      *(struct mu_sockaddr **)ptr = addr;
	    }
	  break;
	      
	default:
	  return EINVAL;
	}
      break;

    default:
      return ENOSYS;
    }
  return 0;
}
      
static const char *
_tlsfd_error_string (struct _mu_stream *stream, int rc)
{
  if (rc == MU_ERR_TLS)
    {
      struct _mu_tlsfd_stream *sp = (struct _mu_tlsfd_stream *) stream;
      return gnutls_strerror (sp->tls_err);
    }
  return mu_strerror (rc);
}


int
mu_tlsfd_stream_create (mu_stream_t *pstream, int ifd, int ofd,
			struct mu_tls_config const *conf,
			enum mu_tls_type type)
{
  struct _mu_tlsfd_stream *sp;
  int rc;
  mu_stream_t stream;
  int session_type;
  static struct mu_tls_config default_conf = { .handshake_timeout = 60 };
  
  if (!pstream)
    return MU_ERR_OUT_PTR_NULL;
  if (!conf)
    {
      conf = &default_conf;
    }

  if (!mu_init_tls_libs ())
    return ENOSYS;

  if (conf)
    {
      switch (mu_tls_config_check (conf, 1))
	{
	case MU_TLS_CONFIG_OK:
	case MU_TLS_CONFIG_NULL:
	  break;
	case MU_TLS_CONFIG_UNSAFE:
	  return EACCES;
	case MU_TLS_CONFIG_FAIL:
	  return ENOENT;
	}
    }
  
  switch (type)
    {
    case MU_TLS_CLIENT:
      session_type = GNUTLS_CLIENT;
      break;
      
    case MU_TLS_SERVER:
      session_type = GNUTLS_SERVER;
      break;
      
    default:
      return EINVAL;
    }
  
  sp = (struct _mu_tlsfd_stream *)
    _mu_stream_create (sizeof (*sp), MU_STREAM_RDWR);
  if (!sp)
    return ENOMEM;

  sp->session_type = session_type;
  sp->state = state_init;
  sp->session = NULL;
  sp->cred = NULL;

  rc = copy_conf (&sp->conf, conf);
  if (rc)
    {
      free (sp);
      return rc;
    }

  sp->fd[MU_TRANSPORT_INPUT] = ifd;
  sp->fd[MU_TRANSPORT_OUTPUT] = ofd;
  
  sp->stream.read = _tlsfd_read; 
  sp->stream.write = _tlsfd_write;
  //  sp->stream.flush = _tlsfd_flush;
  sp->stream.open = _tlsfd_open; 
  sp->stream.close = _tlsfd_close;
  sp->stream.done = _tlsfd_done; 
  sp->stream.ctl = _tlsfd_ioctl;
  sp->stream.wait = _tlsfd_wait;
  sp->stream.error_string = _tlsfd_error_string;

  stream = (mu_stream_t) sp;
  
  mu_stream_set_buffer (stream, mu_buffer_line, 0);
  rc = mu_stream_open (stream);
  if (rc)
    mu_stream_destroy (&stream);
  else
    *pstream = stream;
  return rc;
}

static int
stream_reset_transport (mu_stream_t str)
{
  mu_transport_t t[2];
  int rc;
  
  t[0] = (mu_transport_t) -1;
  t[1] = NULL;
  rc = mu_stream_ioctl (str, MU_IOCTL_TRANSPORT, MU_IOCTL_OP_SET, t);
  if (rc)
    {
      int b = 1;
      mu_debug (MU_DEBCAT_TLS, MU_DEBUG_ERROR,
		("ioctl(str, MU_IOCTL_TRANSPORT, MU_IOCTL_OP_SET): %s",
		 mu_stream_strerror (str, rc)));
      rc = mu_stream_ioctl (str, MU_IOCTL_FD, MU_IOCTL_FD_SET_BORROW, &b);
      if (rc)
	{
	  mu_debug (MU_DEBCAT_TLS, MU_DEBUG_ERROR,
		    ("ioctl(str, MU_IOCTL_FD, MU_IOCTL_FD_SET_BORROW): %s",
		     mu_stream_strerror (str, rc)));
	  return MU_ERR_TRANSPORT_SET;
	}
    }
  return 0;
}

int
mu_tlsfd_stream2_convert (mu_stream_t *pstream,
			  mu_stream_t istr, mu_stream_t ostr,
			  struct mu_tls_config const *conf,
			  enum mu_tls_type type)
{
  mu_transport_t t[2];
  int ifd, ofd;
  int rc;
  
  rc = mu_stream_ioctl (istr, MU_IOCTL_TRANSPORT, MU_IOCTL_OP_GET, t);
  if (rc)
    {
      mu_debug (MU_DEBCAT_TLS, MU_DEBUG_ERROR,
		("ioctl(istr, MU_IOCTL_TRANSPORT, MU_IOCTL_OP_GET): %s",
		 mu_stream_strerror (istr, rc)));
      return MU_ERR_TRANSPORT_GET;
    }
  ifd = (int) (intptr_t) t[0];

  if (ostr)
    {
      rc = mu_stream_ioctl (ostr, MU_IOCTL_TRANSPORT, MU_IOCTL_OP_GET, t);
      if (rc)
	{
	  mu_debug (MU_DEBCAT_TLS, MU_DEBUG_ERROR,
		    ("ioctl(ostr, MU_IOCTL_TRANSPORT, MU_IOCTL_OP_GET): %s",
		     mu_stream_strerror (ostr, rc)));
	  return MU_ERR_TRANSPORT_GET;
	}
      ofd = (int) (intptr_t) t[0];
    }
  else
    ofd = ifd;

  rc = mu_tlsfd_stream_create (pstream, ifd, ofd, conf, type);
  if (rc)
    {
      mu_debug (MU_DEBCAT_TLS, MU_DEBUG_ERROR,
		("mu_tlsfd_stream_create: %s", mu_strerror (rc)));
      return rc;
    }

  if (stream_reset_transport (istr))
    return MU_ERR_TRANSPORT_SET;
  if (ostr)
    {
      if (stream_reset_transport (ostr))
	return MU_ERR_TRANSPORT_SET;
    }
  return 0;
}

int
mu_starttls (mu_stream_t *pstream, struct mu_tls_config *conf,
	     enum mu_tls_type type)
{
  mu_stream_t transport;
  mu_stream_t tlsstream, stream[2], tstr, istr;
  int rc;

  if (!pstream || !*pstream)
    return EINVAL;
  
  transport = *pstream;
  mu_stream_flush (transport);
  
  /*
   * Find the iostream.
   * Unless transcript is enabled the iostream variable refers to a
   * CRLF filter, and its sub-stream is the iostream object.  If transcript
   * is enabled, the transcript stream is added on top and iostream refers
   * to it.
   *
   * The loop below uses the fact that iostream is the only stream in
   * mailutils that returns *both* transport streams on MU_IOCTL_TOPSTREAM/
   * MU_IOCTL_OP_GET ioctl.  Rest of streams that support MU_IOCTL_TOPSTREAM,
   * return the transport stream in stream[0] and NULL in stream[1].
   */
  tstr = istr = transport;
  while ((rc = mu_stream_ioctl (istr,
				MU_IOCTL_TOPSTREAM, MU_IOCTL_OP_GET,
				stream)) == 0
	 && stream[1] == NULL)
    {
      tstr = istr;
      istr = stream[0];
      mu_stream_unref (istr);
    }

  switch (rc)
    {
    case 0:
      /* OK */
      mu_stream_unref (stream[0]);
      mu_stream_unref (stream[1]);
      break;

    case ENOSYS:
      /* There's no underlying stream. */
      tstr = NULL;
      stream[0] = transport;
      stream[1] = NULL;
      break;

    default:
      /* Error */
      mu_debug (MU_DEBCAT_TLS, MU_DEBUG_ERROR,
		("%s", _("INTERNAL ERROR: cannot locate I/O stream")));
      return MU_ERR_TRANSPORT_GET;
    }

  rc = mu_tlsfd_stream2_convert (&tlsstream, stream[0], stream[1], conf, type);
  if (rc)
    {
      mu_debug (MU_DEBCAT_TLS, MU_DEBUG_ERROR,
		(_("cannot open TLS stream: %s"), mu_strerror (rc)));
      if (rc == MU_ERR_TRANSPORT_SET)
	{	  
	  stream_reset_transport (tlsstream);
	  mu_stream_destroy (&tlsstream);
	  /* NOTE: iostream is unusable now */
	  return MU_ERR_FAILURE;
	}
      return rc;
    }

  if (tstr)
    {
      stream[0] = tlsstream;
      stream[1] = NULL;
      rc = mu_stream_ioctl (tstr, MU_IOCTL_TOPSTREAM, MU_IOCTL_OP_SET, stream);
      if (rc)
	{
	  mu_debug (MU_DEBCAT_TLS, MU_DEBUG_ERROR,
		    (_("INTERNAL ERROR: failed to install TLS stream: %s"),
		     mu_strerror (rc)));
	  return MU_ERR_FAILURE;
	}
      mu_stream_unref (tlsstream);
    }
  else
    {
      mu_stream_destroy (&transport);
      *pstream = tlsstream;
    }
  
  return 0;
}
