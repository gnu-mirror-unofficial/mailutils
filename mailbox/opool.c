/* String-list functions for GNU Mailutils.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.

   Based on slist module from GNU Radius.  Written by Sergey Poznyakoff.
   
   GNU Mailutils is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mailutils/types.h>
#include <mailutils/alloc.h>
#include <mailutils/opool.h>

struct mu_opool_bucket
{
  struct mu_opool_bucket *next;
  char *buf;
  size_t level;
  size_t size;
};

struct _mu_opool
{
  int memerr;
  struct mu_opool_bucket *head, *tail;
  struct mu_opool_bucket *free;
};

static struct mu_opool_bucket *
alloc_bucket (struct _mu_opool *opool, size_t size)
{
  struct mu_opool_bucket *p = malloc (sizeof (*p) + size);
  if (!p)
    {
      if (opool->memerr)
	mu_alloc_die ();
    }
  else
    {
      p->buf = (char*)(p + 1);
      p->level = 0;
      p->size = size;
      p->next = NULL;
    }
  return p;
}

static int
alloc_pool (mu_opool_t opool, size_t size)
{
  struct mu_opool_bucket *p = alloc_bucket (opool, MU_OPOOL_BUCKET_SIZE);
  if (!p)
    return ENOMEM;
  if (opool->tail)
    opool->tail->next = p;
  else
    opool->head = p;
  opool->tail = p;
  return 0;
}

static int
copy_chars (mu_opool_t opool, const char *str, size_t n, size_t *psize)
{
  size_t rest;

  if (!opool->head || opool->tail->level == opool->tail->size)
    if (alloc_pool (opool, MU_OPOOL_BUCKET_SIZE))
      return ENOMEM;
  rest = opool->tail->size - opool->tail->level;
  if (n > rest)
    n = rest;
  memcpy (opool->tail->buf + opool->tail->level, str, n);
  opool->tail->level += n;
  *psize = n;
  return 0;
}

int
mu_opool_create (mu_opool_t *pret, int memerr)
{
  struct _mu_opool *x = malloc (sizeof (x[0]));
  if (!x)
    {
      if (memerr)
	mu_alloc_die ();
      return ENOMEM;
    }
  x->memerr = memerr;
  x->head = x->tail = x->free = 0;
  *pret = x;
  return 0;
}

void
mu_opool_clear (mu_opool_t opool)
{
  if (!opool)
    return;
  
  if (opool->tail)
    {
      opool->tail->next = opool->free;
      opool->free = opool->head;
      opool->head = opool->tail = NULL;
    }
}	

void
mu_opool_destroy (mu_opool_t *popool)
{
  struct mu_opool_bucket *p;
  if (popool && *popool)
    {
      mu_opool_t opool = *popool;
      mu_opool_clear (opool);
      for (p = opool->free; p; )
	{
	  struct mu_opool_bucket *next = p->next;
	  free (p);
	  p = next;
	}
      free (opool);
    }
  *popool = NULL;
}

int
mu_opool_append (mu_opool_t opool, const void *str, size_t n)
{
  const char *ptr = str;
  while (n)
    {
      size_t s;
      if (copy_chars (opool, ptr, n, &s))
	return ENOMEM;
      ptr += s;
      n -= s;
    }
  return 0;
}

int
mu_opool_append_char (mu_opool_t opool, char c)
{
  return mu_opool_append (opool, &c, 1);
}	

int
mu_opool_appendz (mu_opool_t opool, const char *str)
{
  return mu_opool_append (opool, str, strlen (str))
         || mu_opool_append_char (opool, 0);
}

size_t
mu_opool_size (mu_opool_t opool)
{
  size_t size = 0;
  struct mu_opool_bucket *p;
  for (p = opool->head; p; p = p->next)
    size += p->level;
  return size;
}

int
mu_opool_coalesce (mu_opool_t opool, size_t *psize)
{
  size_t size;

  if (opool->head && opool->head->next == NULL)
    size = opool->head->level;
  else {
    struct mu_opool_bucket *bucket;
    struct mu_opool_bucket *p;

    size = mu_opool_size (opool);
	
    bucket = alloc_bucket (opool, size);
    if (!bucket)
      return ENOMEM;
    for (p = opool->head; p; )
      {
	struct mu_opool_bucket *next = p->next;
	memcpy (bucket->buf + bucket->level, p->buf, p->level);
	bucket->level += p->level;
	free (p);
	p = next;
      }
    opool->head = opool->tail = bucket;
  }
  if (psize)
    *psize = size;
  return 0;
}

void *
mu_opool_head (mu_opool_t opool, size_t *psize)
{
  if (*psize) 
    *psize = opool->head ? opool->head->level : 0;
  return opool->head ? opool->head->buf : NULL;
}

void *
mu_opool_finish (mu_opool_t opool, size_t *psize)
{
  if (mu_opool_coalesce (opool, psize))
    return NULL;
  mu_opool_clear (opool);
  return opool->free->buf;
}
  