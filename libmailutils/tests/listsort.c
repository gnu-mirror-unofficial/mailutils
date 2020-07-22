/*
  NAME
    listsort - test the mu_list_sort function.

  SYNOPSIS
    listsort [-dv] [-n N] [-l FILE] [--dump] [--count N]
             [--load FILE] [--verbose]

  DESCRIPTION
    To test the mu_list_sort function, listsort creates an array of long
    integers from 0 to N, shuffles them in random order, loads the resulting
    array to a mu_list_t object and runs mu_list_sort on that object.  If
    the items in the resulting array are sorted in increased order, the
    program exits with code 0.  Otherwise, it dumps the original and sorted
    lists in format suitable for use with the -l (--load) option and exists
    with code 1.  On error, it displays the error message and exists with
    code 2.

    The -l (--load) option serves to re-run the test as governed by the
    dump output created during a previous listsort invocation.  If both
    -n and -l options are given, listsort will load at most N numbers from
    the dump.

  OPIONS
    -d, --dump
       Produce a session dump on success.
       
    -v, --verbose
       Verbosely list what is being done.
       
    -n, --count N
       Without -l, create initial list of first N non-negative integer
       numbers.  With -l, load at most N numbers from the dump file.
       
    -l, --load FILE
       Load the initial list from the dump file created by a previous
       invocation of listsort.  This option is designed to facilitate
       debugging.

  EXIT CODES
    0  success
    1  failure (resulting list is not properly ordered)
    2  another error occurred
    64 command line usage error

  AUTHOR
     Sergey Poznyakoff <gray@gnu.org>
    
  LICENSE
     This file is part of GNU mailutils.
     Copyright (C) 2020 Free Software Foundation, Inc.

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
#include <limits.h>
#include <mailutils/mailutils.h>

static size_t n_opt;
static size_t n_count;
static size_t n_max;
static long *n_numbers;
static char *input_file;
static int dump_opt;
static int verbose_opt;

static int
num_comp (const void *item_a, const void *item_b)
{
  const long *a = item_a;
  const long *b = item_b;
  return *a - *b;
}

static int
verify_item (void *item, void *data)
{
  const long *a = item;
  long *b = data;
  if (*b != LONG_MIN && *a != *b + 1)
    return MU_ERR_USER0;
  *b = *a;
  return 0;
}
  
static int
print_item (void *item, void *data)
{
  const long *a = item;
  printf ("%ld\n", *a);
  return 0;
}

struct mu_option options[] = {
  { "count", 'n', "N", MU_OPTION_DEFAULT,
    "list size", mu_c_size, &n_count },
  { "load", 'l', "FILE", MU_OPTION_DEFAULT,
    "load input list from FILE", mu_c_string, &input_file },
  { "dump", 'd', NULL, MU_OPTION_DEFAULT,
    "print input and output lists", mu_c_bool, &dump_opt },
  { "verbose", 'v', NULL, MU_OPTION_DEFAULT,
    "verbose mode", mu_c_bool, &verbose_opt },
  MU_OPTION_END
};

enum
  {
    EX_PASS,
    EX_FAIL,
    EX_ERROR
  };

int
main (int argc, char **argv)
{
  mu_list_t lst;
  size_t i;
  int rc;
  int result = EX_PASS;
  long prev;
  
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC, "mu_list_sort test",
		 MU_CLI_OPTION_END);
  
  MU_ASSERT (mu_list_create (&lst));
  mu_list_set_comparator (lst, num_comp);

  if (input_file)
    {
      unsigned long line = 0;
      char buf[80];
      FILE *fp = fopen (input_file, "r");
      if (!fp)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "fopen", input_file, errno);
	  return EX_ERROR;
	}
      if (verbose_opt)
	fprintf (stderr, "Loading initial array from %s\n", input_file);
      while (fgets (buf, sizeof buf, fp))
	{
	  long n;
	  char *p;

	  ++line;
	  mu_rtrim_class (buf, MU_CTYPE_SPACE);
	  if (line == 1 && strcmp (buf, "INPUT") == 0)
	    continue;
	  if (strcmp (buf, "OUTPUT") == 0)
	    break;
	  errno = 0;
	  n = strtol (buf, &p, 0);
	  if (errno || *p)
	    {
	      mu_error ("%s:%lu: invalid input", input_file, line);
	      return EX_ERROR;
	    }

	  if (n_max == n_count)
	    n_numbers = mu_2nrealloc (n_numbers, &n_max, sizeof (n_numbers[0]));
	  n_numbers[n_count++] = n;
	  if (n_opt && n_count == n_opt)
	    break;
	}
      fclose (fp);
    }
  else if (n_count)
    {
      n_numbers = mu_calloc (n_count, sizeof (n_numbers[0]));
      n_max = n_count;
      srandom (time (NULL));
      if (verbose_opt)
	fprintf (stderr, "Generating initial array\n");
      for (i = 0; i < n_count; i++)
	n_numbers[i] = i;
      for (i = 0; i < n_count; i++)
	{
	  size_t j = rand() % n_count;
	  size_t k = rand() % n_count;
	  if (j != k)
	    {
	      long t = n_numbers[j];
	      n_numbers[j] = n_numbers[k];
	      n_numbers[k] = t;
	    }
	}
    }
  else
    {
      mu_error ("either -n or -l is required");
      return EX_ERROR;
    }

  if (verbose_opt)
    fprintf (stderr, "Loading the list\n");
  for (i = 0; i < n_count; i++)
    {
      MU_ASSERT (mu_list_append (lst, &n_numbers[i]));
    }

  if (verbose_opt)
    fprintf (stderr, "Sorting\n");
  mu_list_sort (lst, NULL);

  if (verbose_opt)
    fprintf (stderr, "Verifying\n");
  prev = LONG_MIN;
  rc = mu_list_foreach (lst, verify_item, &prev);
  switch (rc)
    {
    case 0:
      break;

    case MU_ERR_USER0:
      dump_opt = 1;
      result = EX_FAIL;
      break;

    default:
      mu_diag_funcall (MU_DIAG_ERROR, "mu_list_foreach", NULL, rc);
      return EX_ERROR;
    }

  if (dump_opt)
    {
      printf ("INPUT\n");
      for (i = 0; i < n_count; i++)
	{
	  printf ("%ld\n", n_numbers[i]);
	}
      printf ("OUTPUT\n");
      mu_list_foreach (lst, print_item, NULL);
    }
  return result;
}
