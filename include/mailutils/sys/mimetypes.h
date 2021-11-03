/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2021 Free Software Foundation, Inc.

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

#ifndef _MAILUTILS_SYS_MIMETYPES_H
#define _MAILUTILS_SYS_MIMETYPES_H

#include <regex.h>
#include <mailutils/locus.h>

#define L_OR  0
#define L_AND 1

enum node_type
  {
    true_node,
    functional_node,
    binary_node,
    negation_node,
    suffix_node
  };

struct mimetypes_string
{
  char *ptr;
  size_t len;
}; 

union argument
{
  struct mimetypes_string *string;
  unsigned number;
  int c;
  regex_t rx;
};

struct input_file
{
  char const *name;
  mu_stream_t stream;
};
 
typedef int (*builtin_t) (union argument *args, struct input_file *input);

struct builtin_tab
{
  char *name;
  char *args;
  builtin_t handler;
};

struct node
{
  enum node_type type;
  struct mu_locus_range loc;
  union
  {
    struct
    {
      struct builtin_tab const *builtin;
      union argument *args;
    } function;
    struct node *arg;
    struct
    {
      int op;
      struct node *arg1;
      struct node *arg2;
    } bin; 
    struct mimetypes_string suffix;
  } v;
};

struct rule_tab
{
  char *type;
  int priority;
  struct mu_locus_range loc;
  struct node *node;
};

struct mu_mimetypes
{
  mu_list_t rule_list;
  mu_opool_t pool;
};

struct builtin_tab const *mu_mimetypes_builtin (char const *ident);

#endif
