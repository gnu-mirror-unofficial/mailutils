/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2005-2021 Free Software Foundation, Inc.

   GNU Mailutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Mailutils is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mailutils/mailutils.h>
#include <mailutils/locus.h>
#include <mailutils/yyloc.h>
#include <fnmatch.h>

typedef struct mu_mimetypes *mu_mimetypes_t;

mu_mimetypes_t mimetypes_open (const char *name);
void mu_mimetypes_close (mu_mimetypes_t mt);
const char *mu_mimetypes_stream_type (mu_mimetypes_t mt,
				      char const *name, mu_stream_t str);
const char *mu_mimetypes_file_type (mu_mimetypes_t mt, const char *file);
const char *mu_mimetypes_fd_type (mu_mimetypes_t mt, const char *file, int fd);


