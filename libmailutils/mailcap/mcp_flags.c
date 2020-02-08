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

#include <config.h>
#include <mailutils/sys/mailcap.h>

int
mu_mailcap_set_flags (mu_mailcap_t mailcap, int flags)
{
  if (!mailcap)
    return EINVAL;
  mailcap->flags = flags;
  return 0;
}

int
mu_mailcap_get_flags (mu_mailcap_t mailcap, int *flags)
{
  if (!mailcap)
    return EINVAL;
  if (!flags)
    return MU_ERR_OUT_PTR_NULL;
  *flags = mailcap->flags;
  return 0;
}
