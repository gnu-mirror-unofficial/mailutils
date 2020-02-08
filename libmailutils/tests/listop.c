/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2003-2020 Free Software Foundation, Inc.

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
#include <stdio.h>
#include <stdlib.h>
#include <mailutils/mailutils.h>
#include "tesh.h"

static int interactive;

void
lperror (char *text, int rc)
{
  mu_error ("%s: %s", text, mu_strerror (rc));
  exit (1);
}

#define NITR 4

struct listop_closure
{
  mu_list_t lst;
  mu_iterator_t itr[NITR];
  int num;
};

static void
listop_invalidate_iterators (struct listop_closure *cls)
{
  int i;

  for (i = 0; i < NITR; i++)
    mu_iterator_destroy (&cls->itr[i]);
}

void
print_list (mu_list_t list)
{
  mu_iterator_t itr;
  size_t count;
  int rc;
  
  rc = mu_list_get_iterator (list, &itr);
  if (rc)
    lperror ("mu_list_get_iterator", rc);

  rc = mu_list_count (list, &count);
  if (rc)
    lperror ("mu_iterator_current", rc);

  mu_printf ("# items: %lu\n", (unsigned long) count);
  for (mu_iterator_first (itr); !mu_iterator_is_done (itr);
       mu_iterator_next (itr))
    {
      char *text;

      rc = mu_iterator_current (itr, (void**) &text);
      if (rc)
	lperror ("mu_iterator_current", rc);
      mu_printf ("%s\n", text);
    }
  mu_iterator_destroy (&itr);
}

int
com_print (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  print_list (cls->lst);
  return 0;
}

int
com_count (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  size_t n;
  int rc;

  rc = mu_list_count (cls->lst, &n);
  if (rc)
    lperror ("mu_iterator_current", rc);
  else
    mu_printf ("%lu\n", (unsigned long) n);
  return 0;
}

int
com_next (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  int skip = argc == 2 ? strtoul (argv[1], NULL, 0) :  1;

  if (skip == 0)
    {
      mu_error ("next arg?");
    }
  else
    {
      while (skip--)
	mu_iterator_next (cls->itr[cls->num]);
    }
  return 0;
}

int
com_delete (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_list_t list = cls->lst;
  int rc;

  while (--argc)
    {
      rc = mu_list_remove (list, *++argv);
      if (rc)
	mu_diag_funcall (MU_DIAG_ERROR, "mu_list_remove", *argv, rc);
    }
  return 0;
}

int
com_add (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_list_t list = cls->lst;
  int rc;
  
  while (--argc)
    {
      rc = mu_list_append (list, strdup (*++argv));
      if (rc)
	mu_diag_funcall (MU_DIAG_ERROR, "mu_list_append", *argv, rc);
    }
  return 0;
}

int
com_prep (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_list_t list = cls->lst;
  int rc;
  
  while (--argc)
    {
      rc = mu_list_prepend (list, strdup (*++argv));
      if (rc)
	mu_diag_funcall (MU_DIAG_ERROR, "mu_list_prepend", *argv, rc);
    }
  return 0;
}

static mu_list_t
read_list (int argc, char **argv)
{
  int rc;
  mu_list_t list;
  
  rc = mu_list_create (&list);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_list_create", NULL, rc);
      return NULL;
    }
  mu_list_set_destroy_item (list, mu_list_free_item);
  for (; argc; argc--, argv++)
    {
      rc = mu_list_append (list, strdup (*argv));
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_list_append", *argv, rc);
	  mu_list_destroy (&list);
	  break;
	}
    }
  return list;
}

int
com_ins (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_list_t list = cls->lst;
  int rc;
  char *item;
  int insert_before = 0;

  if (mu_assoc_lookup (options, "before", NULL) == 0)
    insert_before = 1;

  item = argv[1];
  
  if (3 == argc)
    rc = mu_list_insert (list, item, strdup (argv[2]), insert_before);
  else
    {
      mu_list_t tmp = read_list (argc - 2, argv + 2);
      if (!tmp)
	return 0;
      rc = mu_list_insert_list (list, item, tmp, insert_before);
      mu_list_destroy (&tmp);
    }

  if (rc)
    lperror ("mu_list_insert", rc);
  return 0;
}
  
int
com_repl (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_list_t list = cls->lst;
  int rc;
  
  rc = mu_list_replace (list, argv[1], strdup (argv[2]));
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_list_replace", NULL, rc);
  return 0;
}

void
ictl_tell (mu_iterator_t itr, int argc)
{
  size_t pos;
  int rc;

  if (argc)
    {
      mu_error ("ictl tell?");
      return;
    }
  
  rc = mu_iterator_ctl (itr, mu_itrctl_tell, &pos);
  if (rc)
    lperror ("mu_iterator_ctl", rc);
  mu_printf ("%lu\n", (unsigned long) pos);
}

void
ictl_del (mu_iterator_t itr, int argc)
{
  int rc;

  if (argc)
    {
      mu_error ("ictl del?");
      return;
    }
  rc = mu_iterator_ctl (itr, mu_itrctl_delete, NULL);
  if (rc)
    lperror ("mu_iterator_ctl", rc);
}

int
ictl_repl (mu_iterator_t itr, int argc, char **argv)
{
  int rc;
  
  if (argc != 1)
    {
      mu_error ("ictl repl item?");
      return 0;
    }

  rc = mu_iterator_ctl (itr, mu_itrctl_replace, strdup (argv[0]));
  if (rc)
    lperror ("mu_iterator_ctl", rc);
  return 0;
}

void
ictl_dir (mu_iterator_t itr, int argc, char **argv)
{
  int rc;
  int dir;
  
  if (argc > 1)
    {
      mu_error ("ictl dir [backwards|forwards]?");
      return;
    }
  if (argc == 1)
    {
      if (strcmp (argv[0], "backwards") == 0)
	dir = 1;
      else if (strcmp (argv[0], "forwards") == 0)
	dir = 0;
      else
	{
	  mu_error ("ictl dir [backwards|forwards]?");
	  return;
	}
      rc = mu_iterator_ctl (itr, mu_itrctl_set_direction, &dir);
      if (rc)
	lperror ("mu_iterator_ctl", rc);
    }
  else
    {
      rc = mu_iterator_ctl (itr, mu_itrctl_qry_direction, &dir);
      if (rc)
	lperror ("mu_iterator_ctl", rc);
      mu_printf ("%s\n", dir ? "backwards" : "forwards");
    }
  return;
}
  
void
ictl_ins (mu_iterator_t itr, int argc, char **argv)
{
  int rc;
  
  if (argc < 1)
    {
      mu_error ("ictl ins item [item*]?");
      return;
    }

  if (argc == 1)
    rc = mu_iterator_ctl (itr, mu_itrctl_insert, strdup (argv[0]));
  else
    {
      mu_list_t tmp = read_list (argc, argv);
      if (!tmp)
	return;
      rc = mu_iterator_ctl (itr, mu_itrctl_insert_list, tmp);
      mu_list_destroy (&tmp);
    }
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_iterator_ctl", NULL, rc);
  return;
}

int
com_ictl (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_iterator_t itr = cls->itr[cls->num];
  
  if (strcmp (argv[1], "tell") == 0)
    ictl_tell (itr, argc - 2);
  else if (strcmp (argv[1], "del") == 0)
    ictl_del (itr, argc - 2);
  else if (strcmp (argv[1], "repl") == 0)
    ictl_repl (itr, argc - 2, argv + 2);
  else if (strcmp (argv[1], "ins") == 0)
    ictl_ins (itr, argc - 2, argv + 2);
  else if (strcmp (argv[1], "dir") == 0)
    ictl_dir (itr, argc - 2, argv + 2);
  else
    mu_error ("unknown subcommand");
  return 0;
}
    
int
com_iter (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  int n;

  n = strtoul (argv[1], NULL, 0);
  if (n < 0 || n >= NITR)
    {
      mu_error ("iter [0-3]?");
      return 1;
    }

  if (!cls->itr[n])
    {
      int rc = mu_list_get_iterator (cls->lst, &cls->itr[n]);
      if (rc)
	lperror ("mu_list_get_iterator", rc);
      mu_iterator_first (cls->itr[n]);
    }
  cls->num = n;
  return 0;
}

int
com_find (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_iterator_t itr = cls->itr[cls->num];
  char *text;
  
  mu_iterator_current (itr, (void**)&text);
  for (mu_iterator_first (itr); !mu_iterator_is_done (itr); mu_iterator_next (itr))
    {
      char *item;

      mu_iterator_current (itr, (void**)&item);
      if (strcmp (argv[1], item) == 0)
	return 0;
    }

  mu_error ("%s not in list", argv[1]);
  return 0;
}

int
com_cur (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_iterator_t itr = cls->itr[cls->num];
  char *text;
  size_t pos;
  int rc;

  mu_printf ("%lu:", (unsigned long) cls->num);
  rc = mu_iterator_ctl (itr, mu_itrctl_tell, &pos);
  if (rc == MU_ERR_NOENT)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_iterator_ctl", NULL, rc);
      return 0;
    }
  if (rc)
    lperror ("mu_iterator_ctl", rc);
  mu_printf ("%lu:", (unsigned long) pos);

  rc = mu_iterator_current (itr, (void**) &text);
  if (rc)
    lperror ("mu_iterator_current", rc);
  mu_printf ("%s\n", text);
  return 0;
}

int
com_first (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_iterator_first (cls->itr[cls->num]);
  return 0;
}

static int
map_even (void **itmv, size_t itmc, void *call_data)
{
  int *num = call_data, n = *num;
  *num = !*num;
  if ((n % 2) == 0)
    {
      itmv[0] = strdup (itmv[0]);
      return MU_LIST_MAP_OK;
    }
  return MU_LIST_MAP_SKIP;
}

static int
map_odd (void **itmv, size_t itmc, void *call_data)
{
  int *num = call_data, n = *num;
  *num = !*num;
  if (n % 2)
    {
      itmv[0] = strdup (itmv[0]);
      return MU_LIST_MAP_OK;
    }
  return MU_LIST_MAP_SKIP;
}

static int
map_concat (void **itmv, size_t itmc, void *call_data)
{
  char *delim = call_data;
  size_t dlen = strlen (delim);
  size_t i;
  size_t len = 0;
  char *res, *p;
  
  for (i = 0; i < itmc; i++)
    len += strlen (itmv[i]);
  len += (itmc - 1) * dlen + 1;

  res = malloc (len);
  if (!res)
    abort ();
  p = res;
  for (i = 0; ; )
    {
      p = mu_stpcpy (p, itmv[i++]);
      if (i == itmc)
	break;
      p = mu_stpcpy (p, delim);
    }
  itmv[0] = res;
  return MU_LIST_MAP_OK;
}

struct trim_data
{
  size_t n;
  size_t lim;
};

static int
map_skip (void **itmv, size_t itmc, void *call_data)
{
  struct trim_data *td = call_data;

  if (td->n++ < td->lim)
    return MU_LIST_MAP_SKIP;
  itmv[0] = strdup (itmv[0]);
  return MU_LIST_MAP_OK;
}

static int
map_trim (void **itmv, size_t itmc, void *call_data)
{
  struct trim_data *td = call_data;

  if (td->n++ < td->lim)
    {
      itmv[0] = strdup (itmv[0]);
      return MU_LIST_MAP_OK;
    }
  return MU_LIST_MAP_STOP|MU_LIST_MAP_SKIP;
}

int
com_map (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_list_t list = cls->lst;
  mu_list_t result;
  int rc;
  int replace = 0;

  if (mu_assoc_lookup (options, "replace", NULL) == 0)
    replace = 1;
  
  if (strcmp (argv[1], "even") == 0)
    {
      int n = 0;
      rc = mu_list_map (list, map_even, &n, 1, &result);
    }
  else if (strcmp (argv[1], "odd") == 0)
    {
      int n = 0;
      rc = mu_list_map (list, map_odd, &n, 1, &result);
    }
  else if (strcmp (argv[1], "concat") == 0)
    {
      size_t num;
      char *delim = "";
      
      if (argc < 3 || argc > 4)
	{
	  mu_error ("map concat NUM [DELIM]");
	  return 0;
	}
      num = atoi (argv[2]);
      if (argc == 4)
	delim = argv[3];
      
      rc = mu_list_map (list, map_concat, delim, num, &result);
    }
  else if (strcmp (argv[1], "skip") == 0)
    {
      struct trim_data td;

      if (argc < 3 || argc > 4)
	{
	  mu_error ("map skip NUM");
	  return 0;
	}
      td.n = 0;
      td.lim = atoi (argv[2]);
      rc = mu_list_map (list, map_skip, &td, 1, &result);
    }
  else if (strcmp (argv[1], "trim") == 0)
    {
      struct trim_data td;

      if (argc < 3 || argc > 4)
	{
	  mu_error ("map trim NUM");
	  return 0;
	}
      td.n = 0;
      td.lim = atoi (argv[2]);
      rc = mu_list_map (list, map_trim, &td, 1, &result);
    }
  else
    {
      mu_error ("unknown map name");
      return 0;
    }
  
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_list_map", NULL, rc);
      return 0;
    }

  mu_list_set_destroy_item (result, mu_list_free_item);

  if (replace)
    {
      size_t count[2];
      mu_list_count (list, &count[0]);
      mu_list_count (result, &count[1]);
      
      mu_printf ("%lu in, %lu out\n", (unsigned long) count[0],
		 (unsigned long) count[1]);
      mu_list_destroy (&list);
      cls->lst = result;
      listop_invalidate_iterators (cls);
    }
  else
    {
      print_list (result);
      mu_list_destroy (&result);
    }
  return 0;
}

static int
dup_string (void **res, void *itm, void *closure)
{
  *res = strdup (itm);
  return *res ? 0 : ENOMEM;
}

int
com_slice (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_list_t list = cls->lst;
  mu_list_t result;
  int rc, i;
  int replace = 0;
  size_t *buf;
  
  if (mu_assoc_lookup (options, "replace", NULL) == 0)
    replace = 1;

  argc--;
  argv++;

  buf = calloc (argc, sizeof (buf[0]));
  if (!buf)
    abort ();
  for (i = 0; i < argc; i++)
    buf[i] = atoi (argv[i]);

  rc = mu_list_slice_dup (&result, list, buf, argc, dup_string, NULL);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_list_slice_dup", NULL, rc);
      return 0;
    }
  if (replace)
    {
      size_t count[2];
      mu_list_count (list, &count[0]);
      mu_list_count (result, &count[1]);
      
      mu_printf ("%lu in, %lu out\n", (unsigned long) count[0],
		 (unsigned long) count[1]);
      mu_list_destroy (&list);
      cls->lst = result;
      listop_invalidate_iterators (cls);
    }
  else
    {
      print_list (result);
      mu_list_destroy (&result);
    }
  return 0;  
}

int
com_head (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  int rc;
  char *text;
  
  rc = mu_list_head (cls->lst, (void**) &text);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_list_head", NULL, rc);
  else
    mu_printf ("%s\n", text);
  return 0;
}

int
com_tail (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  int rc;
  const char *text;
    
  rc = mu_list_tail (cls->lst, (void**) &text);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_list_tail", NULL, rc);
  else
    mu_printf ("%s\n", text);
  return 0;
}

static int
fold_concat (void *item, void *data, void *prev, void **ret)
{
  char *s;
  size_t len = strlen (item);
  size_t prevlen = 0;
  
  if (prev)
    prevlen = strlen (prev);

  s = realloc (prev, len + prevlen + 1);
  if (!s)
    abort ();
  strcpy (s + prevlen, item);
  *ret = s;
  return 0;
}

int
com_fold (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  char *text = NULL;
  int rc;

  rc = mu_list_fold (cls->lst, fold_concat, NULL, NULL, &text);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_list_fold", NULL, rc);
  else if (text)
    {
      mu_printf ("%s\n", text);
      free (text);
    }
  else
    mu_printf ("NULL\n");
  return 0;
}

int
com_rfold (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  char *text = NULL;
  int rc;

  rc = mu_list_rfold (cls->lst, fold_concat, NULL, NULL, &text);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_list_fold", NULL, rc);
  else if (text)
    {
      mu_printf ("%s\n", text);
      free (text);
    }
  else
    mu_printf ("NULL\n");
  return 0;
}

int
com_sort (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  mu_list_sort (cls->lst, NULL);
  listop_invalidate_iterators (cls);
  return 0;
}

int
com_push (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  while (--argc)
    {
      int rc = mu_list_push (cls->lst, strdup (*++argv));
      if (rc)
	mu_diag_funcall (MU_DIAG_ERROR, "mu_list_push", *argv, rc);
    }
  return 0;
}

int
com_pop (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  char *text;
  int rc;
  
  rc = mu_list_pop (cls->lst, (void**) &text);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_list_pop", NULL, rc);
  else
    mu_printf ("%s\n", text);
  return 0;
}

int
envinit (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;

  if (!cls->itr[cls->num])
    {
      int rc = mu_list_get_iterator (cls->lst, &cls->itr[cls->num]);
      if (rc)
	lperror ("mu_list_get_iterator", rc);
      mu_iterator_first (cls->itr[cls->num]);
    }
  return 0;
}

int
get (int argc, char **argv, mu_assoc_t options, void *env)
{
  struct listop_closure *cls = env;
  char *p;
  size_t n;

  errno = 0;
  n = strtoul (argv[0], &p, 0);
  if (errno || *p != 0)
    return MU_ERR_PARSE;
  else
    {
      char *text;
      int rc = mu_list_get (cls->lst, n, (void**) &text);
      if (rc)
	mu_diag_funcall (MU_DIAG_ERROR, "mu_list_get", argv[0], rc);
      else
	mu_printf ("%s\n", text);
      return 0;
    }
}

static struct mu_tesh_command comtab[] = {
  { "__ENVINIT__", "", envinit },
  { "__NOCMD__", "", get },
  { "print", "", com_print },
  { "count", "", com_count },
  { "next", "[COUNT]", com_next },
  { "del", "ITEM ...", com_delete },
  { "add", "ITEM ...", com_add },
  { "prep", "ITEM ...", com_prep },
  { "ins", "[-before] [-after] ITEM NEW_ITEM ...", com_ins },
  { "repl", "OLD_ITEM NEW_ITEM", com_repl },
  { "ictl", "tell|del|repl|ins|dir [ARG...]", com_ictl },
  { "iter", "NUM", com_iter },
  { "find", "ITEM", com_find },
  { "cur", "", com_cur },
  { "map", "[-replace] NAME [ARG...]", com_map },
  { "slice", "[-replace] NUM ...", com_slice },
  { "first", "", com_first },
  { "head", "", com_head },
  { "tail", "", com_tail },
  { "fold", "", com_fold },
  { "rfold", "", com_rfold },
  { "sort", "", com_sort },
  { "push", "ITEM ...", com_push },
  { "pop", "", com_pop },
  { NULL }
};

static int
string_comp (const void *item, const void *value)
{
  return strcmp (item, value);
}

int
main (int argc, char **argv)
{
  struct listop_closure cls;
  int rc;

  mu_tesh_init (argv[0]);

  interactive = isatty (0);

  memset (&cls, 0, sizeof cls);
  
  rc = mu_list_create (&cls.lst);
  if (rc)
    lperror ("mu_list_create", rc);
  mu_list_set_comparator (cls.lst, string_comp);
  mu_list_set_destroy_item (cls.lst, mu_list_free_item);

  while (--argc)
    {
      rc = mu_list_append (cls.lst, strdup (*++argv));
      if (rc)
	lperror ("mu_list_append", rc);
    }

  mu_tesh_read_and_eval (argc, argv, comtab, &cls);
  
  return 0;
}
