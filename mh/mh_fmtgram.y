%{
/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2002, 2004, 2007, 2009-2012, 2014-2017 Free
   Software Foundation, Inc.

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

#include <mh.h>
#include <mh_format.h>

int yyerror (const char *s);
int yylex (void);
 
static mu_opool_t tokpool;     /* Temporary token storage */


/* Lexical context */
enum context
  {
    ctx_init,   /* Normal text */
    ctx_if,     /* After %< or %? */
    ctx_expr,   /* Expression within cond */
    ctx_func,   /* after (func */
  };

static enum context ctx_stack[512];
size_t ctx_tos;

static inline void
ctx_push (enum context ctx)
{
  if (ctx_tos == MU_ARRAY_SIZE (ctx_stack))
    {
      yyerror ("context nesting level too deep");
      exit (1);
    }
  ctx_stack[ctx_tos++] = ctx;
}

static inline void
ctx_pop (void)
{
  if (ctx_tos == 0)
    {
      yyerror ("out of context");
      abort ();
    }
  ctx_tos--;
}

static inline enum context
ctx_get (void)
{
  return ctx_stack[ctx_tos-1];
}

enum node_type
{
  fmtnode_print,
  fmtnode_literal,
  fmtnode_number,
  fmtnode_body,
  fmtnode_comp,
  fmtnode_funcall,
  fmtnode_cntl,
  fmtnode_typecast
};

struct node
{
  enum node_type nodetype;
  enum mh_type datatype;
  int noprint:1;
  struct node *prev, *next;
  union
  {
    char *str;
    long num;
    struct node *arg;
    struct
    {
      int fmtspec;
      struct node *arg;
    } prt;
    struct
    {
      mh_builtin_t *builtin;
      struct node *arg;
    } funcall;
    struct
    {
      struct node *cond;
      struct node *iftrue;
      struct node *iffalse;
    } cntl;
  } v;
};

static struct node *parse_tree;
static struct node *new_node (enum node_type nodetype, enum mh_type datatype);

static struct node *printelim (struct node *root);
static void codegen (mh_format_t *fmt, int tree);
static struct node *typecast (struct node *node, enum mh_type type);
 
%}

%union {
  char *str;
  long num;
  struct {
    struct node *head, *tail;
  } nodelist;
  struct node *nodeptr;
  mh_builtin_t *builtin;
  int fmtspec;
  struct {
    enum mh_type type;
    union
    {
      char *str;
      long num;
    } v;
  } arg;
};

%token <num> NUMBER
%token <str> STRING COMPONENT
%token <arg> ARGUMENT
%token <builtin> FUNCTION
%token IF ELIF ELSE FI
%token <fmtspec> FMTSPEC
%token BOGUS
%token EOFN

%type <nodelist> list zlist elif_list
%type <nodeptr> item escape component funcall cntl argument
%type <nodeptr> cond cond_expr elif_part else_part printable
%type <builtin> function
%type <fmtspec> fmtspec

%%

input     : list
            {
	      parse_tree = $1.head;
	    }
          ;

list      : item
            {
	      $$.head = $$.tail = $1;
	    }
          | list item
	    {
	      $2->prev = $1.tail;
	      $1.tail->next = $2;
	      $1.tail = $2;
	      $$ = $1;
	    }
          ;

item      : STRING
            {
	      struct node *n = new_node (fmtnode_literal, mhtype_str);
	      n->v.str = $1;
	      $$ = new_node (fmtnode_print, mhtype_str);
	      $$->v.prt.arg = n;
	    }
          | escape
          ;

escape    : cntl
          | fmtspec printable
            {
	      if ($2->noprint)
		$$ = $2;
	      else
		{
		  $$ = new_node (fmtnode_print, $2->datatype);
		  $$->v.prt.fmtspec = $1;
		  $$->v.prt.arg = $2;
		}
	    }
          ;

printable : component
          | funcall
          ;

component : COMPONENT
            {
	      if (mu_c_strcasecmp ($1, "body") == 0)
		$$ = new_node (fmtnode_body, mhtype_str);
	      else
		{
		  $$ = new_node (fmtnode_comp, mhtype_str);
		  $$->v.str = $1;
		}
	    }
          ;

funcall   : function argument EOFN
            {
	      ctx_pop ();
	      if ($1->optarg == MHA_VOID) /*FIXME*/
		{
		  $2->noprint = 1;
		  $$ = $2;
		}
	      else
		{
		  if ($1->argtype == mhtype_none)
		    {
		      if ($2)
			{
			  yyerror ("function doesn't take arguments");
			  YYABORT;
			}
		    }
		  else if ($2 == NULL)
		    {
		      if ($1->optarg != MHA_OPTARG)
			{
			  yyerror ("required argument missing");
			  YYABORT;
			}
		    }
		  $$ = new_node (fmtnode_funcall, $1->type);
		  $$->v.funcall.builtin = $1;
		  $$->v.funcall.arg = typecast ($2, $1->argtype);
		  $$->noprint = $1->type == mhtype_none;
		}
	    }
          ;

fmtspec   : /* empty */
            {
	      $$ = 0;
	    }
          | FMTSPEC
          ;

function  : FUNCTION
            {
	      ctx_push (ctx_func);
	    }
          ;

argument  : /* empty */
            {
	      $$ = NULL;
	    }
          | ARGUMENT
            {
	      switch ($1.type)
		{
		case mhtype_none:
		  $$ = NULL;
		  break;
		  
		case mhtype_str:
		  $$ = new_node (fmtnode_literal, mhtype_str);
		  $$->v.str = $1.v.str;
		  break;

		case mhtype_num:
		  $$ = new_node (fmtnode_number, mhtype_num);
		  $$->v.num = $1.v.num;
		}
	    }
	  | escape
	    {
	      $$ = printelim ($1);
	    }
          ;

/*           1   2    3       4     5    */
cntl      : if cond zlist elif_part fi
            {
	      $$ = new_node(fmtnode_cntl, mhtype_num);
	      $$->v.cntl.cond = $2;
	      $$->v.cntl.iftrue = $3.head;
	      $$->v.cntl.iffalse = $4;
	    }
          ;

zlist     : /* empty */
            {
	      $$.head = $$.tail = NULL;
	    }
          | list
          ;

if        : IF
            {
	      ctx_push (ctx_if);
	    }
          ;

fi        : FI
            {
	      ctx_pop ();
	    }
          ;

elif      : ELIF
            {
	      ctx_pop ();
	      ctx_push (ctx_if);
	    }
          ;

cond      : cond_expr
            {
	      ctx_pop ();
	      ctx_push (ctx_expr);
	      $$ = printelim ($1);
	    }
          ;

cond_expr : component
          | funcall
          ;

elif_part : /* empty */
            {
	      $$ = NULL;
	    }
          | else_part
          | elif_list
	    {
	      $$ = $1.head;
	    }
          ;

elif_list : elif cond zlist
            {
	      struct node *np = new_node (fmtnode_cntl, mhtype_num);
	      np->v.cntl.cond = $2;
	      np->v.cntl.iftrue = $3.head;
	      np->v.cntl.iffalse = NULL;
	      $$.head = $$.tail = np;
	    }
          | elif_list elif cond zlist
	    {
	      struct node *np = new_node(fmtnode_cntl, mhtype_num);
	      np->v.cntl.cond = $3;
	      np->v.cntl.iftrue = $4.head;
	      np->v.cntl.iffalse = NULL;

	      $1.tail->v.cntl.iffalse = np;
	      $1.tail = np;

	      $$ = $1;
	    }
          | elif_list else_part
	    {
	      $1.tail->v.cntl.iffalse = $2;
	      $1.tail = $2;
	      $$ = $1;
	    }	      
          ;

else_part : ELSE list
	    {
	      $$ = $2.head;
	    }
	  ;

%%

static char *start;
static char *curp;

int
yyerror (const char *s)
{
  if (yychar != BOGUS)
    {
      int len;
      mu_error ("%s: %s", start, s);
      len = curp - start;
      mu_error ("%*.*s^", len, len, "");
    }
  return 0;
}

static int backslash(int c);

struct lexer_tab
{
  char *ctx_name;
  int (*lexer) (void);
};

static int yylex_initial (void);
static int yylex_cond (void);
static int yylex_expr (void);
static int yylex_func (void);

static struct lexer_tab lexer_tab[] = {
  [ctx_init] = { "initial",    yylex_initial },
  [ctx_if]   = { "condition",  yylex_cond },
  [ctx_expr] = { "expression", yylex_expr },
  [ctx_func] = { "function",   yylex_func }
};
  
int
yylex (void)
{  
  if (yydebug)
    fprintf (stderr, "lex: [%s] at %-10.10s...]\n",
	     lexer_tab[ctx_get ()].ctx_name, curp);
  return lexer_tab[ctx_get ()].lexer ();
}

static int
token_fmtspec (int flags)
{
  int num = 0;

  if (*curp == '0')
    {
      flags |= MH_FMT_ZEROPAD;
      curp++;
    }
  else if (!mu_isdigit (*curp))
    {
      yyerror ("expected digit");
      return BOGUS;
    }
  
  while (*curp && mu_isdigit (*curp))
    num = num * 10 + *curp++ - '0';
  yylval.fmtspec = flags | num;
  *--curp = '%'; /* FIXME: dirty hack */
  return FMTSPEC;
}

static int
token_function (void)
{
  char *start;
  
  curp++;
  start = curp;
  curp = mu_str_skip_class (start, MU_CTYPE_ALPHA);
  if (start == curp || !strchr (" \t(){%", *curp))
    {
      yyerror ("expected function name");
      return BOGUS;
    }

  yylval.builtin = mh_lookup_builtin (start, curp - start);

  if (!yylval.builtin)
    {
      yyerror ("unknown function");
      return BOGUS;
    }
  
  return FUNCTION;
}
  
static int
token_component (void)
{
  char *start;
  
  curp++;
  if (!mu_isalpha (*curp))
    {
      yyerror ("component name expected");
      return BOGUS;
    }
  start = curp;
  for (; *curp != '}'; curp++)
    {
      if (!(mu_isalnum (*curp) || *curp == '_' || *curp == '-'))
	{
	  yyerror ("component name expected");
	  return BOGUS;
	}
    }
  mu_opool_append (tokpool, start, curp - start);
  mu_opool_append_char (tokpool, 0);
  yylval.str = mu_opool_finish (tokpool, NULL);
  curp++;
  return COMPONENT;
}

int
yylex_initial (void)
{
  if (*curp == '%')
    {
      int c;
      curp++;

      switch (c = *curp++)
	{
	case '<':
	  return IF;
	case '%':
	  return '%';
	case '(':
	  curp--;
	  return token_function ();
	case '{':
	  curp--;
	  return token_component ();
	case '-':
	  return token_fmtspec (MH_FMT_RALIGN);
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  curp--;
	  return token_fmtspec (MH_FMT_DEFAULT);
	default:
	  yyerror ("component or function name expected");
	  return BOGUS;
      }
    }

  if (*curp == 0)
    return 0;

  do
    {
      if (*curp == '\\')
	{
	  int c = backslash (*++curp);
	  mu_opool_append_char (tokpool, c);
	}
      else
	mu_opool_append_char (tokpool, *curp);
      curp++;
    }
  while (*curp && *curp != '%');

  mu_opool_append_char (tokpool, 0);
  yylval.str = mu_opool_finish (tokpool, NULL);
  return STRING;
}  

int
yylex_cond (void)
{
  switch (*curp)
    {
    case '(':
      return token_function ();
    case '{':
      return token_component ();
    default:
      yyerror ("'(' or '{' expected");
      return BOGUS;
    }
}

int
yylex_expr (void)
{
  if (*curp == '%')
    {
      curp++;
      switch (*curp++)
	{
	case '?':
	  return ELIF;
	case '|':
	  return ELSE;
	case '>':
	  return FI;
	}
      curp -= 2;
    }
  return yylex_initial ();
}

int
yylex_func (void)
{
  /* Expected argument or closing parenthesis */
  while (*curp && mu_isspace (*curp))
    curp++;

  switch (*curp)
    {
    case '(':
      return token_function ();
      
    case ')':
      curp++;
      return EOFN;
      
    case '{':
      return token_component ();

    case '%':
      curp++;
      switch (*curp)
	{
	case '<':
	  curp++;
	  return IF;

	case '%':
	  break;

	default:
	  yyerror ("expected '%' or '<'");
	  return BOGUS;
	}
    }

  if (mu_isdigit (*curp))
    {
      yylval.arg.type = mhtype_num;
      yylval.arg.v.num = strtol (curp, &curp, 0);
    }
  else
    {
      do
	{
	  if (*curp == 0)
	    {
	      yyerror("expected ')'");
	      return BOGUS;
	    }
      
	  if (*curp == '\\')
	    {
	      int c = backslash (*++curp);
	      mu_opool_append_char (tokpool, c);
	    }
	  else
	    mu_opool_append_char (tokpool, *curp);
	  curp++;
	}
      while (*curp != ')');
      mu_opool_append_char (tokpool, 0);

      yylval.arg.type = mhtype_str;
      yylval.arg.v.str = mu_opool_finish (tokpool, NULL);
    }

  if (*curp != ')')
    {
      yyerror("expected ')'");
      return BOGUS;
    }
  
  return ARGUMENT;
}

void
mh_format_debug (int val)
{
  yydebug = val;
}

int
mh_format_parse (mh_format_t *fmtptr, char *format_str, int flags)
{
  int rc;
  char *p = getenv ("MHFORMAT_DEBUG");
  
  if (p || mu_debug_level_p (MU_DEBCAT_APP, MU_DEBUG_TRACE2))
    yydebug = 1;
  start = curp = format_str;
  mu_opool_create (&tokpool, MU_OPOOL_ENOMEMABRT);

  ctx_tos = 0;
  ctx_push (ctx_init);
  
  rc = yyparse ();
  if (rc == 0)
    codegen (fmtptr, flags & MH_FMT_PARSE_TREE);
  else
    mu_opool_destroy (&tokpool);

  parse_tree = NULL;
  tokpool = NULL;
  return rc;
}

int
backslash (int c)
{
  static char transtab[] = "b\bf\fn\nr\rt\t";
  char *p;
  
  for (p = transtab; *p; p += 2)
    {
      if (*p == c)
	return p[1];
    }
  return c;
}

static struct node *
new_node (enum node_type nodetype, enum mh_type datatype)
{
  struct node *np = mu_zalloc (sizeof *np);
  np->nodetype = nodetype;
  np->datatype = datatype;
  return np;
}

static void node_list_free (struct node *node);

static void
node_free (struct node *node)
{
  switch (node->nodetype)
    {
    case fmtnode_print:
      node_free (node->v.prt.arg);
      break;

    case fmtnode_literal:
      break;

    case fmtnode_number:
      break;

    case fmtnode_body:
      break;

    case fmtnode_comp:
      break;

    case fmtnode_funcall:
      node_free (node->v.funcall.arg);
      break;

    case fmtnode_cntl:
      node_list_free (node->v.cntl.cond);
      node_list_free (node->v.cntl.iftrue);
      node_list_free (node->v.cntl.iffalse);
      break;

    default:
      abort ();
    }
  free (node);
}

static void
node_list_free (struct node *node)
{
  while (node)
    {
      struct node *next = node->next;
      node_free (node);
      node = next;
    }
}

static struct node *
typecast (struct node *node, enum mh_type type)
{
  if (!node)
    /* FIXME: when passing optional argument, the caller must know the
       type of value returned by the previous expression */
    return node;
  if (node->datatype == type)
    return node;
  if (node->nodetype == fmtnode_cntl)
    {
      node->v.cntl.iftrue = typecast (node->v.cntl.iftrue, type);
      node->v.cntl.iffalse = typecast (node->v.cntl.iffalse, type);
      node->datatype = type;
    }
  else
    {
      struct node *np = new_node (fmtnode_typecast, type);
      np->v.arg = node;
      node = np;
    }
  return node;
}

#define INLINE -1

static inline void
indent (int level)
{
  printf ("%*.*s", 2*level, 2*level, "");
}

static inline void
delim (int level, char const *dstr)
{
  if (level == INLINE)
    printf ("%s", dstr);
  else
    {
      printf ("\n");
      indent (level);
    }
}

static void dump_statement (struct node *node, int level);

void
mh_print_fmtspec (int fmtspec)
{
  if (!(fmtspec & (MH_FMT_RALIGN|MH_FMT_ZEROPAD|MH_FMT_COMPWS)))
    printf ("NONE");
  else
    {
      if (!(fmtspec & MH_FMT_RALIGN))
	printf ("NO");
      printf ("RALIGN|");
      if (!(fmtspec & MH_FMT_ZEROPAD))
	printf ("NO");
      printf ("ZEROPAD|");
      if (!(fmtspec & MH_FMT_COMPWS))
	printf ("NO");
      printf ("COMPWS");
    }
}

static char *typename[] = { "NONE", "NUM", "STR" };

static void
dump_node_pretty (struct node *node, int level)
{  
  switch (node->nodetype)
    {
    case fmtnode_print:
      if (node->v.prt.fmtspec)
	{
	  printf ("FORMAT(");
	  mh_print_fmtspec (node->v.prt.fmtspec);
	  printf(", %d, ", node->v.prt.fmtspec & MH_WIDTH_MASK);
	}
      else
	printf ("PRINT(%d,", node->v.prt.fmtspec);
      dump_statement (node->v.prt.arg, INLINE);
      printf (")");
      break;
      
    case fmtnode_literal:
      {
	char const *p = node->v.str;
	putchar ('"');
	while (*p)
	  {
	    if (*p == '\\' || *p == '"')
	      {
		putchar ('\\');
		putchar (*p);
	      }
	    else if (*p == '\n')
	      {
		putchar ('\\');
		putchar ('n');
	      }
	    else
	      putchar (*p);
	    p++;
	  }
	putchar ('"');
      }	
      break;
      
    case fmtnode_number:
      printf ("%ld", node->v.num);
      break;
      
    case fmtnode_body:
      printf ("BODY");
      break;
      
    case fmtnode_comp:
      printf ("COMPONENT.%s", node->v.str);
      break;
      
    case fmtnode_funcall:
      printf ("%s(", node->v.funcall.builtin->name);
      dump_statement (node->v.funcall.arg, INLINE);
      printf (")");
      break;
      
    case fmtnode_cntl:
      printf ("IF (");
      dump_node_pretty (node->v.cntl.cond, INLINE);
      printf (") THEN");

      if (level != INLINE)
	level++;
      
      delim (level, "; ");

      dump_statement (node->v.cntl.iftrue, level);

      if (node->v.cntl.iffalse)
	{
	  delim (level == INLINE ? level : level - 1, "; ");
	  printf ("ELSE");
	  delim (level, " ");
	  dump_statement (node->v.cntl.iffalse, level);
	}

      if (level != INLINE)
	level--;
      delim (level, "; ");
      printf ("FI");
      break;

    case fmtnode_typecast:
      printf ("%s(", typename[node->datatype]);
      dump_node_pretty (node->v.arg, INLINE);
      printf (")");
      break;

    default:
      abort ();
    }
}

static void
dump_statement (struct node *node, int level)
{
  while (node)
    {
      dump_node_pretty (node, level);
      node = node->next;
      if (node)
	delim (level, "; ");
    }
}

void
mh_format_dump_code (mh_format_t fmt)
{
  dump_statement (fmt->tree, 0);
  printf ("\n");
}

void
mh_format_free_tree (mh_format_t fmt)
{
  if (fmt)
    {
      node_list_free (fmt->tree);
      fmt->tree = NULL;
      mu_opool_destroy (&fmt->pool);
    }
}

void
mh_format_free (mh_format_t fmt)
{
  if (!fmt)
    return;
  
  mh_format_free_tree (fmt);
    
  if (fmt->prog)
    free (fmt->prog);
  fmt->progmax = fmt->progcnt = 0;
  fmt->prog = NULL;
}

void
mh_format_destroy (mh_format_t *fmt)
{
  if (fmt)
    {
      mh_format_free (*fmt);
      *fmt = NULL;
    }
}

static struct node *
printelim (struct node *node)
{
  if (node->nodetype == fmtnode_print)
    {
      struct node *arg = node->v.prt.arg;
      arg->next = node->next;
      free (node);
      node = arg;
    }
  return node;
}

#define PROG_MIN_ALLOC 8

static inline void
ensure_space (struct mh_format *fmt, size_t n)
{
  while (fmt->progcnt + n >= fmt->progmax)
    {
      if (fmt->progmax == 0)
	fmt->progmax = n < PROG_MIN_ALLOC ? PROG_MIN_ALLOC : n;
      fmt->prog = mu_2nrealloc (fmt->prog, &fmt->progmax, sizeof fmt->prog[0]);
    }
}
  
static void
emit_instr (struct mh_format *fmt, mh_instr_t instr)
{
  ensure_space (fmt, 1);
  fmt->prog[fmt->progcnt++] = instr;
}

static inline void
emit_opcode (struct mh_format *fmt, mh_opcode_t op)
{
  emit_instr (fmt, (mh_instr_t) op);
}

static void
emit_string (struct mh_format *fmt, char const *str)
{
  size_t length = strlen (str) + 1;
  size_t count = (length + sizeof (mh_instr_t)) / sizeof (mh_instr_t) + 1;
  
  ensure_space (fmt, count);
  emit_instr (fmt, (mh_instr_t) count);
  memcpy (MHI_STR (fmt->prog[fmt->progcnt]), str, length);
  fmt->progcnt += count;
}

static void codegen_node (struct mh_format *fmt, struct node *node);
static void codegen_nodelist (struct mh_format *fmt, struct node *node);

static void
emit_opcode_typed (struct mh_format *fmt, enum mh_type type,
		   enum mh_opcode opnum, enum mh_opcode opstr)
{
  switch (type)
    {
    case mhtype_num:
      emit_opcode (fmt, opnum);
      break;

    case mhtype_str:
      emit_opcode (fmt, opstr);
      break;

    default:
      abort ();
    }
}

static void
emit_funcall (struct mh_format *fmt, mh_builtin_t *builtin, struct node *arg)
{
  if (arg)
    {
      codegen_node (fmt, arg);
      emit_opcode_typed (fmt, arg->datatype, mhop_movn, mhop_movs);
    }
  else if (builtin->argtype != mhtype_none)
    emit_opcode_typed (fmt, builtin->type, mhop_movn, mhop_movs);

  emit_instr (fmt, (mh_instr_t) (long) R_ARG);
  emit_instr (fmt, (mh_instr_t) (long) R_REG);
  
  emit_opcode (fmt, mhop_call);
  emit_instr (fmt, (mh_instr_t) builtin->fun);
}

static void
codegen_node (struct mh_format *fmt, struct node *node)
{
  switch (node->nodetype)
    {
    case fmtnode_print:
      codegen_node (fmt, node->v.prt.arg);
      if (node->v.prt.fmtspec)
	{
	  emit_opcode (fmt, mhop_fmtspec);
	  emit_instr (fmt, (mh_instr_t) (long) node->v.prt.fmtspec);
	}

      if (node->v.prt.arg->datatype != mhtype_none)
	emit_opcode_typed (fmt, node->v.prt.arg->datatype,
			   mhop_printn, mhop_prints);
      break;

    case fmtnode_literal:
      emit_opcode (fmt, mhop_sets);
      emit_instr (fmt, (mh_instr_t) (long) R_REG);
      emit_string (fmt, node->v.str);
      break;

    case fmtnode_number:
      emit_opcode (fmt, mhop_setn);
      emit_instr (fmt, (mh_instr_t) (long) R_REG);
      break;

    case fmtnode_body:
      emit_opcode (fmt, mhop_ldbody);
      emit_instr (fmt, (mh_instr_t) (long) R_REG);
      break;

    case fmtnode_comp:
      emit_opcode (fmt, mhop_ldcomp);
      emit_instr (fmt, (mh_instr_t) (long) R_REG);
      emit_string (fmt, node->v.str);
      break;

    case fmtnode_funcall:
      emit_funcall (fmt, node->v.funcall.builtin, node->v.funcall.arg);
      break;

    case fmtnode_cntl:
      {
	long pc[2];
	
	codegen_node (fmt, node->v.cntl.cond);
	emit_opcode_typed (fmt, node->v.cntl.cond->datatype,
			   mhop_brzn, mhop_brzs);
	pc[0] = fmt->progcnt;
	emit_instr (fmt, (mh_instr_t) NULL);
	if (node->v.cntl.iftrue)
	  {
	    codegen_nodelist (fmt, node->v.cntl.iftrue);
	  }
	emit_opcode (fmt, mhop_branch);
	pc[1] = fmt->progcnt;
	emit_instr (fmt, (mh_instr_t) NULL);
	
	fmt->prog[pc[0]].num = fmt->progcnt - pc[0];
	if (node->v.cntl.iffalse)
	  {
	    codegen_nodelist (fmt, node->v.cntl.iffalse);
	  }
	fmt->prog[pc[1]].num = fmt->progcnt - pc[1];
      }
      break;

    case fmtnode_typecast:
      codegen_node (fmt, node->v.arg);
      switch (node->datatype)
	{
	case mhtype_num:
	  emit_opcode (fmt, mhop_itoa);
	  break;

	case mhtype_str:
	  emit_opcode (fmt, mhop_atoi);
	  break;

	default:
	  abort ();
	}
      break;
      
    default:
      abort ();
    }
}

static void
codegen_nodelist (struct mh_format *fmt, struct node *node)
{
  while (node)
    {
      codegen_node (fmt, node);
      node = node->next;
    }
}
	
static void
codegen (mh_format_t *fmtptr, int tree)
{
  struct mh_format *fmt;

  fmt = mu_zalloc (sizeof *fmt);

  *fmtptr = fmt;
  emit_opcode (fmt, mhop_stop);
  codegen_nodelist (fmt, parse_tree);
  emit_opcode (fmt, mhop_stop);

  if (tree)
    {
      fmt->tree = parse_tree;
      fmt->pool = tokpool;
    }
  else
    {
      node_list_free (parse_tree);
      mu_opool_destroy (&tokpool);
    }
}


  

