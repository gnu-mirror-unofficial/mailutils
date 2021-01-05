/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2011-2021 Free Software Foundation, Inc.

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

#include <config.h>
#include <stdlib.h>
#include <mailutils/mailutils.h>

mu_msgset_format_t format = mu_msgset_fmt_imap;

static void
parse_msgrange (char const *arg, struct mu_msgrange *range)
{
  size_t msgnum;
  char *p;
  
  errno = 0;
  msgnum = strtoul (arg, &p, 10);
  range->msg_beg = msgnum;
  if (*p == ':')
    {
      if (*++p == '*')
	msgnum = 0;
      else
	{
	  msgnum = strtoul (p, &p, 10);
	  if (*p)
	    {
	      mu_error ("error in message range near %s", p);
	      exit (1);
	    }
	}
    }
  else if (*p == '*')
    msgnum = 0;
  else if (*p)
    {
      mu_error ("error in message range near %s", p);
      exit (1);
    }

  range->msg_end = msgnum;
}

mu_msgset_t
parse_msgset (const char *arg)
{
  int rc;
  mu_msgset_t msgset;
  char *end;

  MU_ASSERT (mu_msgset_create (&msgset, NULL, MU_MSGSET_NUM));
  if (arg)
    {
      rc = mu_msgset_parse_imap (msgset, MU_MSGSET_NUM, arg, &end);
      if (rc)
	{
	  mu_error ("mu_msgset_parse_imap: %s near %s",
		    mu_strerror (rc), end);
	  exit (1);
	}
    }
  return msgset;
}

static void
print_all (mu_msgset_t msgset)
{
  MU_ASSERT (mu_stream_msgset_format (mu_strout, format, msgset));
  mu_printf ("\n");
}

static void
print_first (mu_msgset_t msgset)
{
  size_t n;
  MU_ASSERT (mu_msgset_first (msgset, &n));
  printf ("%zu\n", n);
}

static void
print_last (mu_msgset_t msgset)
{
  size_t n;
  MU_ASSERT (mu_msgset_last (msgset, &n));
  printf ("%zu\n", n);
}

static void
cli_msgset (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  mu_msgset_t *msgset = opt->opt_ptr;
  if (*msgset)
    {
      mu_parseopt_error (po, "message set already defined");
      exit (po->po_exit_error);
    }
  *msgset = parse_msgset (arg);
}

static void
cli_mh (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  format = mu_msgset_fmt_mh;
}

static mu_msgset_t
get_msgset (struct mu_option *opt)
{
  mu_msgset_t *msgset = opt->opt_ptr;
  if (!*msgset)
    {
      *msgset = parse_msgset (NULL);
    }
  return *msgset;
}
    
static void
cli_add (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  struct mu_msgrange range;
  parse_msgrange (arg, &range);
  MU_ASSERT (mu_msgset_add_range (get_msgset (opt), range.msg_beg,
				  range.msg_end, MU_MSGSET_NUM));
}

static void
cli_sub (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  struct mu_msgrange range;
  parse_msgrange (arg, &range);
  MU_ASSERT (mu_msgset_sub_range (get_msgset (opt), range.msg_beg,
				  range.msg_end, MU_MSGSET_NUM));
}

static void
cli_addset (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  mu_msgset_t tset = parse_msgset (arg);
  MU_ASSERT (mu_msgset_add (get_msgset (opt), tset));
  mu_msgset_free (tset);
}

static void
cli_subset (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  mu_msgset_t tset = parse_msgset (arg);
  MU_ASSERT (mu_msgset_sub (get_msgset (opt), tset));
  mu_msgset_free (tset);
}

static void
cli_first (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  void (**print) (mu_msgset_t) = opt->opt_ptr;
  *print = print_first;
}

static void
cli_last (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  void (**print) (mu_msgset_t) = opt->opt_ptr;
  *print = print_last;
}

int
main (int argc, char **argv)
{
  mu_msgset_t msgset = NULL;
  void (*print) (mu_msgset_t) = print_all;
  
  struct mu_option options[] = {
    { "mh", 0, NULL, MU_OPTION_DEFAULT,
      "use MH message set format for output", mu_c_incr, NULL, cli_mh },
    { "msgset", 0, "SET", MU_OPTION_DEFAULT,
      "define message set", mu_c_string, &msgset, cli_msgset },
    { "add", 0, "X[:Y]", MU_OPTION_DEFAULT,
      "add range to message set", mu_c_string, &msgset, cli_add },
    { "sub", 0, "X[:Y]", MU_OPTION_DEFAULT,
      "subtract range from message set", mu_c_string, &msgset, cli_sub },
    { "addset", 0, "SET", MU_OPTION_DEFAULT,
      "add message set to message set", mu_c_string, &msgset, cli_addset },
    { "subset", 0, "SET", MU_OPTION_DEFAULT,
      "subtract message set from message set", mu_c_string, &msgset,
      cli_subset },
    { "first", 0, NULL, MU_OPTION_DEFAULT,
      "print only first element from the resulting set",
      mu_c_string, &print, cli_first },
    { "last", 0, NULL, MU_OPTION_DEFAULT,
      "print only last element from the resulting set",
      mu_c_string, &print, cli_last },
    MU_OPTION_END
  };
    
  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_SINGLE_DASH,
		 MU_CLI_OPTION_PROG_DOC, "message set parser test utility",
		 MU_CLI_OPTION_END);

  if (!msgset)
    {
      mu_error ("nothing to do; try %s -help for assistance", mu_program_name);
      exit (1);
    }
  
  print (msgset);
  mu_msgset_free (msgset);
  
  return 0;
}
  
	     

	
