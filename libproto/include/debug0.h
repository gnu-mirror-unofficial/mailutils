/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999, 2000, 2005  Free Software Foundation, Inc.

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

#ifndef _DEBUG0_H
#define _DEBUG0_H

#ifdef DMALLOC
#  include <dmalloc.h>
#endif

#include <mailutils/debug.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _mu_debug
{
  size_t level;
  char *buffer;
  size_t buflen;
  void *owner;
  int (*_print) (mu_debug_t, size_t level, const char *, va_list);
};

#ifdef __cplusplus
}
#endif

#endif /* _DEBUG0_H */