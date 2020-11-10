/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2020 Free Software Foundation, Inc.

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

#ifndef _MAILUTILS_CSTR_H
#define _MAILUTILS_CSTR_H

#ifdef __cplusplus
extern "C" {
#endif

# include <mailutils/types.h>
  
int mu_strlower (char *);
int mu_strupper (char *);

int mu_c_strcasecmp (const char *a, const char *b);
int mu_c_strncasecmp (const char *a, const char *b, size_t n);
char *mu_c_strcasestr (const char *haystack, const char *needle);

int mu_string_prefix (char const *str, char const *pfx);
int mu_string_suffix (char const *str, char const *sfx);
  
size_t mu_rtrim_class (char *str, int __class);
size_t mu_rtrim_cset (char *str, const char *cset);
size_t mu_ltrim_class (char *str, int __class);
size_t mu_ltrim_cset (char *str, const char *cset);

char *mu_str_skip_class (const char *str, int __class);
char *mu_str_skip_cset (const char *str, const char *cset);

char *mu_str_skip_class_comp (const char *str, int __class);
char *mu_str_skip_cset_comp (const char *str, const char *cset);

char *mu_str_stripws (char *string);  

int mu_string_split (const char *string, char *delim, mu_list_t list);

size_t mu_str_count (char const *str, char const *chr, size_t *cnt);
size_t mu_mem_c_count (char const *str, int c, size_t len);
size_t mu_mem_8bit_count (char const *str, size_t len);

int mu_c_str_escape (char const *str, char const *chr, char const *xtab,
		     char **ret_str);
int mu_c_str_escape_trans (char const *str, char const *trans, char **ret_str);

int mu_c_str_unescape_inplace (char *str, char const *chr, char const *xtab);
int mu_c_str_unescape (char const *str, char const *chr, char const *xtab,
		       char **ret_str);
int mu_c_str_unescape_trans (char const *str, char const *trans,
			     char **ret_str);


int mu_version_string_parse (char const *verstr, int version[3], char **endp);
int mu_version_string_cmp (char const *a, char const *b, int ignoresuf, int *res);
  
int mu_str_expand (char **output, char const *input, mu_assoc_t assoc);
int mu_str_vexpand (char **output, char const *input, ...);

int mu_strtosize (char const *str, char **endp, size_t *ret_val);

#ifdef __cplusplus
}
#endif
  
#endif

