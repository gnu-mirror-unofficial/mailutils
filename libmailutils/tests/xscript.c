/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2005-2020 Free Software Foundation, Inc.

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

/* A simple echo server for testing transcript streams.
   Each input line is prefixed with the input line number and echoed on
   stdout.
   
   Input starting with '\' is a command request.  It must contain a string
   consisting of one or two of the following letters:
   
     n       toggle normal transcript level;
     s       toggle secure transcript level (passwords are not displayed);
     p       toggle payload transcript level;

   If two letters are given, the first one sets mode for the input and
   the second one for the output channel.

   Single request letter may be followed with channel number, optionally
   separated from it by any amount of whitespace. Channel number is 0
   for input (client) and 1 for output (server) channel.

   For the 'p' request, the channel number may be followed by whitespace
   and data length.  In that case, the payload mode will be automatically
   cancelled after receiving that many bytes on the channel. 
     
   The server responds to the command request with a "// " followed by
   the letters describing previous transcript level.
   
   Transcript of the session goes to stderr.
*/

#include <mailutils/mailutils.h>
#include <assert.h>

mu_stream_t
create_transcript (void)
{
  mu_stream_t iostr;
  mu_stream_t xstr;
  mu_stream_t dstr;
  
  mu_stdstream_setup (MU_STDSTREAM_RESET_NONE);
  MU_ASSERT (mu_iostream_create (&iostr, mu_strin, mu_strout));
  mu_stream_set_buffer (iostr, mu_buffer_line, 0);

  MU_ASSERT (mu_dbgstream_create (&dstr, MU_DIAG_DEBUG));
  
  MU_ASSERT (mu_xscript_stream_create (&xstr, iostr, dstr, NULL));
  mu_stream_unref (dstr);
  mu_stream_unref (iostr);
  return xstr;
}

static char level_letter[] = "nsp";

static int
letter_to_level (int l)
{
  char *p = strchr (level_letter, l);
  if (!p)
    return -1;
  return p - level_letter;
}

static int
level_to_letter (int l)
{
  assert (l >= 0 && l < strlen (level_letter));
  return level_letter[l];
}

enum
  {
    getnum_ok,
    getnum_fail,
    getnum_eol
  };

static int
getnum (char **pbuf, unsigned int *pnum, int opt)
{
  unsigned int n;
  char *start, *end;

  start = mu_str_skip_class (*pbuf, MU_CTYPE_SPACE);
  if (*start == 0)
    {
      if (opt)
	return getnum_eol;
      mu_error ("expected number, but found end of line");
      return getnum_fail;
    }
      
  errno = 0;
  n = strtoul (start, &end, 10);
  if (errno || (*end && !mu_isspace (*end)))
    {
      mu_error ("expected number, but found '%s'", start);
      return getnum_fail;
    }
  *pnum = n;
  *pbuf = end;
  return getnum_ok;
}

static void
print_level (mu_stream_t str, int level)
{
  int ilev = MU_XSCRIPT_LEVEL_UNPACK (0, level);
  int olev = MU_XSCRIPT_LEVEL_UNPACK (1, level);
  if (ilev == olev)
    mu_stream_printf (str, "// %c\n", level_to_letter (ilev));
  else
    mu_stream_printf (str, "// %c%c\n",
		      level_to_letter (ilev),
		      level_to_letter (olev));
}

static int
set_channel_level (mu_stream_t str, unsigned int cd, int level, char *buf)
{
  struct mu_xscript_channel chan;
  unsigned int n;
  
  if (cd > 2)
    {
      mu_error ("expected 0 or 1, but found %ul", cd);
      return -1;
    }
  
  chan.cd = cd;
  chan.level = level;
  switch (getnum (&buf, &n, 1))
    {
    case getnum_eol:
      chan.length = 0;
      break;

    case getnum_ok:
      chan.length = n;
      break;

    case getnum_fail:
      return -1;
    }

  if (mu_str_skip_class (buf, MU_CTYPE_SPACE)[0])
    {
      mu_error ("garbage after command");
      return -1;
    }

  MU_ASSERT (mu_stream_ioctl (str, MU_IOCTL_XSCRIPTSTREAM,
			      MU_IOCTL_XSCRIPTSTREAM_CHANNEL, &chan));
  print_level (str, chan.level);
  
  return 0;
}

static int
set_level (mu_stream_t str, int level, char *buf)
{
  unsigned int n;
  int olev;

  olev = letter_to_level (*buf);
  if (olev != -1)
    {
      level = MU_XSCRIPT_LEVEL_PACK (level, olev);
      MU_ASSERT (mu_stream_ioctl (str, MU_IOCTL_XSCRIPTSTREAM,
				  MU_IOCTL_XSCRIPTSTREAM_LEVEL, &level));
      print_level (str, level);
      return 0;
    }

  switch (getnum (&buf, &n, 1))
    {
    case getnum_eol:
      MU_ASSERT (mu_stream_ioctl (str, MU_IOCTL_XSCRIPTSTREAM,
				  MU_IOCTL_XSCRIPTSTREAM_LEVEL, &level));
      print_level (str, level);
      return 0;

    case getnum_ok:
      return set_channel_level (str, n, level, buf);

    case getnum_fail:
      break;
    }

  return -1;
}

static int
xstream_setopt (mu_stream_t str, char *buf)
{
  int lev = letter_to_level (*buf);

  if (lev == -1)
    {
      mu_error ("unrecognized level: %c", *buf);
      return -1;
    }
  return set_level (str, lev, buf + 1);
}      

int
main (int argc, char **argv)
{
  int rc;
  char *buf = NULL;
  size_t size = 0, n;
  unsigned line = 0;
  mu_stream_t str = create_transcript ();

  while ((rc = mu_stream_getline (str, &buf, &size, &n)) == 0 && n > 0)
    {
      line++;
      if (buf[0] == '\\')
	{
	  xstream_setopt (str, buf + 1);
	  continue;
	}
      mu_stream_printf (str, "%04u: %s", line, buf);
    }
  mu_stream_destroy (&str);
  exit (0);
}

  
