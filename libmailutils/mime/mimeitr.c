/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2020 Free Software Foundation, Inc.

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

#include <config.h>
#include <stdlib.h>
#include <mailutils/types.h>
#include <mailutils/errno.h>
#include <mailutils/iterator.h>
#include <mailutils/message.h>
#include <mailutils/sys/iterator.h>

struct mime_part
{
  struct mime_part *up; /* Upper-level part */
  size_t nparts;  /* Number of MIME parts in this part */
  size_t index;   /* Current position in this part */
  mu_message_t mesg;
};

struct mimeitr
{
  int stop;
  struct mime_part *parent;
  struct mime_part *current;
};

static void
mime_part_list_unwind (struct mime_part **pp, struct mime_part *point)
{
  struct mime_part *p = *pp;
  while (p != point)
    {
      struct mime_part *up = p->up;
      free (p);
      p = up;
    }
  *pp = p;
}

static int
mime_part_list_alloc (struct mime_part **pp, mu_message_t msg)
{
  struct mime_part *np = malloc (sizeof (*np));
  if (!np)
    return -1;
  np->up = *pp;
  np->nparts = np->index = 0;
  np->mesg = msg;
  *pp = np;
  return 0;
}

static inline size_t
mime_part_list_len (struct mime_part *p)
{
  size_t n = 1;
  while ((p = p->up))
    n++;
  return n;
}

static int
first_plain (void *owner)
{
  struct mimeitr *itr = owner;
  itr->current->index = 0;
  itr->stop = 0;
  return mime_part_list_alloc (&itr->current, itr->current->mesg);
}

static int
next_plain (void *owner)
{
  struct mimeitr *itr = owner;
  mime_part_list_unwind (&itr->current, itr->parent);
  itr->stop = 1;
  return 0;
}

static int next_mime (void *owner);

static int
first_mime (void *owner)
{
  struct mimeitr *itr = owner;
  mime_part_list_unwind (&itr->current, itr->parent);
  itr->current->index = 0;
  itr->stop = 0;
  return next_mime (owner);
}

static int
next_mime (void *owner)
{
  struct mimeitr *itr = owner;
  
  while (itr->current->index == itr->current->nparts)
    {
      if (!itr->current->up)
	{
	  itr->stop = 1;
	  return 0;
	}
      mime_part_list_unwind (&itr->current, itr->current->up);
    }

  do
    {
      mu_message_t msg;
      int rc, ismime;
      
      itr->current->index++;

      rc = mu_message_get_part (itr->current->mesg, itr->current->index, &msg);
      if (rc)
	return -1;

      rc = mime_part_list_alloc (&itr->current, msg);
      if (rc)
	return -1;

      rc = mu_message_is_multipart (msg, &ismime);
      if (rc == 0)
	{
	  if (ismime)
	    rc = mu_message_get_num_parts (msg, &itr->current->nparts);
	}
      if (rc)
	{
	  mime_part_list_unwind (&itr->current, itr->current->up);
	  return rc;
	}
    }
  while (itr->current->nparts);
  
  return 0;
}

static int
getitem (void *owner, void **pret, const void **pkey)
{
  struct mimeitr *itr = owner;

  if (pkey)
    {
      struct mime_part *p;
      size_t n = mime_part_list_len (itr->current);
      size_t *path = calloc (n, sizeof (*path));

      if (!path)
	return -1;
      path[0] = --n;
      for (p = itr->current->up; p; p = p->up, n--)
	path[n] = p->index;
      *pkey = path;
    }
  *pret = itr->current->mesg;
  return 0;
}

static int
finished_p (void *owner)
{
  struct mimeitr *itr = owner;
  return itr->stop;
}

static int
destroy (mu_iterator_t iterator, void *data)
{
  struct mimeitr *itr = data;
  mime_part_list_unwind (&itr->current, NULL);
  free (itr);
  return 0;
}

static int
itrdup (void **ptr, void *owner)
{
  struct mimeitr *orig = owner;
  struct mimeitr *itr;
  struct mime_part *pi, *po;
  size_t n;
  int rc;
  
  itr = malloc (sizeof (*itr));
  if (!itr)
    return ENOMEM;

  itr->parent = malloc (sizeof (*itr->parent));
  if (!itr->parent)
    {
      free (itr);
      return ENOMEM;
    }
  itr->current = itr->parent;
  
  for (n = mime_part_list_len (orig->current); n > 1; n--)
    {
      rc = mime_part_list_alloc (&itr->current, NULL);
      if (rc)
	break;
    }

  if (rc)
    {
      mime_part_list_unwind (&itr->current, NULL);
      free (itr->current);
      return rc;
    }

  pi = itr->current;
  po = orig->current;
  while (po)
    {
      pi->nparts = po->nparts;
      pi->index = po->index;
      pi->mesg = po->mesg;

      pi = pi->up;
      po = po->up;
    }

  *ptr = itr;
  return 0;
}

int
mu_message_get_iterator (mu_message_t msg, mu_iterator_t *pitr)
{
  mu_iterator_t iterator;
  struct mimeitr *itr;
  int rc, ismime;
  size_t nparts = 0;
  
  rc = mu_message_is_multipart (msg, &ismime);
  if (rc == 0)
    {
      if (ismime)
	rc = mu_message_get_num_parts (msg, &nparts);
    }

  if (rc)
    return rc;
  
  itr = malloc (sizeof (*itr));
  if (!itr)
    return ENOMEM;

  itr->parent = malloc (sizeof (*itr->parent));
  if (!itr->parent)
    {
      free (itr);
      return ENOMEM;
    }

  itr->parent->up = NULL;
  itr->parent->mesg = msg;
  itr->parent->index = 0;
  itr->parent->nparts = nparts;
  itr->current = itr->parent;
  itr->stop = 0;
  
  rc = mu_iterator_create (&iterator, itr);
  if (rc)
    {
      free (itr->parent);
      free (itr);
      return rc;
    }

  mu_iterator_set_first (iterator, ismime ? first_mime : first_plain);
  mu_iterator_set_next (iterator, ismime ? next_mime : next_plain);
  mu_iterator_set_getitem (iterator, getitem);
  mu_iterator_set_finished_p (iterator, finished_p);
  mu_iterator_set_destroy (iterator, destroy);
  mu_iterator_set_dup (iterator, itrdup);

  *pitr = iterator;
  return 0;
}
