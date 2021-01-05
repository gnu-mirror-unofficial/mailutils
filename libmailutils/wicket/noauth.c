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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <mailutils/errno.h>
#include <mailutils/auth.h>

static int
noauth_ticket_get_cred (mu_ticket_t ticket, mu_url_t url, const char *challenge,
			char **pplain, mu_secret_t *psec)
{
  return MU_ERR_AUTH_NO_CRED;
}

int
mu_noauth_ticket_create (mu_ticket_t *pticket)
{
  mu_ticket_t ticket;
  int rc;

  rc = mu_ticket_create (&ticket, NULL);
  if (rc)
    return rc;
  mu_ticket_set_get_cred (ticket, noauth_ticket_get_cred, NULL);
  *pticket = ticket;
  return 0;
}

static int
noauth_get_ticket (mu_wicket_t wicket, void *data,
		   const char *user, mu_ticket_t *pticket)
{
  return mu_noauth_ticket_create (pticket);
}

int
mu_noauth_wicket_create (mu_wicket_t *pwicket)
{
  mu_wicket_t wicket;
  int rc;

  rc = mu_wicket_create (&wicket);
  if (rc)
    return rc;
  mu_wicket_set_get_ticket (wicket, noauth_get_ticket);
  return 0;
}
