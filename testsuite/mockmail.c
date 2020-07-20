/*
  NAME
    mockmail - mock Sendmail binary for use in test suites

  SYNOPSIS
    mockmta [-bm] [-f EMAIL] [-t] [-oi] [EMAIL ...]

  DESCRIPTION
    Mimicks the behavior of "sendmail -bm".  Instead of delivering
    the message, dumps it to the file "mail.dump".  The location of
    the dump file can be changed by setting the MAIL_DUMP environment
    variable.

    Being a mailutils test tool, mockname is written without relying on
    the mailutils libraries.  Only libc is needed.

    Message to be "delivered" is read from the standard input.
    
    Only rudimental message parsing is performed, as necessary for
    the functioning of the -t option.  Similarly, email address parser
    is very naive.

  OPTIONS
    -bm       Ignored for compatibility with Sendmail.
    -f EMAIL  Sets sender email address.
    -t        Read recipients from the message.
    -oi       Don't expect the incoming message to be terminated with
              a dot.  This also turns off dot unstuffing.

  ENVIRONMENT
  
    MAIL_DUMP
      Name of the dump file ("mail.dump")
      
    MAIL_DUMP_APPEND
      When set, append new entries to the mail.dump file, instead of
      overwriting it.

  DUMP FORMAT
    The message is represented as a series of records:

    MSGID: 0001
      This record is for compatibility with mockmta.
    SENDER: <string>
      Sender email address as given by the -f option, if given.
    NRCPT: <numeric>
      Number of recipients.
   
    The list of recipients follows this line.  Each record in the list is
    
    RCPT[<I>]: <string>

    where <I> is 0-based index of the recipient in recipient table.

    LENGTH: <N>
      Total length of the data section, after eventual dot-unstuffing.
      This does not include terminating dot (if such was present and
      the -oi option was not given).

      This line is followed by <N> bytes representing the material received
      from the standard input.

    Message dump is terminated by a single LF character.

  BUGS
    At most 32 recipients are allowed.
    Header and address parsing is rudimental.
    
  AUTHOR
    Sergey Poznyakoff <gray@gnu.org>
    
  LICENSE
    This program is part of GNU Mailutils testsuite.
    Copyright (C) 2020 Free Software Foundation, Inc.

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

char *progname;
FILE *logfile;

char *from_person = NULL; /* Set the name of the `from' person */
int read_recipients = 0;  /* Read the message for recipients */
int dot = 1;              /* Message is terminated by a lone dot on a line */

#define MAXRCPT 32

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

static inline int
is_blank (int c)
{
  return c == ' ' || c == '\t';
}

enum { H_INIT, H_HEADER, H_NL, B_INIT, B_NL, B_DOT };

struct header_map
{
  struct header_map *prev, *next;  /* List of elements */
  size_t hstart;        /* Header start offset */
  size_t nlen;          /* Header name length */
  size_t vstart;        /* Value start offset */
  size_t end;           /* Header end offset (points past the final LF) */
};

struct message
{
  char *buf_ptr;
  size_t buf_len;
  size_t buf_size;
  size_t line_len;
  
  char *rcpt[MAXRCPT];
  int nrcpt;

  size_t header_len;
  struct header_map *header_head, *header_tail;
  
  int state;
};

static void
msg_header_add (struct message *msg)
{
  struct header_map *hmap;
  char *hdr;
  size_t i;
  
  hmap = calloc (1, sizeof hmap[0]);
  if (!hmap)
    nomemory ();
  hmap->hstart = msg->buf_len - msg->line_len;
  hdr = msg->buf_ptr + hmap->hstart;
  for (i = msg->line_len - 1; i > 0; i--)
    if (!is_blank (hdr[i-1]))
      break;
  hmap->nlen = i;
  hmap->vstart = msg->buf_len;
  hmap->next = NULL;
  hmap->prev = msg->header_tail;
  if (msg->header_tail)
    msg->header_tail->next = hmap;
  else
    msg->header_head = hmap;
  msg->header_tail = hmap;
}

static void
msg_header_update (struct message *msg)
{
  struct header_map *hmap = msg->header_tail;
  hmap->end = msg->buf_len;
  while (hmap->vstart < hmap->end && is_blank (msg->buf_ptr[hmap->vstart]))
    hmap->vstart++;
}

static void
msg_add_rcpt (struct message *msg, char const *email)
{
  if (msg->nrcpt == MAXRCPT)
    {
      terror ("too many recipients");
      exit (EX_USAGE);
    }
  if ((msg->rcpt[msg->nrcpt] = strdup (email)) == NULL)
    nomemory ();
  msg->nrcpt++;
}

static void
msg_add_char (struct message *msg, int c)
{
  while (msg->buf_len + 1 > msg->buf_size)
    {
      char *p;
      size_t n = msg->buf_size;
      if (!msg->buf_ptr)
	{
	  n = 64;
	}
      else
	{
	  if ((size_t)-1 / 3 * 2 <= n)
	    nomemory ();
	  n += (n + 1) / 2;
	}
      p = realloc (msg->buf_ptr, n);
      if (!p)
	nomemory ();
      msg->buf_ptr = p;
      msg->buf_size = n;
    }
  msg->buf_ptr[msg->buf_len++] = c;
  if (c == '\n')
    msg->line_len = 0;
  else
    msg->line_len++;
}

int
main (int argc, char **argv)
{
  struct message msg;
  int c;
  char *filename;
  struct header_map *hmap;
  
  progname = argv[0];
  
  while ((c = getopt (argc, argv, "b:f:to:")) != EOF)
    {
      switch (c)
	{
	case 'b':
	  if (strcmp (optarg, "m"))
	    {
	      terror ("-b%s not supported", optarg);
	      exit (EX_USAGE);
	    }
	  break;
	  
	case 'f':
	  from_person = optarg;
	  break;

	case 't':
	  read_recipients = 1;
	  break;

	case 'o':
	  switch (optarg[0])
	    {
	    case 'i':
	      dot = 0;
	      break;
	  
	    default:
	      /* IGNORED */ ;
	    }
	  break;

	default:
	  exit (EX_USAGE);
	}
    }

  memset (&msg, 0, sizeof (msg));
  
  for (c = optind; c < argc; c++)
    msg_add_rcpt (&msg, argv[c]);

  msg.state = H_INIT;

  while ((c = getchar ()) != EOF)
    {
      msg_add_char (&msg, c);
      switch (msg.state)
	{
	case H_INIT:
	  if (c == ':')
	    {
	      msg_header_add (&msg);
	      msg.state = H_HEADER;
	    }
	  break;

	case H_HEADER:
	  if (c == '\n')
	    msg.state = H_NL;
	  break;

	case H_NL:
	  if (is_blank (c))
	    msg.state = H_HEADER;
	  else
	    {
	      msg_header_update (&msg);
	      if (c == '\n')
		{
		  msg.header_len = msg.buf_len;
		  msg.state = B_INIT;
		}
	      else
		msg.state = H_INIT;
	    }
	  break;

	case B_INIT:
	  if (c == '\n')
	    msg.state = B_NL;
	  break;

	case B_NL:
	  if (c == '.')
	    msg.state = B_DOT;
	  else
	    msg.state = B_INIT;
	  break;

	case B_DOT:
	  if (c == '\n')
	    {
	      if (dot)
		goto end;
	      msg.state = B_NL;
	    }
	  else
	    {
	      if (c == '.' && dot)
		/* unstuff */
		msg.buf_len--;
	      msg.state = B_INIT;
	    }
	  break;
	}
    }
 end:

  switch (msg.state)
    {
    case H_INIT:
    case H_HEADER:
      terror ("malformed message");
      break;
      
    case B_DOT:
      msg.buf_len -= 2;
      break;

    default:
      if (dot)
	terror ("missing terminating dot");
    }

  if (read_recipients)
    {
      for (hmap = msg.header_head; hmap; hmap = hmap->next)
	{
	  char *p = msg.buf_ptr + hmap->hstart;
	  if ((hmap->nlen == 2 && strncasecmp (p, "to", 2) == 0) ||
	      (hmap->nlen == 2 && strncasecmp (p, "cc", 2) == 0) ||
	      (hmap->nlen == 3 && strncasecmp (p, "bcc", 3) == 0))
	    {
	      char *cp;
	      size_t i, j;
	      size_t len = hmap->end - hmap->vstart;
	      int unwrap = 0;
	      
	      p = msg.buf_ptr + hmap->vstart;
	      
	      /* Allocate temporary value copy */
	      cp = malloc (len + 1);
	      if (!p)
		nomemory ();
	      /* Unwrap the value */
	      for (i = j = 0; i < len; i++)
		{
		  if (p[i] == '\n')
		    unwrap = 1;
		  else if (unwrap)
		    {
		      unwrap = 0;
		      continue;
		    }
		  else
		    cp[j++] = p[i];
		}
	      cp[j] = 0;
	      
	      /* A *very* naive and simplified address parser */
	      for (p = strtok (cp, ","); p; p = strtok (NULL, ","))
		{
		  char *q = strchr (p, '<');
		  if (q)
		    {
		      p = strchr (q, '>');
		      if (p)
			p[1] = 0;
		      msg_add_rcpt (&msg, q);
		    }
		  else
		    msg_add_rcpt (&msg, p);
		}
	      free (cp);
	    }
	}
    }

  if (msg.nrcpt == 0)
    terror ("no recipients");
  
  filename = getenv ("MAIL_DUMP");
  if (!filename)
    filename = "mail.dump";
    
  logfile = fopen (filename, getenv ("MAIL_DUMP_APPEND") ? "a" : "w");
  if (!logfile)
    {
      terror ("can't open dump file %s: %m", filename);
      exit (EX_FAILURE);
    }

  fprintf (logfile, "MSGID: %04d\n", 1);
  if (from_person)
    fprintf (logfile, "SENDER: %s\n", from_person);
  fprintf (logfile, "NRCPT: %d\n", msg.nrcpt);
  for (c = 0; c < msg.nrcpt; c++)
    fprintf (logfile, "RCPT[%d]: %s\n", c, msg.rcpt[c]);
  fprintf (logfile, "LENGTH: %lu\n", (unsigned long)msg.buf_len);
  fwrite (msg.buf_ptr, msg.buf_len, 1, logfile);
  fputc ('\n', logfile);
  fclose (logfile);
  exit (EX_OK);
}
