/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2021 Free Software Foundation, Inc.

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
#include <stdlib.h>
#include <stdio.h>
#include <mailutils/mailutils.h>

struct list_closure
{
  unsigned long n;
};

static void list_all (mu_mailcap_t mailcap);
static void list_single_entry (mu_mailcap_entry_t ent);
static int list_field (char const *name, char const *value, void *data);
static int list_entry (mu_mailcap_entry_t ent, void *closure);

static void
cli_locus (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  int *pflag = opt->opt_ptr;
  *pflag |= MU_MAILCAP_FLAG_LOCUS;
}

/* usage:
     mailcap [-f FILE]
       List entries
     mailcap [-f FILE] TYPE
       List first entry matching TYPE
     mailcap [-f FILE] TYPE FIELD
       List FIELD from the first entry matching TYPE
 */
int
main (int argc, char **argv)
{
  int status = 0;
  int flags = MU_MAILCAP_FLAG_DEFAULT;
  mu_mailcap_t mailcap;
  char *file = NULL;

  struct mu_option options[] = {
    { "file", 'f', "NAME", MU_OPTION_DEFAULT,
      "mailcap file name", mu_c_string, &file },
    { "locus", 'l', NULL, MU_OPTION_DEFAULT,
      "record location of each entry in the source file",
      mu_c_incr, &flags, cli_locus },
    MU_OPTION_END
  };

  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC,
		 "mailcap test: without arguments lists all mailcap entries, "
		 "with single argument, prints the first entry matching TYPE, "
		 "with two arguments prints FIELD from the first entry "
		 "matching TYPE.",
		 MU_CLI_OPTION_PROG_ARGS, "[TYPE [FIELD]]",
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
		 MU_CLI_OPTION_RETURN_ARGV, &argv,
		 MU_CLI_OPTION_END);
  
  MU_ASSERT (mu_mailcap_create (&mailcap));
  mu_mailcap_set_error (mailcap, &mu_mailcap_default_error_closure);
  if (flags != MU_MAILCAP_FLAG_DEFAULT)
    MU_ASSERT (mu_mailcap_set_flags (mailcap, flags));

  if (file)
    status = mu_mailcap_parse_file (mailcap, file);
  else
    {
      struct mu_locus_point point = MU_LOCUS_POINT_INITIALIZER;

      mu_locus_point_set_file (&point, "<stdin>");
      point.mu_line = 1;
      status = mu_mailcap_parse (mailcap, mu_strin, &point);
      mu_locus_point_deinit (&point);
    }

  if (status && status != MU_ERR_PARSE)
    {
      mu_error ("%s", mu_strerror (status));
      return 1;
    }

  switch (argc)
    {
    case 0:
      list_all (mailcap);
      break;

    case 1:
      {
	mu_mailcap_entry_t entry;

	MU_ASSERT (mu_mailcap_find_entry (mailcap, argv[0], &entry));
	list_single_entry (entry);
      }
      break;

    case 2:
      {
	mu_mailcap_entry_t entry;
	char const *value;

	MU_ASSERT (mu_mailcap_find_entry (mailcap, argv[0], &entry));
	status = mu_mailcap_entry_sget_field (entry, argv[1], &value);
	if (status == 0)
	  {
	    if (value)
	      mu_printf ("%s=%s\n", argv[1], value);
	    else
	      mu_printf ("%s is set\n", argv[1]);
	  }
	else if (status == MU_ERR_NOENT)
	  mu_printf ("%s is not set\n", argv[1]);
	else
	  mu_error ("%s", mu_strerror (status));
      }
      break;

    default:
      mu_error ("too many arguments");
      return 1;
    }

  mu_mailcap_destroy (&mailcap);
  return 0;
}

static void
list_all (mu_mailcap_t mailcap)
{
  struct list_closure lc;
  lc.n = 1;
  mu_mailcap_foreach (mailcap, list_entry, &lc);
}

int
list_entry (mu_mailcap_entry_t ent, void *closure)
{
  struct list_closure *lc = closure;
  struct mu_locus_range lr = MU_LOCUS_RANGE_INITIALIZER;

  if (mu_mailcap_entry_get_locus (ent, &lr) == 0)
    {
      mu_stream_lprintf (mu_strout, &lr, "entry[%lu]\n", lc->n);
      mu_locus_range_deinit (&lr);
    }
  else
    mu_printf ("entry[%lu]\n", lc->n);
  list_single_entry (ent);
  lc->n++;
  return 0;
}

static void
list_single_entry (mu_mailcap_entry_t ent)
{
  struct list_closure fc;
  char const *val;

  MU_ASSERT (mu_mailcap_entry_sget_type (ent, &val));
  mu_printf ("\ttypefield: %s\n", val);
  MU_ASSERT (mu_mailcap_entry_sget_command (ent, &val));
  mu_printf ("\tview-command: %s\n", val);
  fc.n = 1;
  mu_mailcap_entry_fields_foreach (ent, list_field, &fc);
  mu_printf ("\n");
}

static int
list_field (char const *name, char const *value, void *data)
{
  struct list_closure *fc = data;
  mu_printf ("\tfields[%lu]: ", fc->n++);
  if (value)
    mu_printf ("%s=%s", name, value);
  else
    mu_printf ("%s", name);
  mu_printf ("\n");
  return 0;
}
