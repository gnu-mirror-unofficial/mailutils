/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2005-2021 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mailutils/util.h>
#include <mailutils/errno.h>
#include <mailutils/error.h>
#include <mailutils/stream.h>
#include <mailutils/stdstream.h>
#include <mailutils/cli.h>

char *progname;

int
main (int argc, char **argv)
{
  struct mu_tempfile_hints hints;
  int flags = 0;
  int fd;
  char *filename;
  char *infile = NULL;
  int yes = 1;
  int verify = 0;
  int verbose = 0;
  int dry_run = 0;
  int unlink_opt = 0;
  int mkdir_opt = 0;
  
  struct mu_option options[] = {
    { "tmpdir", 'D', "DIRNAME", MU_OPTION_DEFAULT,
      "set the temporary directory to use", mu_c_string, &hints.tmpdir },
    { "suffix", 's', "STRING", MU_OPTION_DEFAULT,
      "set file name suffix", mu_c_string, &hints.suffix },
    { "dry-run", 'n', NULL, MU_OPTION_DEFAULT,
      "dry run mode", mu_c_incr, &dry_run },
    { "unlink", 'u', NULL, MU_OPTION_DEFAULT,
      "unlink the file", mu_c_incr, &unlink_opt },
    { "infile", 'f', "FILE", MU_OPTION_DEFAULT,
      "copy the content of the FILE to the temporary file",
      mu_c_string, &infile },
    { "verify", 'V', NULL, MU_OPTION_DEFAULT,
      "dump the stream?", mu_c_incr, &verify },
    { "verbose", 'v', NULL, MU_OPTION_DEFAULT,
      "verbose mode", mu_c_incr, &verbose },
    { "dir", 'm', NULL, MU_OPTION_DEFAULT,
      "create temporary directory, instead of file",
      mu_c_incr, &mkdir_opt },
    MU_OPTION_END
  };

  memset (&hints, 0, sizeof (hints));
  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC, "test temporary file creation",
		 MU_CLI_OPTION_END);
  
  if (dry_run && unlink_opt)
    {
      mu_error ("both -unlink and -dry-run given");
      exit (1);
    }

  if (infile)
    {
      if (flags & MU_TEMPFILE_MKDIR)
	{
	  mu_error ("--infile is useless with --mkdir");
	  exit (1);
	}
      else if (dry_run)
	{
	  mu_error ("--infile is useless with --dry-run");
	  exit (1);
	}
    }

  if (verify && dry_run)
    {
      mu_error ("--verify is useless with --dry-run");
      exit (1);
    }

  if (hints.tmpdir)
    flags |= MU_TEMPFILE_TMPDIR;
  if (hints.suffix)
    flags |= MU_TEMPFILE_SUFFIX;
  if (mkdir_opt)
    flags |= MU_TEMPFILE_MKDIR;

  MU_ASSERT (mu_tempfile (flags ? &hints : NULL, flags,
			  dry_run ? NULL : &fd,
			  unlink_opt ? NULL : &filename));

  if (filename)
    mu_printf ("created file name %s\n", filename);
    
  if (dry_run)
    return 0;
  
  if (infile)
    {
      mu_stream_t in, out;
      mu_off_t size;
      
      if (strcmp (infile, "-") == 0)
	MU_ASSERT (mu_stdio_stream_create (&in, MU_STDIN_FD, 0));
      else
	MU_ASSERT (mu_file_stream_create (&in, infile, MU_STREAM_READ));

      MU_ASSERT (mu_fd_stream_create (&out, filename, fd, MU_STREAM_WRITE));
      mu_stream_ioctl (out, MU_IOCTL_FD, MU_IOCTL_FD_SET_BORROW, &yes);
      MU_ASSERT (mu_stream_copy (out, in, 0, &size));
      if (verbose)
	mu_printf ("copied %lu bytes to the temporary\n", (unsigned long) size);
      mu_stream_unref (out);
      mu_stream_unref (in);
    }

  if (verify)
    {
      mu_stream_t in;
      mu_off_t size;

      MU_ASSERT (mu_fd_stream_create (&in, filename, fd,
				      MU_STREAM_READ|MU_STREAM_SEEK));
      mu_stream_ioctl (in, MU_IOCTL_FD, MU_IOCTL_FD_SET_BORROW, &yes);
      MU_ASSERT (mu_stream_copy (mu_strout, in, 0, &size));
      if (verbose)
	mu_printf ("dumped %lu bytes\n", (unsigned long) size);
      mu_stream_unref (in);
    }

  close (fd);
  
  return 0;
}

	  
