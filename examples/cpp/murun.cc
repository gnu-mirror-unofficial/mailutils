/*
   GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2004 Free Software Foundation, Inc.

   GNU Mailutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU Mailutils is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Mailutils; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <iostream>
#include <mailutils/cpp/mailutils.h>
#include <mailutils/argcv.h>

using namespace std;
using namespace mailutils;

static char *progname;

static void
read_and_print (Stream *in, Stream& out)
{
  char buffer[128];
  
  in->SequentialReadLine (buffer, sizeof (buffer));
  while (in->GetReadn ())
    {
      out.SequentialWrite (buffer, in->GetReadn ());
      in->SequentialReadLine (buffer, sizeof (buffer));
    }
}

Stream *
createFilter (bool read_stdin, char *cmdline, int flags)
{
  try {
    if (read_stdin)
      {
	StdioStream *in = new StdioStream (stdin, 0);
	in->Open ();
	FilterProgStream *stream = new FilterProgStream (cmdline, in);
	stream->Open ();
	return stream;
      }
    else
      {
	ProgStream *stream = new ProgStream (cmdline, flags);
	stream->Open ();
	return stream;
      }
  }
  catch (Exception& e) {
    cerr << progname << ": cannot create program filter stream: "
	 << e.Method () << ": " << e.MsgError () << endl;
    exit (1);
  }
}

int
main (int argc, char *argv[])
{
  int i = 1;
  int read_stdin = 0;
  int flags = MU_STREAM_READ;
  char *cmdline;
  Stream *stream, out;

  progname = argv[0];
  
  if (argc > 1 && strcmp (argv[i], "--stdin") == 0)
    {
      read_stdin = 1;
      flags |= MU_STREAM_WRITE;
      i++;
    }

  if (i == argc)
    {
      cerr << "Usage: " << argv[0] << " [--stdin] progname [args]" << endl;
      exit (1);
    }

  argcv_string (argc - i, &argv[i], &cmdline);

  stream = createFilter (read_stdin, cmdline, flags);

  try {
    StdioStream out (stdout, 0);
    out.Open ();

    read_and_print (stream, out);

    delete stream;
  }
  catch (Exception& e) {
    cerr << e.Method () << ": " << e.MsgError () << endl;
    exit (1);
  }

  return 0;
}
