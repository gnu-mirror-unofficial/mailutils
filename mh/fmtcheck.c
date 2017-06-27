/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2003, 2005-2012, 2014-2017 Free Software
   Foundation, Inc.

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

/* fmtcheck */

#include <mh.h>

static char prog_doc[] = N_("Check MH format string");
static char args_doc[] = N_("[FILE]");

static char *format_str;
static mh_format_t format;
static int dump_option;
static int disass_option;
static int debug_option;
static char *input_file;
static size_t width;
static size_t msgno;

static struct mu_option options[] = {
  { "form",    0, N_("FILE"),   MU_OPTION_DEFAULT,
    N_("read format from given file"),
    mu_c_string, &format_str, mh_opt_read_formfile },
  
  { "format",  0, N_("FORMAT"), MU_OPTION_DEFAULT,
    N_("use this format string"),
    mu_c_string, &format_str },
  { "dump",    0, NULL,     MU_OPTION_HIDDEN,
    N_("dump the listing of compiled format code"),
    mu_c_bool,   &dump_option },
  { "disassemble",    0, NULL,     MU_OPTION_HIDDEN,
    N_("dump disassembled format code"),
    mu_c_bool,   &disass_option },
  { "debug",   0, NULL,     MU_OPTION_DEFAULT,
    N_("enable parser debugging output"),
    mu_c_bool,   &debug_option },
  { "width",   0, N_("NUMBER"), MU_OPTION_DEFAULT,
    N_("set output width"),
    mu_c_size, &width },
  { "msgno",   0, N_("NUMBER"), MU_OPTION_DEFAULT,
    N_("set message number"),
    mu_c_size, &msgno },

  MU_OPTION_END
};

static void
run (void)
{
  mu_message_t msg = mh_file_to_message (NULL, input_file);
  mh_format (mu_strout, format, msg, msgno, width, MH_FMT_FORCENL);
}

int
main (int argc, char **argv)
{
  mh_getopt (&argc, &argv, options, 0, args_doc, prog_doc, NULL);
  switch (argc)
    {
    case 0:
      dump_option = 1;
      break;
      
    case 1:
      input_file = argv[0];
      break;

    default:
      mu_error (_("too many arguments"));
      return 1;
    }
  
  mh_format_debug (debug_option);
  if (!format_str)
    {
      mu_error (_("Format string not specified"));
      return 1;
    }
  if (mh_format_parse (&format, format_str, MH_FMT_PARSE_TREE))
    {
      mu_error (_("Bad format string"));
      exit (1);
    }

  if (dump_option)
    mh_format_dump_code (format);
  if (disass_option)
    mh_format_dump_disass (format);

  if (input_file)
    run ();
  
  return 0;
}
