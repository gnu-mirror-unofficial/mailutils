/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2011-2021 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <mailutils/errno.h>
#include <mailutils/sys/list.h>

static void
_list_append_entry (struct _mu_list *list, struct list_data *ent)
{
  ent->prev = list->head.prev;
  ent->next = &list->head;
  list->head.prev->next = ent;
  list->head.prev = ent;
  list->count++;
}

static void
_list_qsort (mu_list_t list, int cmp (const void *, const void *, void *),
	     void *data)
{
  struct list_data *cur, *middle;
  struct _mu_list high_list, low_list;
  size_t n;

  if (list->count < 2)
    return;
  if (list->count == 2)
    {
      if (cmp (list->head.prev->item, list->head.next->item, data) < 0)
	{
	  cur = list->head.prev;
	  list->head.prev = list->head.next;
	  list->head.next = cur;
	  
	  list->head.next->prev = &list->head;
	  list->head.next->next = list->head.prev;
	  
	  list->head.prev->next = &list->head;
	  list->head.prev->prev = list->head.next;
	}
      return;
    }

  middle = list->head.next;
  for (n = list->count / 2; n > 0; n--)
    middle = middle->next;
  
  /* Split into two sublists */
  _mu_list_init (&high_list);
  _mu_list_init (&low_list);

  for (cur = list->head.next; cur != &list->head; )
    {
      struct list_data *next = cur->next;
      cur->next = NULL;

      if (cur != middle)
	{
	  if (cmp (middle->item, cur->item, data) < 0)
	    _list_append_entry (&high_list, cur);
	  else
	    _list_append_entry (&low_list, cur);
	}
      cur = next;
    }

  /* Sort both sublists recursively */
  _list_qsort (&low_list, cmp, data);
  _list_qsort (&high_list, cmp, data);
  
  /* Join low_list + middle + high_list */
  if (low_list.head.prev == _mu_list_null (&low_list))
    cur = &low_list.head;
  else
    cur = low_list.head.prev;

  cur->next = middle;
  middle->prev = cur;
  middle->next = &low_list.head;
  low_list.head.prev = middle;
  cur = middle;
  
  if (high_list.head.next != _mu_list_null (&high_list))
    {
      cur->next = high_list.head.next;
      high_list.head.next->prev = cur;
      
      low_list.head.prev = high_list.head.prev;
      low_list.head.prev->next = &low_list.head;

      low_list.count += high_list.count;
    }
  
  /* Return the resulting list */
  list->head.next = low_list.head.next;
  list->head.next->prev = _mu_list_null (list);
      
  list->head.prev = low_list.head.prev;
  list->head.prev->next = _mu_list_null (list);
}

void
mu_list_sort_r (mu_list_t list,
		int (*comp) (const void *, const void *, void *), void *data)
{
  if (list)
    _list_qsort (list, comp, data);
}

static int
callcomp (const void *a, const void *b, void *data)
{
  mu_list_comparator_t comp = data;
  return comp (a, b);
}

void
mu_list_sort (mu_list_t list, mu_list_comparator_t comp)
{
  if (list)
    _list_qsort (list, callcomp, comp ? comp : list->comp);
}

