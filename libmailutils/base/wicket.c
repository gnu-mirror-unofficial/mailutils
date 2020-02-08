/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2020 Free Software Foundation, Inc.

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

#include <mailutils/errno.h>
#include <mailutils/sys/auth.h>
#include <mailutils/sys/url.h>

int
mu_wicket_create (mu_wicket_t *pwicket)
{
  mu_wicket_t wicket = calloc (1, sizeof (*wicket));
  if (!wicket)
    return ENOMEM;
  wicket->refcnt = 1;
  *pwicket = wicket;
  return 0;
}

int
mu_wicket_get_ticket (mu_wicket_t wicket, const char *user,
		      mu_ticket_t *pticket)
{
  if (!wicket)
    return EINVAL;
  if (!pticket)
    return EINVAL;
  if (!wicket->_get_ticket)
    return ENOSYS;
  return wicket->_get_ticket (wicket, wicket->data, user, pticket);
}

int
mu_wicket_ref (mu_wicket_t wicket)
{
  if (!wicket)
    return EINVAL;
  wicket->refcnt++;
  return 0;
}

int
mu_wicket_unref (mu_wicket_t wicket)
{
  if (!wicket)
    return EINVAL;
  if (wicket->refcnt)
    wicket->refcnt--;
  if (wicket->refcnt == 0)
    {
      if (wicket->_destroy)
	wicket->_destroy (wicket);
      free (wicket);
      return 0;
    }
  return MU_ERR_EXISTS;
}


void
mu_wicket_destroy (mu_wicket_t *pwicket)
{
  if (pwicket && *pwicket && mu_wicket_unref (*pwicket) == 0)
    *pwicket = NULL;
}

int
mu_wicket_set_destroy (mu_wicket_t wicket, void (*_destroy) (mu_wicket_t))
{
  if (!wicket)
    return EINVAL;
  wicket->_destroy = _destroy;
  return 0;
}

int
mu_wicket_set_data (mu_wicket_t wicket, void *data)
{
  if (!wicket)
    return EINVAL;
  wicket->data = data;
  return 0;
}

void *
mu_wicket_get_data (mu_wicket_t wicket)
{
  if (!wicket)
    return NULL;
  return wicket->data;
}

int
mu_wicket_set_get_ticket (mu_wicket_t wicket,
			  int (*_get_ticket) (mu_wicket_t, void *,
					      const char *, mu_ticket_t *))
{
  if (!wicket)
    return EINVAL;
  wicket->_get_ticket = _get_ticket;
  return 0;
}

