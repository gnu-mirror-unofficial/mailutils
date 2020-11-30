/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2019-2020 Free Software Foundation, Inc.

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
#include <config.h>
#include <confpaths.h>
#include <fcntl.h>
#include <unistd.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <mailutils/util.h>
#include <mailutils/daemon.h>

int
mu_daemon (void)
{
  int fd;
  
  switch (fork ())
    {
    case 0:
      break;

    case -1:
      return errno;

    default:
      _exit (0);
    }

  if (setsid () == (pid_t) -1)
    return errno;

  signal (SIGHUP, SIG_IGN);
  
  switch (fork ())
    {
    case 0:
      break;

    case -1:
      return errno;

    default:
      _exit (0);
    }

  chdir ("/");
  mu_close_fds (0);

  fd = open (PATH_DEVNULL, O_RDWR);
  if (fd == 0)
    {
      dup2 (fd, 1);
      dup2 (fd, 2);
    }
  else if (fd > 0)
    {
      /* This means that mu_close_fds failed to close stdin.
	 Shouldn't happen, but just in case ... */
      dup2 (fd, 0);      
      dup2 (fd, 1);
      dup2 (fd, 2);
      close (fd);
    }
  
  return 0;
}


  
  
  
    
  
