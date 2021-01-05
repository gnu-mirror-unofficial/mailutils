/*
  NAME
    mockmta - mock MTA server for use in test suites

  SYNOPSIS
    mockmta [-ad] [-c CERT] [-f CA] [-k KEY] [-p PORT] [-t SEC] [DUMPFILE]

  DESCRIPTION
    Starts a mock MTA, which behaves almost identically to the real one,
    except that it does not actually inject messages to the mail transport
    system.  Instead, each accepted message is logged to the DUMPFILE.

    Being a mailutils test tool, mockmta is written without relying on
    the mailutils libraries.  Only libc and, optionally, GnuTLS functions
    are used.

    No attempts are made to interpret the data supplied during the STMP
    transaction, such as domain names, email addresses, etc, neither is
    the material supplied in the DATA command verified to be a valid
    email message.  Except for being logged to DUMPFILE, these data are
    ignored.

    Incoming SMTP sessions are processed sequentially.  Listening for
    incoming requests is blocked while an SMTP session is active.
    
    The utility listens for incoming requests on localhost.  If the port
    to listen on is not given explicitly via the -p option, mockmta
    selects a random unused port and listens on it.  The selected port
    number is printed on the standard output.
    
    By default, mockmta starts as a foreground process.  If the DUMPFILE
    argument is not supplied, messages will be logged to the standard
    output.

    If the -d option is given, mockmta detaches from the terminal and runs
    in daemon mode.  It prints the PID of the daemon process on the standard
    output.  The daemon will terminate after 60 seconds.  This value can be
    configured using the -t option.  When running in daemon mode, the
    DUMPFILE argument becomes mandatory.

    To enable the STARTTLS ESMTP command, supply the names of the certificate
    (-c CERT) and certificate key (-k KEY) files.

    Output summary

    Depending on the command line options given, mockmta can output port
    number it listens on and the PID of the started daemon process.  The
    four possible variants are summarized in the table below:
    
    1. Neither -d nor -p are given.
    
       Prints the selected port number.
       
    2. -p is given, but -d is not
    
       Nothing is printed.
       
    3. -d is given, but -p is not
    
       Prints two numbers, each on a separate line:
         Port number
         PID
	 
    4. Both -d and -p are given.
    
       Prints PID of the daemon process.
       
  OPTIONS
    -a        Append to DUMPFILE instead of overwriting it.
    -c CERT   Name of the certificate file.
    -d        Daemon mode
    -f CA     Name of certificate authority file.
    -k KEY    Name of the certificate key file.
    -p PORT   Listen on this port.
    -t SEC    Terminate the daemon forcefully after this number of seconds.
              Default is 60.  Valid only in daemon mode (-d).

  DUMP FORMAT
    Each message is represented as a series of records:

    MSGID: <numeric>
      Four-digit sequential message identifier.
    DOMAIN: <string>
      EHLO (or HELO) domain.
    SENDER: <string>
      Sender email address as given by the MAIL command.
    NRCPT: <numeric>
      Number of recipients.
   
    The list of recipients follows this line.  Each record in the list is
    
    RCPT[<I>]: <string>

    where <I> is 0-based index of the recipient in recipient table.

    LENGTH: <N>
      Total length of the data section, including terminating dot and
      newline.  Notice, that line ending is changed from CRLF to LF
      prior to length calculation, so that each line, including the
      dot terminator, ends with one ASCII 10 character.

    This line is followed by <N> bytes representing the material received
    after the DATA SMTP keyword.

    Message dump is terminated by a single LF character.
	      
  EXIT CODES
    0   Success.
    1   Failure (see stderr for details).
    2   Command line usage error.

  BUGS
    At most 32 RCPT commands are allowed.
    
  AUTHOR
    Sergey Poznyakoff <gray@gnu.org>
    
  LICENSE
    This program is part of GNU Mailutils testsuite.
    Copyright (C) 2020-2021 Free Software Foundation, Inc.

    Mockmta is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3, or (at your option)
    any later version.

    Mockmta is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>.

 */
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

char *progname;
int daemon_opt;
int daemon_timeout = 60;
int port;
int msgid = 1;

FILE *logfile;

enum
  {
    EX_OK,
    EX_FAILURE,
    EX_USAGE
  };

static void
terror (char const *fmt, ...)
{
  va_list ap;
  int m;
  static char *fmtbuf = NULL;
  static size_t fmtsize = 0;
  int ec = errno;
  char const *es = NULL;
  size_t len;
  
  for (m = 0; fmt[m += strcspn (fmt + m, "%")]; )
    {
      m++;
      if (fmt[m] == 'm')
	break;
    }      

  len = strlen (fmt) + 1;
  if (fmt[m])
    {
      es = strerror (ec);
      len += strlen (es) - 2;
    }
  if (len > fmtsize)
    {
      fmtsize = len;
      fmtbuf = realloc (fmtbuf, fmtsize);
      if (!fmtbuf)
	{
	  perror ("realloc");
	  exit (EX_FAILURE);
	}
    }

  if (es)
    {
      memcpy (fmtbuf, fmt, m - 1);
      memcpy (fmtbuf + m - 1, es, strlen (es) + 1);
      strcat (fmtbuf, fmt + m + 1);
    }
  else
    strcpy (fmtbuf, fmt);

  fprintf (stderr, "%s: ", progname);
  va_start (ap, fmt);
  vfprintf (stderr, fmtbuf, ap);
  va_end (ap);
  fputc ('\n', stderr);
}

static void
nomemory (void)
{
  terror ("out of memory");
  exit (EX_FAILURE);
}

struct iodrv
{
  int (*drv_read) (void *, char *, size_t, size_t *);
  int (*drv_write) (void *, char *, size_t, size_t *);
  void (*drv_close) (void *);
  const char *(*drv_strerror) (void *, int);
};

#define IOBUFSIZE 1024

struct iobase
{
  struct iodrv *iob_drv;
  char iob_buf[IOBUFSIZE];
  size_t iob_start;
  size_t iob_level;
  int iob_errno;
  int iob_eof;
};

static struct iobase *
iobase_create (struct iodrv *drv, size_t size)
{
  struct iobase *bp;

  bp = calloc (1, size);
  if (!bp)
    nomemory ();

  bp->iob_drv = drv;
  bp->iob_start = 0;
  bp->iob_level = 0;
  bp->iob_errno = 0;
  bp->iob_eof = 0;

  return bp;
}

/* Number of data bytes in buffer */
static inline size_t
iobase_data_bytes (struct iobase *bp)
{
  return bp->iob_level - bp->iob_start;
}

/* Pointer to the first data byte */
static inline char *
iobase_data_start (struct iobase *bp)
{
  return bp->iob_buf + bp->iob_start;
}

static inline void
iobase_data_less (struct iobase *bp, size_t n)
{
  bp->iob_start += n;
  if (bp->iob_start == bp->iob_level)
    {
      bp->iob_start = 0;
      bp->iob_level = 0;
    }
}

static inline int
iobase_data_getc (struct iobase *bp)
{
  char *p;
  if (iobase_data_bytes (bp) == 0)
    return -1;
  p = iobase_data_start (bp);
  iobase_data_less (bp, 1);
  return *p;
}

static inline int
iobase_data_ungetc (struct iobase *bp, int c)
{
  if (bp->iob_start > 0)
    bp->iob_buf[--bp->iob_start] = c;
  else if (bp->iob_level == 0)
    bp->iob_buf[bp->iob_level++] = c;
  else
    return -1;
  return 0;
}

/* Number of bytes available for writing in buffer */
static inline size_t
iobase_avail_bytes (struct iobase *bp)
{
  return IOBUFSIZE - bp->iob_level;
}

/* Pointer to the first byte available for writing */
static inline char *
iobase_avail_start (struct iobase *bp)
{
  return bp->iob_buf + bp->iob_level;
}

static inline void
iobase_avail_less (struct iobase *bp, size_t n)
{
  bp->iob_level += n;
}

/* Fill the buffer */
static inline int
iobase_fill (struct iobase *bp)
{
  int rc;
  size_t n;

  rc = bp->iob_drv->drv_read (bp, iobase_avail_start (bp),
			      iobase_avail_bytes (bp), &n);
  if (rc == 0)
    {
      if (n == 0)
	bp->iob_eof = 1;
      else
	iobase_avail_less (bp, n);
    }
  bp->iob_errno = rc;
  return rc;
}

/* Flush the data available in buffer to external storage */
static inline int
iobase_flush (struct iobase *bp)
{
  int rc;
  size_t n;

  rc = bp->iob_drv->drv_write (bp, iobase_data_start (bp),
			       iobase_data_bytes (bp), &n);
  if (rc == 0)
    iobase_data_less (bp, n);
  bp->iob_errno = rc;
  return rc;
}

static inline char const *
iobase_strerror (struct iobase *bp)
{
  return bp->iob_drv->drv_strerror (bp, bp->iob_errno);
}

static inline int
iobase_eof (struct iobase *bp)
{
  return bp->iob_eof && iobase_data_bytes (bp) == 0;
}

#if 0
/* Not actually used.  Provided for completeness sake. */
static ssize_t
iobase_read (struct iobase *bp, char *buf, size_t size)
{
  size_t len = 0;

  while (size)
    {
      size_t n = iobase_data_bytes (bp);
      if (n == 0)
	{
	  if (bp->iob_eof)
	    break;
	  if (iobase_fill (bp))
	    break;
	  continue;
	}
      if (n > size)
	n = size;
      memcpy (buf, iobase_data_start (bp), n);
      iobase_data_less (bp, n);
      len += n;
      buf += n;
      size -= n;
    }

  if (len == 0 && bp->iob_errno)
    return -1;
  return len;
}
#endif

static ssize_t
iobase_readln (struct iobase *bp, char *buf, size_t size)
{
  size_t len = 0;
  int cr_seen = 0;

  size--;
  while (len < size)
    {
      int c = iobase_data_getc (bp);
      if (c < 0)
	{
	  if (bp->iob_eof)
	    break;
	  if (iobase_fill (bp))
	    break;
	  continue;
	}
      if (c == '\r')
	{
	  cr_seen = 1;
	  continue;
	}
      if (c != '\n' && cr_seen)
	{
	  buf[len++] = '\r';
	  if (len == size)
	    {
	      if (iobase_data_ungetc (bp, c))
		abort ();
	      break;
	    }
	}
      cr_seen = 0;
      buf[len++] = c;
      if (c == '\n')
	break;
    }
  if (len == 0 && bp->iob_errno)
    return -1;
  buf[len] = 0;
  return len;
}

static ssize_t
iobase_write (struct iobase *bp, char *buf, size_t size)
{
  size_t len = 0;

  while (size)
    {
      size_t n = iobase_avail_bytes (bp);
      if (n == 0)
	{
	  if (iobase_flush (bp))
	    break;
	  continue;
	}
      if (n > size)
	n = size;
      memcpy (iobase_avail_start (bp), buf + len, n);
      iobase_avail_less (bp, n);
      len += n;
      size -= n;
    }
  if (len == 0 && bp->iob_errno)
    return -1;
  return len;
}

static ssize_t
iobase_writeln (struct iobase *bp, char *buf, size_t size)
{
  size_t len = 0;
  
  while (size)
    {
      char *p = memchr (buf, '\n', size);
      size_t n = p ? p - buf + 1 : size;
      ssize_t rc = iobase_write (bp, buf, n);
      if (rc <= 0)
	break;
      if (p && iobase_flush (bp))
	break;
      buf = p;
      len += rc;
      size -= rc;
    }
  if (len == 0 && bp->iob_errno)
    return -1;
  return len;
}

static void
iobase_close (struct iobase *bp)
{
  bp->iob_drv->drv_close (bp);
  free (bp);
}

/* File-descriptor I/O streams */
struct iofile
{
  struct iobase base;
  int fd;
};

static int
iofile_read (void *sd, char *data, size_t size, size_t *nbytes)
{
  struct iofile *bp = sd;
  ssize_t n = read (bp->fd, data, size);
  if (n == -1)
    return errno;
  if (nbytes)
    *nbytes = n;
  return 0;
}

static int
iofile_write (void *sd, char *data, size_t size, size_t *nbytes)
{
  struct iofile *bp = sd;
  ssize_t n = write (bp->fd, data, size);
  if (n == -1)
    return errno;
  if (nbytes)
    *nbytes = n;
  return 0;
}

static const char *
iofile_strerror (void *sd, int rc)
{
  return strerror (rc);
}

static void
iofile_close (void *sd)
{
  struct iofile *bp = sd;
  close (bp->fd);
}

static struct iodrv iofile_drv = {
  iofile_read,
  iofile_write,
  iofile_close,
  iofile_strerror
};

struct iobase *
iofile_create (int fd)
{
  struct iofile *bp = (struct iofile *) iobase_create (&iofile_drv, sizeof (*bp));
  bp->fd = fd;
  return (struct iobase*) bp;
}

enum
  {
    IO2_RD,
    IO2_WR
  };

static void disable_starttls (void);

#ifdef WITH_TLS
# include <gnutls/gnutls.h>

/* TLS support */
char *tls_cert;			/* TLS sertificate */
char *tls_key;			/* TLS key */
char *tls_cafile;

static inline int
set_tls_opt (int c)
{
  switch (c)
    {
    case 'c':
      tls_cert = optarg;
      break;
      
    case 'k':
      tls_key = optarg;
      break;
      
    case 'f':
      tls_cafile = optarg;
      break;

    default:
      return 1;
    }
  return 0;
}

static inline int
enable_tls (void)
{
  return tls_cert != NULL && tls_key != NULL;
}

/* TLS streams */
struct iotls
{
  struct iobase base;
  gnutls_session_t sess;
  int fd[2];
};

static int
iotls_read (void *sd, char *data, size_t size, size_t *nbytes)
{
  struct iotls *iob = sd;
  int rc;
  
  do
    rc = gnutls_record_recv (iob->sess, data, size);
  while (rc == GNUTLS_E_AGAIN || rc == GNUTLS_E_INTERRUPTED);
  if (rc >= 0)
    {
      if (nbytes)
	*nbytes = rc;
      return 0;
    }
  return rc;  
}

static int
iotls_write (void *sd, char *data, size_t size, size_t *nbytes)
{
  struct iotls *iob = sd;
  int rc;

  do
    rc = gnutls_record_send (iob->sess, data, size);
  while (rc == GNUTLS_E_INTERRUPTED || rc == GNUTLS_E_AGAIN);
  if (rc >= 0)
    {
      if (nbytes)
	*nbytes = rc;
      return 0;
    }
  return rc;
}

static const char *
iotls_strerror (void *sd, int rc)
{
  //  struct iotls *iob = sd;
  return gnutls_strerror (rc);
}

static void
iotls_close (void *sd)
{
  struct iotls *iob = sd;
  gnutls_bye (iob->sess, GNUTLS_SHUT_RDWR);
  gnutls_deinit (iob->sess);
}

static struct iodrv iotls_drv = {
  iotls_read,
  iotls_write,
  iotls_close,
  iotls_strerror
};

static gnutls_dh_params_t dh_params;
static gnutls_certificate_server_credentials x509_cred;

static void
_tls_cleanup_x509 (void)
{
  if (x509_cred)
    gnutls_certificate_free_credentials (x509_cred);
}

#define DH_BITS 512
static void
generate_dh_params (void)
{
  gnutls_dh_params_init (&dh_params);
  gnutls_dh_params_generate2 (dh_params, DH_BITS);
}

static int
tls_init (void)
{
  int rc;
  
  if (!enable_tls())
    return -1;

  gnutls_global_init ();
  atexit (gnutls_global_deinit);
  gnutls_certificate_allocate_credentials (&x509_cred);
  atexit (_tls_cleanup_x509);
  if (tls_cafile)
    {
      rc = gnutls_certificate_set_x509_trust_file (x509_cred,
						   tls_cafile,
						   GNUTLS_X509_FMT_PEM);
      if (rc < 0)
	{
	  terror ("%s: %s", tls_cafile, gnutls_strerror (rc));
	  return -1;
	}
    }

  rc = gnutls_certificate_set_x509_key_file (x509_cred,
					     tls_cert, tls_key,
					     GNUTLS_X509_FMT_PEM);
  if (rc < 0)
    {
      terror ("error reading certificate files: %s", gnutls_strerror (rc));
      return -1;
    }
  
  generate_dh_params ();
  gnutls_certificate_set_dh_params (x509_cred, dh_params);

  return 0;
}

static ssize_t
_tls_fd_pull (gnutls_transport_ptr_t fd, void *buf, size_t size)
{
  struct iotls *bp = fd;
  int rc;
  do
    {
      rc = read (bp->fd[IO2_RD], buf, size);
    }
  while (rc == -1 && errno == EAGAIN);
  return rc;
}

static ssize_t
_tls_fd_push (gnutls_transport_ptr_t fd, const void *buf, size_t size)
{
  struct iotls *bp = fd;
  int rc;
  do
    {
      rc = write (bp->fd[IO2_WR], buf, size);
    }
  while (rc == -1 && errno == EAGAIN);
  return rc;
}

struct iobase *
iotls_create (int in, int out)
{
  struct iotls *bp = (struct iotls *) iobase_create (&iotls_drv, sizeof (*bp));
  int rc;

  bp->fd[IO2_RD] = in;
  bp->fd[IO2_WR] = out;
  gnutls_init (&bp->sess, GNUTLS_SERVER);
  gnutls_set_default_priority (bp->sess);
  gnutls_credentials_set (bp->sess, GNUTLS_CRD_CERTIFICATE, x509_cred);
  gnutls_certificate_server_set_request (bp->sess, GNUTLS_CERT_REQUEST);
  gnutls_dh_set_prime_bits (bp->sess, DH_BITS);

  gnutls_transport_set_pull_function (bp->sess, _tls_fd_pull);
  gnutls_transport_set_push_function (bp->sess, _tls_fd_push);

  gnutls_transport_set_ptr2 (bp->sess,
			     (gnutls_transport_ptr_t) bp,
			     (gnutls_transport_ptr_t) bp);
  rc = gnutls_handshake (bp->sess);
  if (rc < 0)
    {
      gnutls_deinit (bp->sess);
      gnutls_perror (rc);
      free (bp);
      return NULL;
    }

  return (struct iobase *)bp;
}
#else
static inline int set_tls_opt (int c) {
  terror ("option -%c not supported: program compiled without support for TLS",
	  c);
  return 1;
}
static inline int enable_tls(void) { return 0; }
static inline int tls_init (void) { return -1; }
static inline struct iobase *iotls_create (int in, int out) { return NULL; }
#endif

/* Two-way I/O */
struct io2
{
  struct iobase base;
  struct iobase *iob[2];
};

static int
io2_read (void *sd, char *data, size_t size, size_t *nbytes)
{
  struct io2 *iob = sd;
  ssize_t n = iobase_readln (iob->iob[IO2_RD], data, size);
  if (n < 0)
    return -(1 + IO2_RD);
  *nbytes = n;
  return 0;
}

static int
io2_write (void *sd, char *data, size_t size, size_t *nbytes)
{
  struct io2 *iob = sd;
  ssize_t n = iobase_writeln (iob->iob[IO2_WR], data, size);
  if (n < 0)
    return -(1 + IO2_WR);
  *nbytes = n;
  return 0;
}

static char const *
io2_strerror (void *sd, int rc)
{
  struct io2 *iob = sd;
  int n = -rc - 1;
  switch (n)
    {
    case IO2_RD:
    case IO2_WR:
      return iobase_strerror (iob->iob[n]);

    default:
      return "undefined error";
    }
}

static void
io2_close (void *sd)
{
  struct io2 *iob = sd;
  iobase_close (iob->iob[IO2_RD]);  
  iobase_close (iob->iob[IO2_WR]);
}

static struct iodrv io2_drv = {
  io2_read,
  io2_write,
  io2_close,
  io2_strerror
};

struct iobase *
io2_create (struct iobase *in, struct iobase *out)
{
  struct io2 *bp = (struct io2 *) iobase_create (&io2_drv, sizeof (*bp));
  bp->iob[IO2_RD] = in;
  bp->iob[IO2_WR] = out;
  return (struct iobase*) bp;
}

/* SMTP implementation */
enum smtp_state
  {
    STATE_ERR, // Reserved
    STATE_INIT,
    STATE_EHLO,
    STATE_MAIL,
    STATE_RCPT,
    STATE_DATA,
    STATE_QUIT,
    MAX_STATE
  };

#define MAX_RCPT 32

struct smtp
{
  enum smtp_state state;
  struct iobase *iob;
  char buf[IOBUFSIZE];
  char *arg;
  int capa_mask;
  char *helo;
  char *sender;
  char *rcpt[MAX_RCPT];
  int nrcpt;
  char *data_buf;
  size_t data_len;
  size_t data_size;
};

static void
smtp_io_send (struct iobase *iob, int code, char *fmt, ...)
{
  va_list ap;
  char buf[IOBUFSIZE];
  int n;
  
  snprintf (buf, sizeof buf, "%3d ", code);
  va_start (ap, fmt);
  n = vsnprintf (buf + 4, sizeof buf - 6, fmt, ap);
  va_end (ap);
  n += 4;
  buf[n++] = '\r';
  buf[n++] = '\n';
  if (iobase_writeln (iob, buf, n) < 0)
    {
      terror ("iobase_writeln: %s", iobase_strerror (iob));
      exit (EX_FAILURE);
    }
}

static void
smtp_io_mlsend (struct iobase *iob, int code, char const **av)
{
  char buf[IOBUFSIZE];
  size_t n;
  int i;

  snprintf (buf, sizeof buf, "%3d", code);
  for (i = 0; av[i]; i++)
    {
      n = snprintf (buf, sizeof(buf), "%3d%c%s\r\n",
		    code, av[i+1] ? '-' : ' ', av[i]);
      if (iobase_writeln (iob, buf, n) < 0)
	{
	  terror ("iobase_writeln: %s", iobase_strerror (iob));
	  exit (EX_FAILURE);
	}
    }      
}

static void
smtp_reset (struct smtp *smtp, int state)
{
  switch (state)
    {
    case STATE_INIT:
      free (smtp->helo);
      smtp->helo = NULL;
      /* FALL THROUGH */
    case STATE_MAIL:
      free (smtp->sender);
      smtp->sender = NULL;
      /* FALL THROUGH */
    case STATE_RCPT:
      {
	int i;
	for (i = 0; i < smtp->nrcpt; i++)
	  free (smtp->rcpt[i]);
	smtp->nrcpt = 0;
      }
      /* FALL THROUGH */
    case STATE_DATA:
      free (smtp->data_buf);
      smtp->data_buf = NULL;
      smtp->data_len = 0;
      smtp->data_size = 0;
    }
}  

void
smtp_end (struct smtp *smtp)
{
  smtp_io_send (smtp->iob, 221, "Bye");
  iobase_close (smtp->iob);
  smtp_reset (smtp, STATE_INIT);
  smtp->iob = NULL;
}

enum smtp_keyword
  {
    KW_HELP,
    KW_RSET,
    KW_EHLO,
    KW_HELO,
    KW_MAIL,
    KW_RCPT,
    KW_DATA,
    KW_STARTTLS,
    KW_QUIT,
    MAX_KW
  };

static char *smtp_keyword_trans[MAX_KW] = {
  [KW_HELP] = "HELP",
  [KW_RSET] = "RSET",
  [KW_EHLO] = "EHLO",
  [KW_HELO] = "HELO",
  [KW_MAIL] = "MAIL",
  [KW_RCPT] = "RCPT",
  [KW_DATA] = "DATA",
  [KW_STARTTLS] = "STARTTLS",
  [KW_QUIT] = "QUIT"
};

static int
smtp_keyword_find (char const *kw)
{
  int i;
  for (i = 0; i < MAX_KW; i++)
    if (strcasecmp (kw, smtp_keyword_trans[i]) == 0)
      return i;
  return -1;
}

static int
smtp_help (struct smtp *smtp)
{
  smtp_io_send (smtp->iob, 214, "http://www.ietf.org/rfc/rfc2821.txt");
  return 0;
}

static int
smtp_rset (struct smtp *smtp)
{
  if (smtp->arg)
    {
      smtp_io_send (smtp->iob, 501, "rset does not take arguments");
      return -1;
    }
  smtp_io_send (smtp->iob, 250, "Reset state");

  smtp_reset (smtp, STATE_INIT);

  return 0;
}

enum
  {
    CAPA_PIPELINING,
    CAPA_STARTTLS,
    CAPA_HELP,
    MAX_CAPA
  };

static char const *capa_str[] = {
  "PIPELINING",
  "STARTTLS",
  "HELP"
};

#define CAPA_MASK(n) (1<<(n))

static int
smtp_ehlo (struct smtp *smtp)
{
  char const *capa[MAX_CAPA+2];
  int i, j;
  
  if (!smtp->arg)
    {
      smtp_io_send (smtp->iob, 501, "ehlo requires domain address");
      return -1;
    }

  capa[0] = "localhost Mock MTA pleased to meet you";
  for (i = 0, j = 1; i < MAX_CAPA; i++)
    if (!(smtp->capa_mask & CAPA_MASK (i)))
      capa[j++] = capa_str[i];
  capa[j] = NULL;
  smtp_io_mlsend (smtp->iob, 250, capa);

  smtp_reset (smtp, STATE_INIT);
  if ((smtp->helo = strdup (smtp->arg)) == NULL)
    nomemory ();
  return 0;
}

static int
smtp_starttls (struct smtp *smtp)
{
  struct io2 *orig_iob = (struct io2 *) smtp->iob;
  struct iofile *inb = (struct iofile *)orig_iob->iob[IO2_RD];
  struct iofile *outb = (struct iofile *)orig_iob->iob[IO2_WR];
  struct iobase *iob;

  if (smtp->arg)
    {
      smtp_io_send (smtp->iob, 501, "Syntax error (no parameters allowed)");
      return -1;
    }
  
  smtp_io_send (smtp->iob, 220, "Ready to start TLS");
  
  iob = iotls_create (inb->fd, outb->fd);

  if (iob)
    {
      free (inb);
      free (outb);
      free (orig_iob);
      smtp->iob = iob;
      disable_starttls ();
      smtp->capa_mask |= CAPA_MASK (CAPA_STARTTLS);
    }
  else
    {
      free (iob);
      exit (EX_FAILURE);
    }
  return 0;
}

static int
smtp_quit (struct smtp *smtp)
{
  return 0;
}

static int
smtp_helo (struct smtp *smtp)
{
  if (!smtp->arg)
    {
      smtp_io_send (smtp->iob, 501, "helo requires domain address");
      return -1;
    }
  smtp_io_send (smtp->iob, 250, "localhost Mock MTA pleased to meet you");

  smtp_reset (smtp, STATE_INIT);
  if ((smtp->helo = strdup (smtp->arg)) == NULL)
    nomemory ();
  return 0;  
}

static int
smtp_mail (struct smtp *smtp)
{
  static char from_str[] = "FROM:";
  static size_t from_len = sizeof(from_str) - 1;
  char *p;
  
  if (!smtp->arg)
    {
      smtp_io_send (smtp->iob, 501, "mail requires email address");
      return -1;
    }
  if (strncasecmp (smtp->arg, from_str, from_len))
    {
      smtp_io_send (smtp->iob, 501, "syntax error");
      return -1;
    }
  p = smtp->arg + from_len;
  while (*p && (*p == ' ' || *p == '\t'))
    p++;
  if (!*p)
    {
      smtp_io_send (smtp->iob, 501, "mail requires email address");
      return -1;
    }
  smtp_reset (smtp, STATE_MAIL);
  if ((smtp->sender = strdup (p)) == NULL)
    nomemory ();
  smtp_io_send (smtp->iob, 250, "Sender ok");
  return 0;
}

static int
smtp_rcpt (struct smtp *smtp)
{
  static char to_str[] = "TO:";
  static size_t to_len = sizeof(to_str) - 1;
  char *p;
  
  if (!smtp->arg)
    {
      smtp_io_send (smtp->iob, 501, "rcpt requires email address");
      return -1;
    }
  if (strncasecmp (smtp->arg, to_str, to_len))
    {
      smtp_io_send (smtp->iob, 501, "syntax error");
      return -1;
    }
  p = smtp->arg + to_len;
  while (*p && (*p == ' ' || *p == '\t'))
    p++;
  if (!*p)
    {
      smtp_io_send (smtp->iob, 501, "to requires email address");
      return -1;
    }
  if (smtp->nrcpt == MAX_RCPT)
    {
      smtp_io_send (smtp->iob, 501, "too many recipients");
      return -1;
    }
  if ((smtp->rcpt[smtp->nrcpt] = strdup (p)) == NULL)
    nomemory ();
  smtp->nrcpt++;
  smtp_io_send (smtp->iob, 250, "Recipient ok");
  return 0;
}

static void
smtp_data_save (struct smtp *smtp)
{
  size_t len = strlen (smtp->buf);
  while (smtp->data_len + len > smtp->data_size)
    {
      char *p;
      size_t n = smtp->data_size;
      if (!smtp->data_buf)
	{
	  n = smtp->data_len + len;
	}
      else
	{
	  if ((size_t)-1 / 3 * 2 <= n)
	    nomemory ();
	  n += (n + 1) / 2;
	}
      p = realloc (smtp->data_buf, n);
      if (!p)
	nomemory ();
      smtp->data_buf = p;
      smtp->data_size = n;
    }
  memcpy (smtp->data_buf + smtp->data_len, smtp->buf, len);
  smtp->data_len += len;
}

static int
smtp_data (struct smtp *smtp)
{
  ssize_t n;
  int i;
  
  smtp_io_send (smtp->iob, 354,
		"Enter mail, end with \".\" on a line by itself");
  fprintf (logfile, "MSGID: %04d\n", msgid);
  fprintf (logfile, "DOMAIN: %s\n", smtp->helo);
  fprintf (logfile, "SENDER: %s\n", smtp->sender);
  fprintf (logfile, "NRCPT: %d\n", smtp->nrcpt);
  for (i = 0; i < smtp->nrcpt; i++)
    fprintf (logfile, "RCPT[%d]: %s\n", i, smtp->rcpt[i]);
  
  while (1)
    {
      char *p;
      n = iobase_readln (smtp->iob, smtp->buf, sizeof (smtp->buf));
      if (n <= 0)
	{
	  smtp->state = STATE_QUIT;
	  return -1;
	}
      smtp_data_save (smtp);
      if (smtp->buf[n-1] == '\n')
	smtp->buf[--n] = 0;
      else
	{
	  //FIXME
	  terror ("line too long");
	  exit (EX_FAILURE);
	}
      p = smtp->buf;
      if (*p == '.')
	{
	  if (p[1] == 0)
	    break;
	  if (p[1] == '.')
	    p++;
	}
    }
  fprintf (logfile, "LENGTH: %lu\n", (unsigned long)smtp->data_len);
  fwrite (smtp->data_buf, smtp->data_len, 1, logfile);
  fputc ('\n', logfile);
  fflush (logfile);
  smtp_io_send (smtp->iob, 250, "%04d Message accepted for delivery", msgid);
  msgid++;
  return 0;
}


struct smtp_transition
{
  int new_state;
  int (*handler) (struct smtp *);
};

static struct smtp_transition smtp_transition_table[MAX_STATE][MAX_KW] = {
  [STATE_INIT] = {
    [KW_HELP] = { STATE_INIT, smtp_help },
    [KW_RSET] = { STATE_INIT, smtp_rset },
    [KW_HELO] = { STATE_EHLO, smtp_helo },
    [KW_EHLO] = { STATE_EHLO, smtp_ehlo },
    [KW_QUIT] = { STATE_QUIT, smtp_quit }
  },
  [STATE_EHLO] = {
    [KW_HELP] = { STATE_EHLO, smtp_help },
    [KW_RSET] = { STATE_INIT, smtp_rset },
    [KW_HELO] = { STATE_EHLO, smtp_helo },
    [KW_EHLO] = { STATE_EHLO, smtp_ehlo },
    [KW_MAIL] = { STATE_MAIL, smtp_mail },
    [KW_STARTTLS] = { STATE_EHLO, smtp_starttls },
    [KW_QUIT] = { STATE_QUIT, smtp_quit }
  },
  [STATE_MAIL] = {
    [KW_HELP] = { STATE_MAIL, smtp_help },
    [KW_RSET] = { STATE_INIT, smtp_rset },
    [KW_RCPT] = { STATE_RCPT, smtp_rcpt },
    [KW_HELO] = { STATE_EHLO, smtp_helo },
    [KW_EHLO] = { STATE_EHLO, smtp_ehlo },
    [KW_QUIT] = { STATE_QUIT, smtp_quit }
  },
  [STATE_RCPT] = {
    [KW_HELP] = { STATE_RCPT, smtp_help },
    [KW_RSET] = { STATE_INIT, smtp_rset },
    [KW_RCPT] = { STATE_RCPT, smtp_rcpt },
    [KW_HELO] = { STATE_EHLO, smtp_helo },
    [KW_EHLO] = { STATE_EHLO, smtp_ehlo },
    [KW_DATA] = { STATE_EHLO, smtp_data },
    [KW_QUIT] = { STATE_QUIT, smtp_quit }
  },
};  

static void
disable_starttls (void)
{
#ifdef WITH_TLS
  tls_cert = tls_key = tls_cafile = NULL;
#endif
  smtp_transition_table[STATE_EHLO][KW_STARTTLS].new_state = STATE_ERR;
}

static void
do_smtp (int fd)
{
  struct iobase *iob = io2_create (iofile_create (fd), iofile_create (fd));
  struct smtp smtp;
  struct smtp_transition *trans;
  
  smtp.state = STATE_INIT;
  smtp.iob = iob;
  smtp.capa_mask = 0;
  if (!enable_tls ())
    smtp.capa_mask |= CAPA_MASK (CAPA_STARTTLS);
  smtp.helo = NULL;
  smtp.sender = NULL;
  smtp.nrcpt = 0;
  smtp.data_buf = NULL;
  smtp.data_len = 0;
  smtp.data_size = 0;
  
  smtp_io_send (smtp.iob, 220, "Ready");
  while (smtp.state != STATE_QUIT)
    {
      size_t i;
      int kw;
      int new_state;
      ssize_t n = iobase_readln (smtp.iob, smtp.buf, sizeof (smtp.buf));
      if (n <= 0)
	break;
      smtp.buf[--n] = 0;
      i = strcspn (smtp.buf, " \t");
      if (smtp.buf[i])
	{
	  smtp.buf[i++] = 0;
	  while (i < n && (smtp.buf[i] == ' ' || smtp.buf[i] == '\t'))
	    i++;
	  if (smtp.buf[i])
	    smtp.arg = &smtp.buf[i];
	  else
	    smtp.arg = NULL;
	}
      else
	smtp.arg = NULL;

      kw = smtp_keyword_find (smtp.buf);
      if (kw == -1)
	{
	  smtp_io_send(smtp.iob, 500, "Command unrecognized");
	  continue;
	}

      trans = &smtp_transition_table[smtp.state][kw];
      new_state = trans->new_state;
      if (new_state == STATE_ERR)
	{
	  smtp_io_send(smtp.iob, 500, "Command not valid");
	  continue;
	}

      if (trans->handler (&smtp))
	continue;

      smtp.state = new_state;
    }
	  
  smtp_end (&smtp);
}

static int
mta_open (int port)
{
  int on = 1;
  struct sockaddr_in address;
  int fd;

  fd = socket (PF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    {
      terror ("socket: %m");
      exit (EX_FAILURE);
    }

  setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));

  memset (&address, 0, sizeof (address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  if (port)
    address.sin_port = htons (port);
  else
    address.sin_port = 0;
  
  if (bind (fd, (struct sockaddr *) &address, sizeof (address)) < 0)
    {
      close (fd);
      terror ("bind: %m");
      exit (EX_FAILURE);
    }
  
  listen (fd, 5);

  if (!port)
    {
      socklen_t len = sizeof (address);
      int rc = getsockname (fd, (struct sockaddr *) &address, &len);
      if (rc)
	{
	  close (fd);
	  terror ("getsockname: %m");
	  exit (EX_FAILURE);
	}
      port = ntohs (address.sin_port);
      printf ("%d\n", port);
    }
  return fd;
}

void
mta_run (int fd)
{
  while (1)
    {
      int sfd;
      struct sockaddr_in remote_addr;
      socklen_t len = sizeof (remote_addr);
      
      if ((sfd = accept (fd, (struct sockaddr *) &remote_addr, &len)) < 0)
	{
	  terror ("accept: %m");
	  exit (EX_FAILURE);
	}

      do_smtp (sfd);
      close (sfd);
    }
}

int
main (int argc, char **argv)
{
  int c;
  int fd;
  int append_opt = 0;
  
  progname = argv[0];
  
  while ((c = getopt (argc, argv, "adc:f:k:p:t:")) != EOF)
    {
      switch (c)
	{
	case 'a':
	  append_opt = 1;
	  break;
	  
	case 'd':
	  daemon_opt = 1;
	  break;

	case 'p':
	  port = atoi (optarg);
	  break;

	case 't':
	  daemon_timeout = atoi (optarg);
	  if (daemon_timeout == 0)
	    {
	      terror ("wrong timeout value");
	      exit (EX_USAGE);
	    }
	  break;
	  
	default:
	  if (set_tls_opt (c))
	    exit (EX_USAGE);
	}
    }

  argc -= optind;
  argv += optind;

  if (argc == 0)
    {
      if (daemon_opt)
	{
	  terror ("DUMPFILE is required in daemon mode");
	  exit (EX_USAGE);
	}
      logfile = stdout;
    }
  else if (argc == 1)
    {
      logfile = fopen (argv[0], append_opt ? "a" : "w");
      if (!logfile)
	{
	  terror ("can't open %s for writing: %m", argv[0]);
	  exit (EX_FAILURE);
	}
    }
  else
    {
      terror ("too many arguments");
      exit (EX_USAGE);
    }
  
  if (tls_init ())
    {
      disable_starttls ();
    }

  fd = mta_open (port);
  if (daemon_opt)
    {
      pid_t pid = fork ();
      if (pid == -1)
	{
	  terror ("fork: %m");
	  exit (EX_FAILURE);
	}
      if (pid)
	{
	  /* master */
	  printf ("%lu\n", (unsigned long) pid);
	  return 0;
	}
      else
	{
	  /* child */
	  /* Close unneded descriptors */
	  int i;
	  for (i = 0; i < sysconf (_SC_OPEN_MAX); i++)
	    {
	      if (i != fileno (logfile) && i != fd)
		close (i);
	    }
	  /* Provide replacements for the three standard streams */
	  if (open ("/dev/null", O_RDONLY) != 0 ||
	      dup2 (fileno (logfile), 1) == -1 ||
	      dup2 (fileno (logfile), 2) == -1)
	    abort ();
	  /* Impose the runtime timeout */
	  alarm (daemon_timeout);
	}
    }

  mta_run (fd);
  exit (EX_OK);
}
