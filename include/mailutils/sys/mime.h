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

#ifndef _MAILUTILS_SYS_MIME_H
#define _MAILUTILS_SYS_MIME_H

#include <sys/types.h>
#include <mailutils/mime.h>
#include <mailutils/util.h>

#ifdef __cplusplus
extern "C" { 
#endif

/* Parser states */
enum
  {
    MIME_STATE_SCAN_BOUNDARY,
    MIME_STATE_HEADERS,
    MIME_STATE_END
  };

#define MIME_FLAG_MASK             0x0000ffff

/* private */
#define MIME_PARSER_ACTIVE         0x80000000
#define MIME_NEW_MESSAGE           0x20000000
#define MIME_ADDED_CT              0x10000000
#define MIME_ADDED_MULTIPART_CT    0x08000000
#define MIME_INSERT_BOUNDARY       0x04000000
#define MIME_ADDING_BOUNDARY       0x02000000
#define MIME_SEEK_ACTIVE           0x01000000

struct _mu_mime
{
  int ref_count;
  mu_message_t       msg;
  mu_header_t        hdrs;
  mu_stream_t        stream;
  int                flags;
  mu_content_type_t  content_type;

  size_t          tparts;
  size_t          nmtp_parts;
  struct _mime_part **mtp_parts;      /* list of parts in the msg */
  char const     *boundary;
  size_t          cur_offset;
  size_t          cur_part;
  size_t          part_offset;
  size_t          boundary_len;
  size_t          preamble;
  size_t          postamble;
  mu_stream_t     part_stream;
  /* parser state */
  char           *cur_line;           /* Line buffer */
  size_t          line_length;        /* Length of line in cur_line */
  size_t          line_size;          /* Actual capacity of cur_line */
  char           *header_buf;
  size_t          header_buf_size;
  size_t          header_length;
  size_t          body_offset;
  size_t          body_length;
  size_t          body_lines;
  int             parser_state;
};

struct _mime_part
{
  mu_mime_t          mime;
  mu_message_t       msg;
  int                body_created;
  size_t             offset;
  size_t             len;
  size_t             lines;
};

#ifdef __cplusplus
}
#endif

#endif                          /* MIME0_H */
