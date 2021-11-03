/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2005-2021 Free Software Foundation, Inc.

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

#ifndef _MAILUTILS_MIMETYPES_H
#define _MAILUTILS_MIMETYPES_H

#include <mailutils/types.h>

typedef struct mu_mimetypes *mu_mimetypes_t;

mu_mimetypes_t mu_mimetypes_open (const char *name);
void mu_mimetypes_close (mu_mimetypes_t mt);
const char *mu_mimetypes_stream_type (mu_mimetypes_t mt,
				      char const *name, mu_stream_t str);
const char *mu_mimetypes_file_type (mu_mimetypes_t mt, const char *file);
const char *mu_mimetypes_fd_type (mu_mimetypes_t mt, const char *file, int fd);

#endif

