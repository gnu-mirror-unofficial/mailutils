/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2020-2021 Free Software Foundation, Inc.

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

#ifndef _MAILUTILS_SYS_TEMP_STREAM_H
#define _MAILUTILS_SYS_TEMP_STREAM_H

#include <mailutils/sys/stream.h>
#include <mailutils/sys/memory_stream.h>
#include <mailutils/sys/temp_file_stream.h>

struct _mu_temp_stream
{
  union
  {
    struct _mu_stream stream;
    struct _mu_memory_stream mem;
    struct _mu_temp_file_stream file;
  } s;
  size_t max_size;
  int (*saved_write) (struct _mu_stream *, const char *, size_t, size_t *);
};
#endif
