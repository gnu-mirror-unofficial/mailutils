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

#ifndef _MAILUTILS_MAILCAP_H
#define _MAILUTILS_MAILCAP_H

#include <mailutils/types.h>
#include <mailutils/errno.h>
#include <mailutils/locus.h>
#include <mailutils/util.h>

/* See RFC1524 (A User Agent Configuration Mechanism).  */

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

struct mu_mailcap_selector_closure
{
  int (*selector) (mu_mailcap_entry_t, void *);
  void *data;
  void (*data_free) (void *);
};

struct mu_mailcap_error_closure
{
  void (*error) (void *, struct mu_locus_range const *, char const *);
  void *data;
  void (*data_free) (void *);
};

extern struct mu_mailcap_error_closure mu_mailcap_default_error_closure;

#define MU_MAILCAP_FLAG_DEFAULT 0
#define MU_MAILCAP_FLAG_LOCUS   0x1

int mu_mailcap_create (mu_mailcap_t *pmailcap);
void mu_mailcap_destroy (mu_mailcap_t *pmailcap);

int mu_mailcap_set_flags (mu_mailcap_t mailcap, int flags);
int mu_mailcap_get_flags (mu_mailcap_t mailcap, int *flags);

int mu_mailcap_set_error (mu_mailcap_t mailcap,
			  struct mu_mailcap_error_closure const *err);
int mu_mailcap_get_error (mu_mailcap_t mailcap,
			  struct mu_mailcap_error_closure *err);
int mu_mailcap_set_selector (mu_mailcap_t mailcap,
			     struct mu_mailcap_selector_closure const *sel);
int mu_mailcap_get_selector (mu_mailcap_t mailcap,
			     struct mu_mailcap_selector_closure *sel);

int mu_mailcap_get_count (mu_mailcap_t mailcap, size_t *pcount);
int mu_mailcap_get_iterator (mu_mailcap_t mailcap, mu_iterator_t *pitr);
int mu_mailcap_foreach (mu_mailcap_t mailcap,
			int (*action) (mu_mailcap_entry_t, void *),
			void *data);
int mu_mailcap_get_entry (mu_mailcap_t mailcap, size_t n,
			  mu_mailcap_entry_t *entry);
int mu_mailcap_find_entry (mu_mailcap_t mailcap, char const *type,
			   mu_mailcap_entry_t *entry);

int mu_mailcap_parse (mu_mailcap_t mailcap, mu_stream_t input,
		      struct mu_locus_point const *pt);
int mu_mailcap_parse_file (mu_mailcap_t mailcap, char const *file_name);

int mu_mailcap_entry_create (mu_mailcap_entry_t *ret_entry,
			     char *type, char *command);
void mu_mailcap_entry_destroy (mu_mailcap_entry_t *pent);
void mu_mailcap_entry_destroy_item (void *ptr);

int mu_mailcap_entry_sget_type (mu_mailcap_entry_t ent, char const **ptype);
int mu_mailcap_entry_aget_type (mu_mailcap_entry_t ent, char **ptype);
int mu_mailcap_entry_get_type (mu_mailcap_entry_t ent,
			       char *buffer, size_t buflen,
			       size_t *pn);

int mu_mailcap_entry_sget_command (mu_mailcap_entry_t ent, char const **pcommand);
int mu_mailcap_entry_aget_command (mu_mailcap_entry_t ent, char **pcommand);
int mu_mailcap_entry_get_command (mu_mailcap_entry_t ent,
				  char *buffer, size_t buflen,
				  size_t *pn);

int mu_mailcap_entry_get_locus (mu_mailcap_entry_t ent,
				struct mu_locus_range *loc);

void mu_mailcap_entry_field_deallocate (void *ptr);
int mu_mailcap_entry_set_bool (mu_mailcap_entry_t ent, char const *name);
int mu_mailcap_entry_set_string (mu_mailcap_entry_t ent, char const *name,
				 char const *value);
int mu_mailcap_entry_field_unset (mu_mailcap_entry_t ent, char const *name);
int mu_mailcap_entry_fields_count (mu_mailcap_entry_t ent, size_t *pcount);
int mu_mailcap_entry_fields_foreach (mu_mailcap_entry_t ent,
			      int (*action) (char const *, char const *, void *),
			      void *data);
int mu_mailcap_entry_fields_get_iterator (mu_mailcap_entry_t ent,
					  mu_iterator_t *pitr);

int mu_mailcap_entry_sget_field (mu_mailcap_entry_t ent, char const *name,
				 char const **pval);
int mu_mailcap_entry_aget_field (mu_mailcap_entry_t ent, char const *name,
				 char **pval);
int mu_mailcap_entry_get_field (mu_mailcap_entry_t ent,
				char const *name,
				char *buffer, size_t buflen,
				size_t *pn);

#define MU_MAILCAP_NEEDSTERMINAL "needsterminal"
#define MU_MAILCAP_COPIOUSOUTPUT "copiousoutput"
#define MU_MAILCAP_COMPOSE "compose"
#define MU_MAILCAP_COMPOSETYPED "composetyped"
#define MU_MAILCAP_PRINT "print"
#define MU_MAILCAP_EDIT "edit"
#define MU_MAILCAP_TEST "test"
#define MU_MAILCAP_X11_BITMAP "x11-bitmap"
#define MU_MAILCAP_TEXTUALNEWLINES "textualnewlines"
#define MU_MAILCAP_DESCRIPTION "description"

int mu_mailcap_string_match (char const *pattern, int delim, char const *type);
int mu_mailcap_content_type_match (const char *pattern, int delim,
				   mu_content_type_t ct);

typedef struct _mu_mailcap_finder *mu_mailcap_finder_t;

int mu_mailcap_finder_create (mu_mailcap_finder_t *, int,
			      struct mu_mailcap_selector_closure *,
			      struct mu_mailcap_error_closure *,
			      char **file_names);
int mu_mailcap_finder_next_match (mu_mailcap_finder_t, mu_mailcap_entry_t *);
void mu_mailcap_finder_destroy (mu_mailcap_finder_t *);

#ifdef __cplusplus
}
#endif

#endif
