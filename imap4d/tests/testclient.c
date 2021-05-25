/*
  NAME
    testclient - test imap client library using GNU imap4d

  SYNOPSIS
    testclient CONFIG_FILE CLIENT_COMMAND

  DESCRIPTION
    Auxiliary tool for testing the mailutils IMAP client library.
    
    Arguments are:
      CONFIG_FILE    - name of the imap4d configuration file
      CLIENT_COMMAND - command to run.

    The tool finds first unused TCP port on localhost and starts
    listening on that port in the background.  When a connection
    arrives, it starts imap4d in inetd mode with the given
    configuratiom file.

    The master process then adds the following two variables to the
    environment:
    
      PORT - port number it is listening on
      URL  - mailutils URL (imap://127.0.0.1:$PORT)

    Finally, it executes /bin/sh -c CLIENT_COMMAND. The client
    command is supposed to connect to the port and issue some IMAP
    commands.

    The program imposes a 60 second timeout on the execution time.
    
  LICENSE
    Copyright (C) 2021 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

char const *progname;

static void
error (int exit_code, int error_code, char const *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "%s: ", progname);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  if (error_code)
    fprintf (stderr, ": %s", strerror (error_code));
  fputc ('\n', stderr);
  if (exit_code)
    exit (exit_code);
}

static int
listener_setup (char *scheme)
{
  struct addrinfo *ap, hints;
  socklen_t len;
  int fd;
  int i;
  char serv[80];
  char urlbuf[80];
  
  /* Find first free port */
  memset (&hints, 0, sizeof (hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_family = AF_INET;
  
  if ((i = getaddrinfo ("127.0.0.1", NULL, &hints, &ap)) != 0)
    error (EX_OSERR, errno, "getaddrinfo: %s", gai_strerror (i));
	
  fd = socket (PF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    error (EX_OSERR, errno, "socket");
  i = 1;
  setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof (i));

  ((struct sockaddr_in*)ap->ai_addr)->sin_port = 0;
  if (bind (fd, ap->ai_addr, ap->ai_addrlen) < 0)
    error (EX_OSERR, errno, "bind");

  if (listen (fd, 8) == -1)
    error (EX_OSERR, errno, "listen");

  len = ap->ai_addrlen;
  if (getsockname (fd, ap->ai_addr, &len))
    error(EX_OSERR, errno, "getsockname");

  /* Prepare environment */
  if ((i = getnameinfo (ap->ai_addr, len, NULL, 0, serv, sizeof serv,
			NI_NUMERICSERV)) != 0)
    error (EX_OSERR, errno, "getnameinfo: %s", gai_strerror (i));
  
  setenv ("PORT", serv, 1);
  snprintf (urlbuf, sizeof urlbuf, "%s://127.0.0.1:%s", scheme, serv);
  setenv ("URL", urlbuf, 1);
  
  return fd;
}

void
usage (void)
{
  printf ("usage: %s CONFIG_FILE COMMAND\n", progname);
}

int
main (int argc, char **argv)
{
  int lfd;
  pid_t pid;

  progname = argv[0];
  
  if (argc != 3)
    {
      usage ();
      exit (EX_USAGE);
    }

  lfd = listener_setup ("imap");
  pid = fork ();
  if (pid == -1)
    error (EX_OSERR, errno, "fork");

  if (pid == 0)
    {
      /* Run server */
      int fd;
      char *sargv[] = {
	"imap4d",
	"--inetd",
	"--preauth",
	"--foreground",
	"--no-config",
	"--config-file",
	argv[1],
	"--set",
	".logging.syslog=off",
	NULL
      };
      struct sockaddr_in sin;
      socklen_t len = sizeof sin;
      
      alarm (60);//FIXME
      fd = accept (lfd, (struct sockaddr *)&sin, &len);

      if (fd == -1)
	error (EX_OSERR, errno, "accept");

      dup2 (fd, 0);
      dup2 (fd, 1);
      if (fd > 1)
	close (fd);
      close (lfd);
      
      execvp (sargv[0], sargv);
      _exit (127);
    }

  execlp ("/bin/sh", "/bin/sh", "-c", argv[2], NULL);
  error (EX_OSERR, errno, "execlp");
}
