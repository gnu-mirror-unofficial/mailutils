/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2021 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library.  If not, see
   <http://www.gnu.org/licenses/>. */

#ifndef _MAILUTILS_SYS_MAILCAP_H
# define _MAILUTILS_SYS_MAILCAP_H
# include <mailutils/mailcap.h>
# include <mailutils/locus.h>

struct _mu_mailcap_entry
{
  char *type;
  char *command;
  mu_assoc_t fields;
  struct mu_locus_range *lrp;
};

enum fld_type
  {
    fld_bool,
    fld_string
  };

struct mailcap_field
{
  enum fld_type type;
  char *strval;
};

struct _mu_mailcap
{
  int flags;
  mu_list_t elist;
  struct mu_mailcap_selector_closure selector;
  struct mu_mailcap_error_closure error;
  struct mu_locus_range locus;
};

struct _mu_mailcap_finder
{
  struct _mu_mailcap *mcp;
  mu_iterator_t itr;
};

#endif
