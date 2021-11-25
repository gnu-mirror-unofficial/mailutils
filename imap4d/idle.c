/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2003-2021 Free Software Foundation, Inc.

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

#include "imap4d.h"

int
imap4d_idle (struct imap4d_session *session,
             struct imap4d_command *command, imap4d_tokbuf_t tok)
{
  struct timeval stop_time, tv, *to;
  char *token_str = NULL;
  size_t token_size = 0, token_len;
  
  if (imap4d_tokbuf_argc (tok) != 2)
    return io_completion_response (command, RESP_BAD, "Invalid arguments");

  if (mu_stream_ioctl (iostream, MU_IOCTL_TIMEOUT, MU_IOCTL_OP_GET, &tv))
    return io_completion_response (command, RESP_NO, "Cannot idle");

  io_sendf ("+ idling\n");
  io_flush ();

  if (idle_timeout)
    {
      gettimeofday (&stop_time, NULL);
      stop_time.tv_sec += idle_timeout;
      to = &tv;
    }
  else
    to = NULL;

  while (1)
    {
      int rc;

      if (to)
	{
	  struct timeval d;

	  gettimeofday (&d, NULL);
	  if (mu_timeval_cmp (&d, &stop_time) >= 0)
	    {
	      imap4d_bye (ERR_TIMEOUT);
	    }
	  *to = mu_timeval_sub (&stop_time, &d);
	}

      rc = mu_stream_timed_getline (iostream, &token_str, &token_size,
				    to, &token_len);
      if (rc == MU_ERR_TIMEOUT)
	{
	  imap4d_bye (ERR_TIMEOUT);
	}
      else if (rc)
	{
	  mu_error (_("read error: %s"), mu_strerror (rc));
	  imap4d_bye (ERR_NO_IFILE);
	}
      else if (token_len == 0)
	{
	  mu_error ("%s", _("eof while idling"));
	  imap4d_bye (ERR_NO_IFILE);
	}
      
      token_len = mu_rtrim_class (token_str, MU_CTYPE_ENDLN);

      if (token_len == 4 && mu_c_strcasecmp (token_str, "done") == 0)
	break;

      imap4d_sync ();
      io_flush ();
    }
  free (token_str);
  return io_completion_response (command, RESP_OK, "terminated");
}

