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

#include <mailutils/mimetypes.h>
#include <grammar.h>

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

static void *
mimetypes_malloc (mu_mimetypes_t mth, size_t size)
{
  mu_opool_alloc (mth->pool, size);
  return mu_opool_finish (mth->pool, NULL);
}

static struct mimetypes_string *     
mimetypes_string_dup (mu_mimetypes_t mth, struct mimetypes_string *s)
{
  mu_opool_append (mth->pool, s, sizeof *s);
  return mu_opool_finish (mth->pool, NULL);
}

static void rule_destroy_item (void *ptr);
 
%}

%define api.pure full
%define api.prefix {mimetypes_yy}

%code requires {
#include <stdio.h>
#include <regex.h>  
#include <mailutils/stream.h>  
#include <mailutils/cctype.h>
#include <mailutils/cstr.h>  
#include <mailutils/locus.h>
#include <mailutils/yyloc.h>
#include <mailutils/opool.h>
#include <mailutils/list.h>
#include <mailutils/nls.h>
#include <mailutils/diag.h>
#include <mailutils/stdstream.h>
#include <mailutils/iterator.h>
#include <mailutils/util.h>  
#include <mailutils/mimetypes.h>
#include <mailutils/sys/mimetypes.h>

#define MIMETYPES_YYLTYPE struct mu_locus_range

typedef void *yyscan_t;  

struct parser_control
{
  mu_linetrack_t trk;
  struct mu_locus_point string_beg; 
  size_t errors;
  mu_mimetypes_t mth;
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

%destructor { mu_list_destroy (&$$); } <list>

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
	     mu_list_destroy (&$3);
	   }
         ;

arglist  : arg
           {
	     mu_list_create (&$$);
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
mu_mimetypes_open (const char *name)
{
  int rc;
  struct mu_mimetypes *mtp;
  struct parser_control ctl;
  void *scanner;

  mtp = calloc (1, sizeof *mtp);
  if (!mtp)
    return NULL;

  //FIXME: install destroy_item ?
  if (mu_list_create (&mtp->rule_list)
      || mu_opool_create (&mtp->pool, MU_OPOOL_DEFAULT))
    rc = 1;
  else
    {
      mu_list_set_destroy_item (mtp->rule_list, rule_destroy_item);
      parser_control_init (&ctl, name, mtp);  

      mimetypes_yylex_init_extra (&ctl, &scanner);
      if (mimetypes_scanner_open (scanner, name))
	rc = 1;
      else
	{
	  yydebug = mu_debug_level_p (MU_DEBCAT_MIMETYPES, MU_DEBUG_TRACE3);
	  locus_on ();
	  rc = yyparse (&ctl, scanner);
	  locus_off ();
	}
      mimetypes_yylex_destroy (scanner);
    }
  
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

static struct node *
make_negation_node (mu_mimetypes_t mth,
		    struct node *p, struct mu_locus_range const *loc)
{
  struct node *node = make_node (mth, negation_node, loc);
  node->v.arg = p;
  return node;
}

static struct node *
make_suffix_node (mu_mimetypes_t mth,
		  struct mimetypes_string *suffix,
		  struct mu_locus_range const *loc)
{
  struct node *node = make_node (mth, suffix_node, loc);
  node->v.suffix = *suffix;
  return node;
}

static struct node *
make_functional_node (mu_mimetypes_t mth,
		      char *ident, mu_list_t list,
		      struct mu_locus_range const *loc)
{
  size_t count, i;
  struct builtin_tab const *p;
  struct node *node;
  union argument *args;
  mu_iterator_t itr;
  int rc;
  
  p = mu_mimetypes_builtin (ident);
  if (!p)
    {
      yyerror (loc, NULL, NULL, _("unknown builtin: %s"), ident);
      return NULL;
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
  mu_iterator_destroy (&itr);
  node = make_node (mth, functional_node, loc);
  node->v.function.builtin = p;
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

static void
free_node (struct node *node)
{
  switch (node->type)
    {
    case functional_node:
      {
	char const *p;

	for (p = node->v.function.builtin->args; *p; p++)
	  {
	    switch (*p)
	      {
	      case 'd':
		break;
		
	      case 'x':
		regfree (&node->v.function.args[0].rx);
		break;
		
	      case 's':
		break;
	      }
	  }
	//	free (node->v.function.args);
      }
      break;

    case binary_node:
      free_node (node->v.bin.arg1);
      free_node (node->v.bin.arg2);
      break;

    case negation_node:
      free_node (node->v.arg);
      break;

    default:
      break;
    }

  mu_locus_range_deinit (&node->loc);
}

static void
rule_destroy_item (void *ptr)
{
  struct rule_tab *rt = ptr;
  mu_locus_range_deinit (&rt->loc);  
  free_node (rt->node);
}

  

