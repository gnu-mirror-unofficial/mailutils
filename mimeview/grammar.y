%{
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
  
#include <mailutils/cctype.h>
#include <mimeview.h>
#include <grammar.h>
#include <regex.h>

static void
yyprint (FILE *output, unsigned short toknum, YYSTYPE val)
{
  switch (toknum)
    {
    case TYPE:
    case IDENT:
    case STRING:
      fprintf (output, "[%lu] %s", (unsigned long) val.string.len,
	       val.string.ptr);
      break;

    case EOL:
      fprintf (output, "\\n");
      break;
      
    default:
      if (mu_isprint (toknum))
	fprintf (output, "'%c'", toknum);
      else
	fprintf (output, "tok(%d)", toknum);
      break;
    }
}

#define YYPRINT yyprint
  
#define L_OR  0
#define L_AND 1

enum node_type
  {
    true_node,
    functional_node,
    binary_node,
    negation_node,
    suffix_node
  };

union argument
{
  struct mimetypes_string *string;
  unsigned number;
  int c;
  regex_t rx;
};

struct input_file
{
  char const *name;
  mu_stream_t stream;
};
 
typedef int (*builtin_t) (union argument *args, struct input_file *input);

struct node
{
  enum node_type type;
  struct mu_locus_range loc;
  union
  {
    struct
    {
      builtin_t fun;
      union argument *args;
    } function;
    struct node *arg;
    struct
    {
      int op;
      struct node *arg1;
      struct node *arg2;
    } bin; 
    struct mimetypes_string suffix;
  } v;
};

static struct node *make_node (mu_mimetypes_t mth,
                               enum node_type type,
			       struct mu_locus_range const *loc); 
static struct node *make_binary_node (mu_mimetypes_t mth,
                                      int op,
				      struct node *left, struct node *rigth,
				      struct mu_locus_range const *loc);
static struct node *make_negation_node (mu_mimetypes_t mth,
                                        struct node *p,
					struct mu_locus_range const *loc);

static struct node *make_suffix_node (mu_mimetypes_t mth,
                                      struct mimetypes_string *suffix,
				      struct mu_locus_range const *loc);
static struct node *make_functional_node (mu_mimetypes_t mth,
                                          char *ident, mu_list_t list,
					  struct mu_locus_range const *loc);

static int eval_rule (struct node *root, struct input_file *input);

static void *
mimetypes_malloc (mu_mimetypes_t mth, size_t size)
{
  mu_opool_alloc (mth->pool, size);
  return mu_opool_finish (mth->pool, NULL);
}

struct mimetypes_string *     
mimetypes_string_dup (mu_mimetypes_t mth, struct mimetypes_string *s)
{
  mu_opool_append (mth->pool, s, sizeof *s);
  return mu_opool_finish (mth->pool, NULL);
}
 
%}

%define api.pure full
%define api.prefix {mimetypes_yy}

%code requires {
#define MIMETYPES_YYLTYPE struct mu_locus_range

struct rule_tab
{
  char *type;
  int priority;
  struct mu_locus_range loc;
  struct node *node;
};

struct mu_mimetypes
{
  mu_list_t rule_list;
  mu_opool_t pool;
};

typedef void *yyscan_t;  

struct parser_control
{
  mu_linetrack_t trk;
  struct mu_locus_point string_beg; 
  size_t errors;
  mu_list_t arg_list;
  mu_mimetypes_t mth;
};

struct mimetypes_string
{
  char *ptr;
  size_t len;
}; 
}

%code provides {
int mimetypes_yylex (MIMETYPES_YYSTYPE *lvalp, MIMETYPES_YYLTYPE *llocp,
		     yyscan_t yyscanner);
int mimetypes_yylex_init_extra (struct parser_control *, yyscan_t *);
int mimetypes_yylex_destroy (yyscan_t);
void mimetypes_yyerror (MIMETYPES_YYLTYPE const *llocp,
			struct parser_control *pctl, yyscan_t scanner,
			char const *fmt, ...)
  MU_PRINTFLIKE(4,5);
int mimetypes_scanner_open (yyscan_t scanner, const char *name);
 
void lex_next_rule (MIMETYPES_YYLTYPE *llocp, yyscan_t scanner);
}

%parse-param { struct parser_control *pctl } { void *yyscanner }
%lex-param { yyscan_t yyscanner }
%initial-action {
  /*
   * yylloc is a local variable of yyparse.  It is maintained in sync with
   * the trk member of struct parser_control, and need to be deallocated
   * before returning from yyparse.  Since Bison does not provide any
   * %final-action, the variable is deinited in the <<EOF>> scanner rule.
   */
  mu_locus_range_init (&yylloc);
}
%locations
%expect 15

%token <string> TYPE IDENT
%token <string> STRING
%token EOL BOGUS PRIORITY

%left ','
%left '+'

%type <string> arg
%type <list> arglist
%type <node> function stmt rule maybe_rule
%type <result> priority maybe_priority

%union {
  struct mimetypes_string string;
  char *s;
  mu_list_t list;
  int result;
  struct node *node;
}

%%

input    : list
         ;

list     : rule_line
         | list EOL rule_line
         ; 

rule_line: /* empty */ 
         | TYPE maybe_rule maybe_priority
           {
	     struct rule_tab *p = mimetypes_malloc (pctl->mth, sizeof (*p));
	     p->type = $1.ptr;
	     p->node = $2;
	     p->priority = $3;
	     mu_locus_range_init (&p->loc);
	     mu_locus_point_copy (&p->loc.beg, &@1.beg);
	     mu_locus_point_copy (&p->loc.end, &@3.end);
#if 0
	     YY_LOCATION_PRINT (stderr, p->loc);
	     fprintf (stderr, ": rule %s\n", p->type);
#endif
	     mu_list_append (pctl->mth->rule_list, p);
	   }
	 | BOGUS
	   {
	     YYERROR;
	   }
         | error 
           {
	     pctl->errors++;
	     if (pctl->arg_list)
	       mu_list_destroy (&pctl->arg_list);
	     lex_next_rule (&@1, yyscanner);
	     yyerrok;
	     yyclearin;
	   }
         ; 

maybe_rule: /* empty */
           {
	     $$ = make_node (pctl->mth, true_node, &yylloc);
	   }
         | rule
	 ;

rule     : stmt
         | rule rule %prec ','
           {
	     struct mu_locus_range lr;
	     lr.beg = @1.beg;
	     lr.end = @2.end;
	     $$ = make_binary_node (pctl->mth, L_OR, $1, $2, &lr);
	   }
         | rule ',' rule
           {
	     struct mu_locus_range lr;
	     lr.beg = @1.beg;
	     lr.end = @3.end;
	     $$ = make_binary_node (pctl->mth, L_OR, $1, $3, &lr);
	   }
         | rule '+' rule
           {
	     struct mu_locus_range lr;
	     lr.beg = @1.beg;
	     lr.end = @3.end;
	     $$ = make_binary_node (pctl->mth, L_AND, $1, $3, &lr);
	   }
         ;

stmt     : '!' stmt
           {
	     $$ = make_negation_node (pctl->mth, $2, &@2);
	   }
         | '(' rule ')'
           {
	     $$ = $2;
	   }
         | STRING
           {
	     $$ = make_suffix_node (pctl->mth, &$1, &@1);
	   }
         | function
	 | BOGUS
	   {
	     YYERROR;
	   }
         ;

priority : PRIORITY '(' arglist ')'
           {
	     size_t count = 0;
	     struct mimetypes_string *arg;
	     
	     mu_list_count ($3, &count);
	     if (count != 1)
	       {
		 yyerror (&@3, pctl, yyscanner,
			  "%s", _("priority takes single numberic argument"));
		 YYERROR;
	       }
	     mu_list_head ($3, (void**) &arg);
	     $$ = atoi (arg->ptr);
	     mu_list_destroy (&$3);
	   }
         ;

maybe_priority: /* empty */
           {
	     $$ = 100;
	   }
         | priority
	 ;

function : IDENT '(' arglist ')'
           {
	     struct mu_locus_range lr;
	     lr.beg = @1.beg;
	     lr.end = @4.end;
	     
	     $$ = make_functional_node (pctl->mth, $1.ptr, $3, &lr);
	     if (!$$)
	       YYERROR;
	   }
         ;

arglist  : arg
           {
	     mu_list_create (&pctl->arg_list);
	     $$ = pctl->arg_list;
	     mu_list_append ($$, mimetypes_string_dup (pctl->mth, &$1));
	   }
         | arglist ',' arg
           {
	     mu_list_append ($1, mimetypes_string_dup (pctl->mth, &$3));
	     $$ = $1;
	   }
         ;

arg      : STRING
         | BOGUS
           {
	     YYERROR;
	   }
         ;

%%

void
yyerror (MIMETYPES_YYLTYPE const *loc, struct parser_control *pctl,
	 yyscan_t scanner, char const *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  mu_vdiag_at_locus_range (MU_DIAG_ERROR, loc, fmt, ap);
  va_end (ap);
}

static void
parser_control_init (struct parser_control *ctl,
		     char const *filename, struct mu_mimetypes *mth)
{
  memset (ctl, 0, sizeof *ctl);
  mu_linetrack_create (&ctl->trk, filename, 3);
  ctl->mth = mth;
}

static void
parser_control_destroy (struct parser_control *ctl)
{
  mu_linetrack_destroy (&ctl->trk);
  mu_locus_point_deinit (&ctl->string_beg);
  mu_list_destroy (&ctl->arg_list);
}

static void
locus_on (void)
{
  int mode;
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_GET_MODE, &mode);
  mode |= MU_LOGMODE_LOCUS;
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_SET_MODE, &mode);
}

static void
locus_off (void)
{
  int mode;

  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_GET_MODE, &mode);
  mode &= ~MU_LOGMODE_LOCUS;
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_SET_MODE, &mode);
  mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
                   MU_IOCTL_LOGSTREAM_SET_LOCUS_RANGE, NULL);
}

mu_mimetypes_t 
mimetypes_open (const char *name)
{
  int rc;
  struct mu_mimetypes *mtp;
  struct parser_control ctl;
  void *scanner;

  mtp = mu_alloc (sizeof *mtp);
  //FIXME: install destroy_item ?
  mu_list_create (&mtp->rule_list);
  mu_opool_create (&mtp->pool, MU_OPOOL_ENOMEMABRT);

  parser_control_init (&ctl, name, mtp);  

  mimetypes_yylex_init_extra (&ctl, &scanner);
  if (mimetypes_scanner_open (scanner, name))
    rc = 1;
  else
    {
      yydebug = mu_debug_level_p (MU_DEBCAT_APP, MU_DEBUG_TRACE3);
      locus_on ();
      rc = yyparse (&ctl, scanner);
      locus_off ();
    }
  mimetypes_yylex_destroy (scanner);

  if (rc || ctl.errors)
    {
      mu_mimetypes_close (mtp);
      mtp = NULL;
    }
  parser_control_destroy (&ctl);

  return mtp;
}

void
mu_mimetypes_close (mu_mimetypes_t mt)
{
  if (mt)
    {
      mu_list_destroy (&mt->rule_list);
      mu_opool_destroy (&mt->pool);
      free (mt);
    }
}

static struct node *
make_node (mu_mimetypes_t mth,
	   enum node_type type, struct mu_locus_range const *loc)
{
  struct node *p = mimetypes_malloc (mth, sizeof *p);
  p->type = type;
  mu_locus_range_init (&p->loc);
  mu_locus_range_copy (&p->loc, loc);
  return p;
}

static struct node *
make_binary_node (mu_mimetypes_t mth,
		  int op, struct node *left, struct node *right,
		  struct mu_locus_range const *loc)
{
  struct node *node = make_node (mth, binary_node, loc);

  node->v.bin.op = op;
  node->v.bin.arg1 = left;
  node->v.bin.arg2 = right;
  return node;
}

struct node *
make_negation_node (mu_mimetypes_t mth,
		    struct node *p, struct mu_locus_range const *loc)
{
  struct node *node = make_node (mth, negation_node, loc);
  node->v.arg = p;
  return node;
}

struct node *
make_suffix_node (mu_mimetypes_t mth,
		  struct mimetypes_string *suffix,
		  struct mu_locus_range const *loc)
{
  struct node *node = make_node (mth, suffix_node, loc);
  node->v.suffix = *suffix;
  return node;
}

struct builtin_tab
{
  char *name;
  char *args;
  builtin_t handler;
};

/*        match("pattern")
            Pattern match on filename
*/
static int
b_match (union argument *args, struct input_file *input)
{
  return fnmatch (args[0].string->ptr, input->name, 0) == 0;
}

/*       ascii(offset,length)
            True if bytes are valid printable ASCII (CR, NL, TAB,
            BS, 32-126)
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
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
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

/*       printable(offset,length)
            True if bytes are printable 8-bit chars (CR, NL, TAB,
            BS, 32-126, 128-254)
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
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
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

/*        string(offset,"string")
            True if bytes are identical to string
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
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
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

/*        istring(offset,"string")
            True if a case-insensitive comparison of the bytes is
            identical
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
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
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
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
      return 0;
    }
  
  rc = mu_stream_read (input->stream, buf, size, &n);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_read", NULL, rc);
      return 0;
    }
  else if (n != size)
    return 0;
  return memcmp (sample, buf, size) == 0;
}

/*       char(offset,value)
            True if byte is identical
*/
static int
b_char (union argument *args, struct input_file *input)
{
  char val = args[1].number;
  char buf;
  return compare_bytes (args, input, &val, &buf, sizeof (buf));
}

/*        short(offset,value)
            True if 16-bit integer is identical
	  FIXME: Byte order  
*/
static int
b_short (union argument *args, struct input_file *input)
{
  uint16_t val = args[1].number;
  uint16_t buf;
  return compare_bytes (args, input, &val, &buf, sizeof (buf));
}

/*        int(offset,value)
            True if 32-bit integer is identical
          FIXME: Byte order
*/
static int
b_int (union argument *args, struct input_file *input)
{
  uint32_t val = args[1].number;
  uint32_t buf;
  return compare_bytes (args, input, &val, &buf, sizeof (buf));
}

/*        locale("string")
            True if current locale matches string
*/
static int
b_locale (union argument *args, struct input_file *input)
{
  abort (); /* FIXME */
  return 0;
}

/*        contains(offset,range,"string")
            True if the range contains the string
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
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
      return 0;
    }

  buf = mu_alloc (args[1].number);
  rc = mu_stream_read (input->stream, buf, args[1].number, &count);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_read", NULL, rc);
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

/*   regex(offset,"regex")		True if bytes match regular expression
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
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_seek", NULL, rc);
      return 0;
    }

  rc = mu_stream_read (input->stream, buf, sizeof buf - 1, &count);
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_stream_read", NULL, rc);
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
  
struct node *
make_functional_node (mu_mimetypes_t mth,
		      char *ident, mu_list_t list,
		      struct mu_locus_range const *loc)
{
  size_t count, i;
  struct builtin_tab *p;
  struct node *node;
  union argument *args;
  mu_iterator_t itr;
  int rc;
  
  for (p = builtin_tab; ; p++)
    {
      if (!p->name)
	{
	  yyerror (loc, NULL, NULL, _("%s: unknown function"), ident);
	  return NULL;
	}

      if (strcmp (ident, p->name) == 0)
	break;
    }

  mu_list_count (list, &count);
  i = strlen (p->args);

  if (count < i)
    {
      yyerror (loc, NULL, NULL, _("too few arguments in call to `%s'"), ident);
      return NULL;
    }
  else if (count > i)
    {
      yyerror (loc, NULL, NULL, _("too many arguments in call to `%s'"), ident);
      return NULL;
    }

  args = mimetypes_malloc (mth, count * sizeof *args);
  
  mu_list_get_iterator (list, &itr);
  for (i = 0, mu_iterator_first (itr); !mu_iterator_is_done (itr);
       mu_iterator_next (itr), i++)
    {
      struct mimetypes_string *data;
      char *tmp;
      
      mu_iterator_current (itr, (void **)&data);
      switch (p->args[i])
	{
	case 'd':
	  args[i].number = strtoul (data->ptr, &tmp, 0);
	  if (*tmp)
	    goto err;
	  break;
	  
	case 's':
	  args[i].string = data;
	  break;

	case 'x':
	  {
	    char *s;
	    
	    rc = mu_c_str_unescape_trans (data->ptr,
					  "\\\\\"\"a\ab\bf\fn\nr\rt\tv\v", &s);
	    if (rc)
	      {
		mu_diag_funcall (MU_DIAG_ERROR, "mu_c_str_unescape_trans",
				 data->ptr, rc);
		return NULL;
	      }
	    rc = regcomp (&args[i].rx, s, REG_EXTENDED|REG_NOSUB);
	    free (s);
	    if (rc)
	      {
		char errbuf[512];
		regerror (rc, &args[i].rx, errbuf, sizeof errbuf);
		yyerror (loc, NULL, NULL, "%s", errbuf);
		return NULL;
	      }
	  }
	  break;
	  
	case 'c':
	  args[i].c = strtoul (data->ptr, &tmp, 0);
	  if (*tmp)
	    goto err;
	  break;
	  
	default:
	  abort ();
	}
    }

  node = make_node (mth, functional_node, loc);
  node->v.function.fun = p->handler;
  node->v.function.args = args;
  return node;
  
 err:
  {
    yyerror (loc, NULL, NULL,
	     _("argument %lu has wrong type in call to `%s'"),
	     (unsigned long) i, ident); 
    return NULL;
  }
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
  if (mu_debug_level_p (MU_DEBCAT_APP, lev))
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
      result = root->v.function.fun (root->v.function.args, input);
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
      mu_error (_("Cannot open `%s': %s"), file, mu_strerror (rc));
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

  //FIXME: Fix 2nd argument in mu_fd_stream_create prototype
  rc = mu_fd_stream_create (&str, (char*) file, fd, MU_STREAM_READ);
  if (rc)
    {
      mu_error (_("Cannot open `%s': %s"), file, mu_strerror (rc));
      return NULL;
    }
  res = mu_mimetypes_stream_type (mt, file, str);
  mu_stream_destroy (&str);
  return res;
}
