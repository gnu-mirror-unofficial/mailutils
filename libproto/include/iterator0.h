/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999, 2000, 2004 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301 USA */

#ifndef _ITERATOR0_H
#define _ITERATOR0_H

#ifdef DMALLOC
#  include <dmalloc.h>
#endif

#include <mailutils/iterator.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _mu_iterator
{
  struct _mu_iterator *next_itr; /* Next iterator in the chain */
  void *owner;                /* Object whose contents is being iterated */
  int is_advanced;            /* Is the iterator already advanced */

  int (*dup) (void **ptr, void *owner);
  int (*destroy) (mu_iterator_t itr, void *owner);
  int (*first) (void *owner);
  int (*next) (void *owner);
  int (*getitem) (void *owner, void **pret);
  int (*curitem_p) (void *owner, void *item);
  int (*finished_p) (void *owner);
};

#ifdef __cplusplus
}
#endif

#endif /* _ITERATOR0_H */