/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2021 Free Software Foundation, Inc.

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

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include <regex.h>
#include <inttypes.h>
#include <mailutils/stream.h>
#include <mailutils/locus.h>
#include <mailutils/diag.h>
#include <mailutils/cctype.h>
#include <mailutils/cstr.h>
#include <mailutils/stdstream.h>
#include <mailutils/list.h>
#include <mailutils/nls.h>
#include <mailutils/errno.h>
#include <mailutils/sys/mimetypes.h>
#include <mailutils/mimetypes.h>

/*
 * match("pattern")
 *        Pattern match on filename
 */
static int
b_match (union argument *args, struct input_file *input)
{
  return fnmatch (args[0].string->ptr, input->name, 0) == 0;
}

/*
 * ascii(offset,length)
 *        True if bytes are valid printable ASCII (CR, NL, TAB,
 *        BS, 32-126)
 */
#define ISASCII(c) ((c) &&\
                    (strchr ("\n\r\t\b",c) \
                     || (32<=((unsigned) c) && ((unsigned) c)<=126)))
static int
b_ascii (union argument *args, struct input_file *input)
{
  int i;
  int rc;

  rc = mu_stream_seek (input->stream, args[0].number, MU_SEEK_SET, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("mu_stream_seek: %s", mu_strerror (rc)));
      return 0;
    }

  for (i = 0; i < args[1].number; i++)
    {
      unsigned char c;
      size_t n;

      rc = mu_stream_read (input->stream, &c, 1, &n);
      if (rc || n == 0)
	break;
      if (!ISASCII (c))
	return 0;
    }
      
  return 1;
}

/*
 * printable(offset,length)
 *        True if bytes are printable 8-bit chars (CR, NL, TAB,
 *        BS, 32-126, 128-254)
 */
#define ISPRINT(c) (ISASCII (c) \
		    || (128<=((unsigned) c) && ((unsigned) c)<=254))
static int
b_printable (union argument *args, struct input_file *input)
{
  int i;
  int rc;

  rc = mu_stream_seek (input->stream, args[0].number, MU_SEEK_SET, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("mu_stream_seek: %s", mu_strerror (rc)));
      return 0;
    }

  for (i = 0; i < args[1].number; i++)
    {
      unsigned char c;
      size_t n;

      rc = mu_stream_read (input->stream, &c, 1, &n);
      if (rc || n == 0)
	break;
      if (!ISPRINT (c))
	return 0;
    }
  return 1;
}

/*
 * string(offset,"string")
 *        True if bytes are identical to string
 */
static int
b_string (union argument *args, struct input_file *input)
{
  struct mimetypes_string *str = args[1].string;
  int i;
  int rc;
  
  rc = mu_stream_seek (input->stream, args[0].number, MU_SEEK_SET, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("mu_stream_seek: %s", mu_strerror (rc)));
      return 0;
    }

  for (i = 0; i < str->len; i++)
    {
      char c;
      size_t n;

      rc = mu_stream_read (input->stream, &c, 1, &n);
      if (rc || n == 0 || c != str->ptr[i])
	return 0;
    }
  return 1;
}

/*
 * istring(offset,"string")
 *        True if a case-insensitive comparison of the bytes is
 *        identical
 */
static int
b_istring (union argument *args, struct input_file *input)
{
  int i;
  struct mimetypes_string *str = args[1].string;
  
  int rc;

  rc = mu_stream_seek (input->stream, args[0].number, MU_SEEK_SET, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("mu_stream_seek: %s", mu_strerror (rc)));
      return 0;
    }

  for (i = 0; i < str->len; i++)
    {
      char c;
      size_t n;

      rc = mu_stream_read (input->stream, &c, 1, &n);
      if (rc || n == 0 || mu_tolower (c) != mu_tolower (str->ptr[i]))
	return 0;
    }
  return 1;
}

int
compare_bytes (union argument *args, struct input_file *input,
	       void *sample, void *buf, size_t size)
{
  int rc;
  size_t n;
  
  rc = mu_stream_seek (input->stream, args[0].number, MU_SEEK_SET, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("mu_stream_seek: %s", mu_strerror (rc)));
      return 0;
    }
  
  rc = mu_stream_read (input->stream, buf, size, &n);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("mu_stream_read: %s", mu_strerror (rc)));
      return 0;
    }
  else if (n != size)
    return 0;
  return memcmp (sample, buf, size) == 0;
}

/*
 * char(offset,value)
 *        True if byte is identical
 */
static int
b_char (union argument *args, struct input_file *input)
{
  char val = args[1].number;
  char buf;
  return compare_bytes (args, input, &val, &buf, sizeof (buf));
}

/*
 * short(offset,value)
 *        True if 16-bit integer is identical
 *	  FIXME: Byte order  
 */
static int
b_short (union argument *args, struct input_file *input)
{
  uint16_t val = args[1].number;
  uint16_t buf;
  return compare_bytes (args, input, &val, &buf, sizeof (buf));
}

/*
 * int(offset,value)
 *        True if 32-bit integer is identical
 *        FIXME: Byte order
 */
static int
b_int (union argument *args, struct input_file *input)
{
  uint32_t val = args[1].number;
  uint32_t buf;
  return compare_bytes (args, input, &val, &buf, sizeof (buf));
}

/*
 * locale("string")
 *        True if current locale matches string
 */
static int
b_locale (union argument *args, struct input_file *input)
{
  abort (); /* FIXME */
  return 0;
}

/*
 * contains(offset,range,"string")
 *        True if the range contains the string
 */
static int
b_contains (union argument *args, struct input_file *input)
{
  size_t i, count;
  char *buf;
  struct mimetypes_string *str = args[2].string;
  int rc;

  rc = mu_stream_seek (input->stream, args[0].number, MU_SEEK_SET, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("mu_stream_seek: %s", mu_strerror (rc)));
      return 0;
    }

  if ((buf = malloc (args[1].number)) == NULL)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("malloc: %s", mu_strerror (rc)));
      return 0;
    }
  
  rc = mu_stream_read (input->stream, buf, args[1].number, &count);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("mu_stream_read: %s", mu_strerror (rc)));
    }
  else if (count > str->len)
    for (i = 0; i <= count - str->len; i++)
      if (buf[i] == str->ptr[0] && memcmp (buf + i, str->ptr, str->len) == 0)
	{
	  free (buf);
	  return 1;
	}
  free (buf);
  return 0;
}

#define MIME_MAX_BUFFER 4096

/*
 * regex(offset,"regex")
 *        True if bytes match regular expression
 */
static int
b_regex (union argument *args, struct input_file *input)
{
  size_t count;
  int rc;
  char buf[MIME_MAX_BUFFER];
  
  rc = mu_stream_seek (input->stream, args[0].number, MU_SEEK_SET, NULL);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("mu_stream_seek: %s", mu_strerror (rc)));
      return 0;
    }

  rc = mu_stream_read (input->stream, buf, sizeof buf - 1, &count);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("mu_stream_read: %s", mu_strerror (rc)));
      return 0;
    }
  buf[count] = 0;

  return regexec (&args[1].rx, buf, 0, NULL, 0) == 0;
} 
  

static struct builtin_tab builtin_tab[] = {
  { "match", "s", b_match },
  { "ascii", "dd", b_ascii },
  { "printable", "dd", b_printable },
  { "regex", "dx", b_regex },
  { "string", "ds", b_string },
  { "istring", "ds", b_istring },
  { "char", "dc", b_char },
  { "short", "dd", b_short },
  { "int", "dd", b_int },
  { "locale", "s", b_locale },
  { "contains", "dds", b_contains },
  { NULL }
};
  
struct builtin_tab const *
mu_mimetypes_builtin (char const *ident)
{
  struct builtin_tab *p;
  for (p = builtin_tab; p->name; p++)
    if (strcmp (ident, p->name) == 0)
      return p;
  return NULL;
}

static int
check_suffix (char *suf, struct input_file *input)
{
  char *p = strrchr (input->name, '.');
  if (!p)
    return 0;
  return strcmp (p+1, suf) == 0;
}

void
mime_debug (int lev, struct mu_locus_range const *loc, char const *fmt, ...)
{
  if (mu_debug_level_p (MU_DEBCAT_MIMETYPES, lev))
    {
      va_list ap;

      if (loc->beg.mu_col == 0)					       
	mu_debug_log_begin ("%s:%u", loc->beg.mu_file, loc->beg.mu_line);
      else if (strcmp(loc->beg.mu_file, loc->end.mu_file))
	mu_debug_log_begin ("%s:%u.%u-%s:%u.%u",
			    loc->beg.mu_file,
			    loc->beg.mu_line, loc->beg.mu_col,
			    loc->end.mu_file,
			    loc->end.mu_line, loc->end.mu_col);
      else if (loc->beg.mu_line != loc->end.mu_line)
	mu_debug_log_begin ("%s:%u.%u-%u.%u",
			    loc->beg.mu_file,
			    loc->beg.mu_line, loc->beg.mu_col,
			    loc->end.mu_line, loc->end.mu_col);
      else if (loc->beg.mu_col != loc->end.mu_col)
	mu_debug_log_begin ("%s:%u.%u-%u",
			    loc->beg.mu_file,
			    loc->beg.mu_line, loc->beg.mu_col,
			    loc->end.mu_col);
      else
	mu_debug_log_begin ("%s:%u.%u",
			    loc->beg.mu_file,
			    loc->beg.mu_line, loc->beg.mu_col);

      mu_stream_write (mu_strerr, ": ", 2, NULL);

      va_start (ap, fmt);
      mu_stream_vprintf (mu_strerr, fmt, ap);
      va_end (ap);
      mu_debug_log_nl ();
    }
}

static int
eval_rule (struct node *root, struct input_file *input)
{
  int result;
  
  switch (root->type)
    {
    case true_node:
      result = 1;
      break;
      
    case functional_node:
      result = root->v.function.builtin->handler (root->v.function.args, input);
      break;
      
    case binary_node:
      result = eval_rule (root->v.bin.arg1, input);
      switch (root->v.bin.op)
	{
	case L_OR:
	  if (!result)
	    result |= eval_rule (root->v.bin.arg2, input);
	  break;
	  
	case L_AND:
	  if (result)
	    result &= eval_rule (root->v.bin.arg2, input);
	  break;
	  
	default:
	  abort ();
	}
      break;
      
    case negation_node:
      result = !eval_rule (root->v.arg, input);
      break;
      
    case suffix_node:
      result = check_suffix (root->v.suffix.ptr, input);
      break;

    default:
      abort ();
    }
  mime_debug (MU_DEBUG_TRACE2, &root->loc, "result %s", result ? "true" : "false");
  return result;
}

static int
evaluate (void **itmv, size_t itmc, void *call_data)
{
  struct rule_tab *p = itmv[0];
  if (eval_rule (p->node, call_data))
    {
      itmv[0] = p;
      mime_debug (MU_DEBUG_TRACE1, &p->loc, "rule %s matches", p->type);
      return MU_LIST_MAP_OK;
    }
  return MU_LIST_MAP_SKIP;
}

static int
rule_cmp (const void *a, const void *b)
{
  struct rule_tab const *arule = a;
  struct rule_tab const *brule = b;

  if (arule->priority == brule->priority)
    {
      if (arule->node->type == true_node
	  && brule->node->type != true_node)
	return 1;
      else if (brule->node->type == true_node
	       && arule->node->type != true_node)
	return -1;
      else
	return mu_c_strcasecmp (arule->type, brule->type);
    }
  return arule->priority - brule->priority;
}

const char *
mu_mimetypes_stream_type (mu_mimetypes_t mt, char const *name, mu_stream_t str)
{
  mu_list_t res = NULL;
  const char *type = NULL;
  struct input_file input;

  input.name = name;
  input.stream = str;
  
  mu_stream_seek (str, 0, MU_SEEK_SET, NULL);
  mu_list_map (mt->rule_list, evaluate, &input, 1, &res);
  if (!mu_list_is_empty (res))
    {
      struct rule_tab *rule;
      mu_list_sort (res, rule_cmp);
      mu_list_head (res, (void**) &rule);
      mime_debug (MU_DEBUG_TRACE0, &rule->loc, "selected rule %s", rule->type);
      type = rule->type;
    }
  mu_list_destroy (&res);
  return type;
}
    
const char *
mu_mimetypes_file_type (mu_mimetypes_t mt, const char *file)
{
  int rc;
  mu_stream_t str;
  const char *res;
  
  rc = mu_file_stream_create (&str, file, MU_STREAM_READ);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("cannot open %s: %s", file, mu_strerror (rc)));
      return NULL;
    }
  res = mu_mimetypes_stream_type (mt, file, str);
  mu_stream_destroy (&str);
  return res;
}

const char *
mu_mimetypes_fd_type (mu_mimetypes_t mt, const char *file, int fd)
{
  int rc;
  mu_stream_t str;
  const char *res;

  rc = mu_fd_stream_create (&str, file, fd, MU_STREAM_READ);
  if (rc)
    {
      mu_debug (MU_DEBCAT_MIMETYPES, MU_DEBUG_ERROR,
		("cannot open %s: %s", file, mu_strerror (rc)));
      return NULL;
    }
  res = mu_mimetypes_stream_type (mt, file, str);
  mu_stream_destroy (&str);
  return res;
}
