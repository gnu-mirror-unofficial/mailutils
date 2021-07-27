/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2021 Free Software Foundation, Inc.

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

#include "mail.h"
#include "mailutils/kwd.h"

#define MAILVAR_ALIAS  0x0001
#define MAILVAR_RDONLY 0x0002
#define MAILVAR_HIDDEN 0x0004

#define MAILVAR_TYPEMASK(type) (1<<(8+(type)))

enum mailvar_cmd
  {
    mailvar_cmd_set,
    mailvar_cmd_unset
  };

struct mailvar_symbol
{
  struct mailvar_variable var;
  int flags;
  char *descr;
  int (*handler) (enum mailvar_cmd, struct mailvar_variable *);
};

mu_list_t mailvar_list = NULL;

static int set_decode_fallback (enum mailvar_cmd cmd,
				struct mailvar_variable *);
static int set_replyregex (enum mailvar_cmd cmd,
			   struct mailvar_variable *);
static int set_screen (enum mailvar_cmd cmd,
		       struct mailvar_variable *);
static int set_verbose (enum mailvar_cmd cmd,
			struct mailvar_variable *);
static int set_debug (enum mailvar_cmd cmd,
		      struct mailvar_variable *);
static int set_folder (enum mailvar_cmd cmd,
		       struct mailvar_variable *);
static int set_headline (enum mailvar_cmd,
			 struct mailvar_variable *);
static int set_outfilename (enum mailvar_cmd,
			    struct mailvar_variable *);
static int set_escape (enum mailvar_cmd,
		       struct mailvar_variable *);

struct mailvar_symbol mailvar_tab[] =
  {
    /* FIXME: */
    { { mailvar_name_allnet, }, MAILVAR_HIDDEN },

    /* For compatibility with other mailx implementations.
       Never used, always true. */
    { { mailvar_name_append, },
      MAILVAR_TYPEMASK (mailvar_type_boolean) | MAILVAR_RDONLY,
      /* TRANSLATORS: "mbox" is the name of a command. Don't translate it. */
      N_("messages saved in mbox are appended to the end rather than prepended") },
    { { mailvar_name_appenddeadletter, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      /* TRANSLATORS: Don't translate "dead.letter". */
      N_("append the contents of canceled letter to dead.letter file") },
    { { mailvar_name_askbcc, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("prompt user for bcc before composing the message") },
    { { mailvar_name_askcc, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("prompt user for cc before composing the message") },
    { { mailvar_name_ask, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("prompt user for subject before composing the message") },
    { { mailvar_name_asksub, }, MAILVAR_ALIAS, NULL },
    { { mailvar_name_autoinc, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("automatically incorporate newly arrived messages")},
    { { mailvar_name_autoprint, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      /* TRANSLATORS: "delete" and "dp" are command names. */
      N_("delete command behaves like dp") },
    { { mailvar_name_bang, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("replace every occurrence of ! in arguments to the shell command"
	 " with the last executed command") },
    { { mailvar_name_charset, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("output character set for decoded header fields") },
    { { mailvar_name_cmd, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      /* TRANSLATORS: "pipe" is the command name. */
      N_("default shell command for pipe") },
    { { mailvar_name_columns, },
      MAILVAR_TYPEMASK (mailvar_type_number),
      N_("number of columns on terminal screen") },
    { { mailvar_name_crt, },
      MAILVAR_TYPEMASK (mailvar_type_number) |
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("if numeric, sets the minimum number of output lines needed "
	 "to engage paging; if boolean, use the height of the terminal "
	 "screen to compute the threshold") },
    { { mailvar_name_datefield, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("get date from the `Date:' header, instead of the envelope") },
    { { mailvar_name_debug, },
      MAILVAR_TYPEMASK (mailvar_type_string) |
	MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("set Mailutils debug level"),
      set_debug },
    { { mailvar_name_decode_fallback, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("how to represent characters that cannot be rendered using the "
	 "current character set"),
      set_decode_fallback },
    { { mailvar_name_dot, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("input message is terminated with a dot alone on a line") },
    { { mailvar_name_editheaders, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("allow editing message headers while composing") },
    { { mailvar_name_emptystart, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("start interactive mode if the mailbox is empty") },
    { { mailvar_name_escape, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("command escape character"),
      set_escape },
    { { mailvar_name_flipr, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("swap the meaning of reply and Reply commands") },
    { { mailvar_name_folder, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("folder directory name"),
      set_folder },
    { { mailvar_name_fromfield, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("get sender address from the `From:' header, instead of "
	 "the envelope") },
    { { mailvar_name_gnu_last_command, },
      MAILVAR_TYPEMASK (mailvar_type_string) | MAILVAR_RDONLY,
      N_("last executed command line") },
    { { mailvar_name_header, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("run the `headers' command after entering interactive mode") },
    { { mailvar_name_headline, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("format string to use for the header summary"),
      set_headline },
    { { mailvar_name_hold, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("hold the read or saved messages in the system mailbox") },
    { { mailvar_name_ignore, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("ignore keyboard interrupts when composing messages") },
    { { mailvar_name_ignoreeof, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("ignore EOF character") },
    { { mailvar_name_indentprefix, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("string used by the ~m escape for indenting quoted messages") },
    { { mailvar_name_inplacealiases, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("expand aliases in the address header field "
	 "before starting composing the message") },
    /* For compatibility with other mailx implementations.
       Never used, always true. */
    { { mailvar_name_keep, },
      MAILVAR_TYPEMASK (mailvar_type_boolean) | MAILVAR_RDONLY,
      N_("keep the empty user's system mailbox,"
	 " instead of removing it") },
    { { mailvar_name_keepsave, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("keep saved messages in system mailbox too") },
    { { mailvar_name_mailx, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("enable mailx compatibility mode") },
    { { mailvar_name_metamail, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("interpret the content of message parts; if set to a string "
	 "specifies the name of the external metamail command") },
    { { mailvar_name_metoo, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("do not remove sender addresses from the recipient list") },
    { { mailvar_name_mimenoask, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("a comma-separated list of MIME types for which "
	 "no confirmation is needed before running metamail interpreter") },
    { { mailvar_name_mode, },
      MAILVAR_TYPEMASK (mailvar_type_string) | MAILVAR_RDONLY,
      N_("the name of current operation mode") },
    { { mailvar_name_nullbody, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("accept messages with an empty body") },
    { { mailvar_name_nullbodymsg, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("display this text when sending a message with empty body") },
    { { mailvar_name_outfolder, },
      MAILVAR_TYPEMASK (mailvar_type_string) |
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("If boolean, causes the files used to record outgoing messages to"
	 " be located in the directory specified by the folder variable"
	 " (unless the pathname is absolute).\n"
	 "If string, names the directory where to store these files.") },
    { { mailvar_name_page, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("pipe command terminates each message with a formfeed") },
    { { mailvar_name_prompt, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("command prompt sequence") },
    { { mailvar_name_quit, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("keyboard interrupts terminate the program") },
    { { mailvar_name_rc, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("read the system-wide configuration file upon startup") },
    { { mailvar_name_readonly, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("mailboxes are opened in readonly mode") },
    { { mailvar_name_record, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("save outgoing messages in this file") },
    { { mailvar_name_recursivealiases, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("recursively expand aliases") },
    { { mailvar_name_regex, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("use of regular expressions in search message specifications") },
    { { mailvar_name_replyprefix, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("prefix for the subject line of a reply message") },
    { { mailvar_name_replyregex, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("regexp for recognizing subject lines of reply messages"),
      set_replyregex },
    { { mailvar_name_return_address },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("return address for outgoing messages") },
    { { mailvar_name_save, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("store aborted messages in the user's dead.file") },
    { { mailvar_name_screen, },
      MAILVAR_TYPEMASK (mailvar_type_number),
      N_("number of lines on terminal screen"),
      set_screen },
    { { mailvar_name_sendmail, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("URL of the mail transport agent") },
    /* FIXME: Not yet used. */
    { { mailvar_name_sendwait, },
      MAILVAR_TYPEMASK (mailvar_type_boolean) | MAILVAR_HIDDEN, NULL },
    { { mailvar_name_sign, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("signature for use with the ~a command") },
    { { mailvar_name_Sign, },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("name of the signature file for use with the ~A command") },
    { { mailvar_name_showenvelope, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("`print' command includes the SMTP envelope in its output") },
    { { mailvar_name_showto, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("if the message was sent by me, print its recipient address "
	 "in the header summary") },
    { { mailvar_name_toplines, },
      MAILVAR_TYPEMASK (mailvar_type_number),
      N_("number of lines to be displayed by `top' or `Top'") },
    { { mailvar_name_variable_pretty_print, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("print variables with short descriptions") },
    { { mailvar_name_varpp, }, MAILVAR_ALIAS },
    { { mailvar_name_variable_strict, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("perform strict checking when setting variables") },
    { { mailvar_name_varstrict, }, MAILVAR_ALIAS },
    { { mailvar_name_verbose, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("verbosely trace the process of message delivery"),
      set_verbose },

    { { mailvar_name_useragent, },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("add the `User-Agent' header to the outgoing messages") },
    { { mailvar_name_xmailer, }, MAILVAR_ALIAS },

    { { mailvar_name_mime },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("always compose MIME messages") },

    { { mailvar_name_fullnames },
      MAILVAR_TYPEMASK (mailvar_type_boolean),
      N_("when replying, preserve personal parts of recipient emails") },

    { { mailvar_name_outfilename },
      MAILVAR_TYPEMASK (mailvar_type_string),
      N_("how to create outgoing file name: local, domain, email"),
      set_outfilename },

    { { mailvar_name_PID },
      MAILVAR_TYPEMASK (mailvar_type_string) | MAILVAR_RDONLY,
      N_("PID of this process") },

    /* These will be implemented later */
    { { mailvar_name_onehop, }, MAILVAR_HIDDEN, NULL },

    { { mailvar_name_quiet, },
      MAILVAR_TYPEMASK (mailvar_type_boolean) | MAILVAR_HIDDEN,
      N_("suppress the printing of the version when first invoked") },

    { { NULL }, }
  };

static int mailvar_symbol_count =
  sizeof (mailvar_tab) / sizeof (mailvar_tab[0]) - 1;

struct mailvar_symbol *
find_mailvar_symbol (const char *var)
{
  struct mailvar_symbol *ep;
  for (ep = mailvar_tab; ep->var.name; ep++)
    if (strcmp (ep->var.name, var) == 0)
      {
	while ((ep->flags & MAILVAR_ALIAS) && ep > mailvar_tab)
	  ep--;
	return ep;
      }
  return NULL;
}

static void
print_descr (mu_stream_t out, const char *s, int n,
	     int doc_col, int rmargin, char *pfx)
{
  mu_stream_stat_buffer stat;

  if (!s)
    return;

  mu_stream_set_stat (out, MU_STREAM_STAT_MASK (MU_STREAM_STAT_OUT), stat);
  stat[MU_STREAM_STAT_OUT] = n;
  do
    {
      const char *p;
      const char *space = NULL;

      if (stat[MU_STREAM_STAT_OUT] && pfx)
	mu_stream_printf (out, "%s", pfx);

      while (stat[MU_STREAM_STAT_OUT] < doc_col)
	mu_stream_write (out, " ", 1, NULL);

      for (p = s; *p && p < s + (rmargin - doc_col); p++)
	if (*p == '\n')
	  {
	    space = p;
	    break;
	  }
	else if (mu_isspace (*p))
	  space = p;

      if (!space || (*space != '\n' && p < s + (rmargin - doc_col)))
	{
	  mu_stream_printf (out, "%s", s);
	  s += strlen (s);
	}
      else
	{
	  for (; s < space; s++)
	    mu_stream_write (out, s, 1, NULL);
	  for (; *s && mu_isspace (*s); s++)
	    ;
	}
      mu_stream_printf (out, "\n");
      stat[MU_STREAM_STAT_OUT] = 1;
    }
  while (*s);
  mu_stream_set_stat (out, 0, NULL);
}

/* Functions for dealing with internal mailvar_list variables */
static int
mailvar_variable_comp (const void *a, const void *b)
{
  const struct mailvar_variable *v1 = a;
  const struct mailvar_variable *v2 = b;

  return strcmp (v1->name, v2->name);
}

/* Find mailvar_list entry VAR. If not found and CREATE is not NULL, then
   create the (unset and untyped) variable */
struct mailvar_variable *
mailvar_find_variable (const char *name, int create)
{
  struct mailvar_symbol *sym;
  struct mailvar_variable *var;

  sym = find_mailvar_symbol (name);
  if (sym)
    var = &sym->var;
  else
    {
      struct mailvar_variable entry, *p;

      entry.name = (char*)name;
      /* Store it into the list */
      if (mailvar_list == NULL)
	{
	  mu_list_create (&mailvar_list);
	  mu_list_set_comparator (mailvar_list, mailvar_variable_comp);
	}

      if (mu_list_locate (mailvar_list, &entry, (void**)&p))
	{
	  if (!create)
	    return 0;
	  else
	    {
	      p = mu_alloc (sizeof *p);
	      p->name = mu_strdup (name);
	      mu_list_prepend (mailvar_list, p);
	    }
	}
      var = p;
      var->set = 0;
      var->type = mailvar_type_whatever;
      var->value.number = 0;
    }

  return var;
}


/* Retrieve the value of a specified variable of given type.
   The value is stored in the location pointed to by PTR variable.
   VARIABLE and TYPE specify the variable name and type. If the
   variable is not found and WARN is not null, the warning message
   is issued.

   Return value is 0 if the variable is found, 1 otherwise.
   If PTR is not NULL, it must point to

   int           if TYPE is mailvar_type_number or mailvar_type_boolean
   const char *  if TYPE is mailvar_type_string.

   Passing PTR=NULL may be used to check whether the variable is set
   without retrieving its value. */

int
mailvar_get (void *ptr, const char *variable, enum mailvar_type type, int warn)
{
  struct mailvar_variable *var = mailvar_find_variable (variable, 0);

  if (!var->set || (type != mailvar_type_whatever && var->type != type))
    {
      if (warn)
	mu_error (_("No value set for \"%s\""), variable);
      return 1;
    }
  if (ptr)
    switch (type)
      {
      case mailvar_type_string:
	*(char**)ptr = var->value.string;
	break;

      case mailvar_type_number:
	*(int*)ptr = var->value.number;
	break;

      case mailvar_type_boolean:
	*(int*)ptr = var->value.bool;
	break;

      default:
	break;
      }

  return 0;
}

int
mailvar_is_true (char const *name)
{
  return mailvar_get (NULL, name, mailvar_type_boolean, 0) == 0;
}

/* Initialize mailvar_list entry: clear set indicator and free any memory
   associated with the data */
void
mailvar_variable_reset (struct mailvar_variable *var)
{
  if (!var->set)
    return;

  switch (var->type)
    {
    case mailvar_type_string:
      free (var->value.string);
      var->value.string = NULL;
      break;

    default:
      break;
    }
  var->set = 0;
}

/* Set environement
   The  mailvar_set() function adds to the mailvar_list the VARIABLE
   with the given VALUE, if VARIABLE does not already exist.
   If it does exist in the mailvar_list, then its value is changed
   to VALUE if MOPTF_OVERWRITE bit is set in FLAGS, otherwise the
   value is not changed.

   Unless MOPTF_QUIET bit is set in FLAGS, the function performs
   semantic check, using the builtin options table.

   If VALUE is null the VARIABLE is unset. */
int
mailvar_set (const char *variable, void *value, enum mailvar_type type,
	     int flags)
{
  struct mailvar_variable *var, newvar;
  const struct mailvar_symbol *sym = find_mailvar_symbol (variable);
  enum mailvar_cmd cmd =
    (flags & MOPTF_UNSET) ? mailvar_cmd_unset : mailvar_cmd_set;

  if (!(flags & MOPTF_QUIET) && mailvar_is_true (mailvar_name_variable_strict))
    {
      if (!sym)
	mu_diag_output (MU_DIAG_WARNING, _("setting unknown variable %s"),
			variable);
      else if (sym->flags & MAILVAR_RDONLY)
	{
	  mu_error (cmd == mailvar_cmd_set
		     ? _("Cannot set read-only variable %s")
		     : _("Cannot unset read-only variable %s"),
		    variable);
	  return 1;
	}
      else if (!(sym->flags & MAILVAR_TYPEMASK (type))
	       && cmd == mailvar_cmd_set)
	{
	  mu_error (_("Wrong type for %s"), variable);
	  return 1;
	}
    }

  var = mailvar_find_variable (variable, cmd == mailvar_cmd_set);

  if (!var || (var->set && !(flags & MOPTF_OVERWRITE)))
    return 0;

  newvar.name = var->name;
  newvar.type = var->type;
  newvar.set = 0;
  memset (&newvar.value, 0, sizeof (newvar.value));

  switch (cmd)
    {
    case mailvar_cmd_set:
      if (value)
	{
	  switch (type)
	    {
	    case mailvar_type_number:
	      newvar.value.number = *(int*)value;
	      break;

	    case mailvar_type_string:
	      {
		char *p = strdup (value);
		if (!p)
		  {
		    mu_error ("%s", _("Not enough memory"));
		    return 1;
		  }
		newvar.value.string = p;
	      }
	      break;

	    case mailvar_type_boolean:
	      newvar.value.bool = *(int*)value;
	      break;

	    default:
	      abort();
	    }
	  newvar.set = 1;
	}
      newvar.type = type;
      if (sym
	  && sym->handler
	  && sym->flags & MAILVAR_TYPEMASK (type)
	  && sym->handler (cmd, &newvar))
	{
	  mailvar_variable_reset (&newvar);
	  return 1;
	}
      mailvar_variable_reset (var);
      *var = newvar;
      break;

    case mailvar_cmd_unset:
      if (sym
	  && sym->handler
	  && sym->handler (cmd, var))
	return 1;
      mailvar_variable_reset (var);
    }

  return 0;
}

static int
set_folder (enum mailvar_cmd cmd, struct mailvar_variable *var)
{
  int rc;

  if (var->value.string)
    {
      char *p = mu_tilde_expansion (var->value.string, MU_HIERARCHY_DELIMITER,
				    NULL);
      if (!p)
	mu_alloc_die ();
      if (var->set)
	free (var->value.string);
      var->value.string = p;
    }

  rc = mu_set_folder_directory (var->value.string);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_set_folder_directory",
		     var->value.string, rc);
  return rc;
}


static int
set_headline (enum mailvar_cmd cmd, struct mailvar_variable *var)
{
  if (cmd == mailvar_cmd_unset)
    return 1;

  mail_compile_headline (var->value.string);
  return 0;
}

static int
set_decode_fallback (enum mailvar_cmd cmd, struct mailvar_variable *var)
{
  char *value;
  int rc;

  switch (cmd)
    {
    case mailvar_cmd_set:
      value = var->value.string;
      break;

    case mailvar_cmd_unset:
      value = "none";
    }

  rc = mu_set_default_fallback (value);
  if (rc)
    mu_error (_("Incorrect value for decode-fallback"));
  return rc;
}

static int
set_replyregex (enum mailvar_cmd cmd, struct mailvar_variable *var)
{
  int rc;
  char *err;

  switch (cmd)
    {
    case mailvar_cmd_set:
      if ((rc = mu_unre_set_regex (var->value.string, 0, &err)))
	{
	  if (err)
	    mu_error ("%s: %s", mu_strerror (rc), err);
	  else
	    mu_error ("%s", mu_strerror (rc));
	  return 1;
	}
      break;

    case mailvar_cmd_unset:
      return 1;
    }

  return 0;
}

static int
set_screen (enum mailvar_cmd cmd, struct mailvar_variable *var)
{
  if (cmd == mailvar_cmd_set)
    page_invalidate (1);
  return 0;
}

#define DEFAULT_DEBUG_LEVEL  MU_DEBUG_LEVEL_UPTO (MU_DEBUG_TRACE7)

static int
set_verbose (enum mailvar_cmd cmd, struct mailvar_variable *var)
{
  switch (cmd)
    {
    case mailvar_cmd_set:
      mu_debug_set_category_level (MU_DEBCAT_APP, DEFAULT_DEBUG_LEVEL);
      break;

    case mailvar_cmd_unset:
      mu_debug_set_category_level (MU_DEBCAT_APP, 0);
    }
  return 0;
}

static int
set_debug (enum mailvar_cmd cmd, struct mailvar_variable *var)
{
  mu_debug_clear_all ();

  if (cmd == mailvar_cmd_set)
    {
      if (var->type == mailvar_type_boolean)
	{
	  if (var->set)
	    mu_debug_set_category_level (MU_DEBCAT_ALL, DEFAULT_DEBUG_LEVEL);
	  return 0;
	}
      mu_debug_parse_spec (var->value.string);
    }
  return 0;
}

static int
set_outfilename (enum mailvar_cmd cmd, struct mailvar_variable *var)
{
  static struct mu_kwd kwtab[] = {
    { "local", outfilename_local },
    { "email", outfilename_email },
    { "domain", outfilename_domain },
    { NULL }
  };
  
  switch (cmd)
    {
    case mailvar_cmd_set:
      if (mu_kwd_xlat_name (kwtab, var->value.string, &outfilename_mode))
	{
	  mu_error (_("unsupported value for %s"), var->name);
	  return 1;
	}
      break;
      
    default:
      return 1;
    }
  return 0;
}

static int
set_escape (enum mailvar_cmd cmd, struct mailvar_variable *var)
{
  if (cmd == mailvar_cmd_set)
    {
      if (var->value.string[0] == 0)
	{
	  mu_error (_("escape character cannot be empty"));
	  return 1;
	}
      else if (var->value.string[1] != 0)
	{
	  mu_error (_("escape value must be a single character"));
	  return 1;
	}
    }
  return 0;
}

size_t
_mailvar_symbol_count (int set)
{
  if (!set)
    return mailvar_symbol_count;
  else
    {
      struct mailvar_symbol *s;
      size_t count = 0;
      for (s = mailvar_tab; s->var.name; s++)
	if (s->var.set)
	  count++;
      return count;
    }
}

static int
mailvar_mapper (void **itmv, size_t itmc, void *call_data)
{
  return MU_LIST_MAP_OK;
}

int
_mailvar_symbol_to_list (int set, mu_list_t list)
{
  struct mailvar_symbol *s;
  for (s = mailvar_tab; s->var.name; s++)
    if (!set || s->var.set)
      mu_list_append (list, &s->var);
  return 0;
}

mu_list_t
mailvar_list_copy (int set)
{
  mu_list_t list = NULL;

  if (mailvar_list)
    mu_list_map (mailvar_list, mailvar_mapper, NULL, 1, &list);
  if (!list)
    mu_list_create (&list);
  _mailvar_symbol_to_list (set, list);
  mu_list_sort (list, mailvar_variable_comp);
  return list;
}


enum
  {
    MAILVAR_ITR_ALL = 0,         /* Return all variables */
    MAILVAR_ITR_SET = 0x1,       /* Return only variables that have been set */
    MAILVAR_ITR_WRITABLE = 0x2   /* Return only writable variables */
  };

struct mailvar_iterator
{
  int flags;                /* MAILVAR_ITR_ bits */
  const char *prefix;       /* Prefix to match */
  int prefixlen;            /* Length of the prefix */
  mu_list_t varlist;        /* List of collected variables */
  mu_iterator_t varitr;     /* Iterator over varlist */
};

/* Return next match from ITR */
const char *
mailvar_iterate_next (struct mailvar_iterator *itr)
{
  struct mailvar_variable *vp;

  while (!mu_iterator_is_done (itr->varitr))
    {
      size_t len;

      mu_iterator_current (itr->varitr, (void**) &vp);
      mu_iterator_next (itr->varitr);

      if (itr->flags & MAILVAR_ITR_WRITABLE)
	{
	  const struct mailvar_symbol *sym = find_mailvar_symbol (vp->name);
	  if (sym && (sym->flags & MAILVAR_RDONLY))
	    continue;
	}

      if (strlen (vp->name) >= itr->prefixlen
	  && strncmp (vp->name, itr->prefix, itr->prefixlen) == 0)
	return strdup (vp->name);

      /* See if it's a negated boolean */
      if (itr->prefixlen >= 2 && memcmp (itr->prefix, "no", 2) == 0
	  && (len = strlen (vp->name)) >= itr->prefixlen - 2
	  && memcmp (vp->name, itr->prefix + 2, itr->prefixlen - 2) == 0)
	{
	  char *p;
	  struct mailvar_symbol *sym = find_mailvar_symbol (vp->name);
	  if (sym && !(sym->flags & (MAILVAR_TYPEMASK (mailvar_type_boolean))))
	    continue;
	  p = malloc (len + 3);
	  if (p)
	    {
	      strcpy (p, "no");
	      strcpy (p + 2, vp->name);
	    }
	  return p;
	}
    }
  return NULL;
}

/* Initialize iterator, return the name of the first match */
const char *
mailvar_iterate_first (int flags,
		       const char *prefix, struct mailvar_iterator **pitr)
{
  struct mailvar_iterator *itr = mu_alloc (sizeof *itr);
  itr->flags = flags;
  itr->prefix = prefix;
  itr->prefixlen = strlen (prefix);
  itr->varlist = mailvar_list_copy (flags & MAILVAR_ITR_SET);
  mu_list_get_iterator (itr->varlist, &itr->varitr);
  mu_iterator_first (itr->varitr);
  *pitr = itr;
  return mailvar_iterate_next (itr);
}

/* Release memory used by ITR */
void
mailvar_iterate_end (struct mailvar_iterator **pitr)
{
  if (pitr && *pitr)
    {
      struct mailvar_iterator *itr = *pitr;
      mu_iterator_destroy (&itr->varitr);
      mu_list_destroy (&itr->varlist);
      free (itr);
      *pitr = NULL;
    }
}

struct mailvar_print_closure
{
  int prettyprint;
  mu_stream_t out;
  int width;
};

static int
mailvar_printer (void *item, void *data)
{
  struct mailvar_variable *vp = item;
  struct mailvar_print_closure *clos = data;

  if (clos->prettyprint)
    {
      const struct mailvar_symbol *sym = find_mailvar_symbol (vp->name);

      if (sym)
	{
	  if (sym->flags & MAILVAR_HIDDEN)
	    return 0;
	  if (sym->flags & MAILVAR_RDONLY)
	    mu_stream_printf (clos->out, "# %s:\n", _("Read-only variable"));
	  print_descr (clos->out, gettext (sym->descr), 1, 3,
		       clos->width - 1, "# ");
	}
    }
  switch (vp->type)
    {
    case mailvar_type_number:
      mu_stream_printf (clos->out, "%s=%d", vp->name, vp->value.number);
      break;

    case mailvar_type_string:
      mu_stream_printf (clos->out, "%s=\"%s\"", vp->name, vp->value.string);
      break;

    case mailvar_type_boolean:
      if (!vp->value.bool)
	mu_stream_printf (clos->out, "no");
      mu_stream_printf (clos->out, "%s", vp->name);
      break;

    case mailvar_type_whatever:
      mu_stream_printf (clos->out, "%s %s", vp->name, _("oops?"));
    }
  mu_stream_printf (clos->out, "\n");
  return 0;
}

void
mailvar_print (int set)
{
  mu_list_t varlist;
  size_t count;
  struct mailvar_print_closure clos;

  varlist = mailvar_list_copy (set);
  mu_list_count (varlist, &count);
  clos.out = open_pager (count);
  clos.prettyprint = mailvar_is_true (mailvar_name_variable_pretty_print);
  clos.width = util_screen_columns ();

  mu_list_foreach (varlist, mailvar_printer, &clos);
  mu_list_destroy (&varlist);
  mu_stream_unref (clos.out);
}


void
mailvar_variable_format (mu_stream_t stream,
			 const struct mailvar_variable *var,
			 const char *defval)
{
  if (var)
    switch (var->type)
      {
      case mailvar_type_string:
	mu_stream_printf (stream, "%s", var->value.string);
	break;

      case mailvar_type_number:
	mu_stream_printf (stream, "%d", var->value.number);
	break;

      case mailvar_type_boolean:
	mu_stream_printf (stream, "%s", var->set ? "yes" : "no");
	break;

      default:
	if (defval)
	  mu_stream_printf (stream, "%s", defval);
	break;
      }
}


static char *typestr[] =
  {
    N_("untyped"),
    N_("numeric"),
    N_("string"),
    N_("boolean")
  };

static void
describe_symbol (mu_stream_t out, int width, const struct mailvar_symbol *sym)
{
  int i, t;
  const struct mailvar_symbol *ali;
  mu_stream_stat_buffer stat;

  mu_stream_set_stat (out, MU_STREAM_STAT_MASK (MU_STREAM_STAT_OUT),
		      stat);
  mu_stream_printf (out, "%s", sym->var.name);
  for (ali = sym + 1; ali->var.name && ali->flags & MAILVAR_ALIAS; ali++)
    {
      size_t len = strlen (ali->var.name) + 2;
      if (stat[MU_STREAM_STAT_OUT] + len > width)
	{
	  stat[MU_STREAM_STAT_OUT] = 0;
	  mu_stream_printf (out, "\n%s", ali->var.name);
	}
      else
	mu_stream_printf (out, ", %s", ali->var.name);
    }
  mu_stream_printf (out, "\n");
  mu_stream_set_stat (out, 0, NULL);

  mu_stream_printf (out, _("Type: "));
  for (i = 0, t = 0; i < sizeof (typestr) / sizeof (typestr[0]); i++)
    if (sym->flags & MAILVAR_TYPEMASK (i))
      {
	if (t++)
	  mu_stream_printf (out, " %s ", _("or"));
	mu_stream_printf (out, "%s", gettext (typestr[i]));
      }
  if (!t)
    mu_stream_printf (out, "%s", gettext (typestr[0]));
  mu_stream_printf (out, "\n");

  mu_stream_printf (out, "%s", _("Current value: "));
  mailvar_variable_format (out, &sym->var, _("[not set]"));

  if (sym->flags & MAILVAR_RDONLY)
    mu_stream_printf (out, " [%s]", _("read-only"));
  mu_stream_printf (out, "\n");

  print_descr (out, gettext (sym->descr ? sym->descr : N_("Not documented")),
	       1, 1, width - 1, NULL);
  mu_stream_printf (out, "\n");
}

int
mail_variable (int argc, char **argv)
{
  int pagelines = util_get_crt ();
  int width = util_screen_columns ();
  mu_stream_t out = open_pager (pagelines + 1);

  if (argc == 1)
    {
      struct mailvar_symbol *sym;

      for (sym = mailvar_tab; sym->var.name; sym++)
	if (!(sym->flags & (MAILVAR_HIDDEN|MAILVAR_ALIAS)))
	  describe_symbol (out, width, sym);
    }
  else
    {
      int i;

      for (i = 1; i < argc; i++)
	{
	  struct mailvar_symbol *sym = find_mailvar_symbol (argv[i]);
	  if (!sym)
	    mu_stream_printf (out, "%s: unknown\n", argv[i]);
	  else
	    describe_symbol (out, width, sym);
	}
    }
  mu_stream_unref (out);
  return 0;
}

#ifdef WITH_READLINE
static char *
mailvar_generator (int flags, const char *text, int state)
{
  static struct mailvar_iterator *itr;
  const char *p;

  if (!state)
    p = mailvar_iterate_first (flags, text, &itr);
  else
    p = mailvar_iterate_next (itr);

  if (!p)
    {
      mailvar_iterate_end (&itr);
      return NULL;
    }
  return strdup (p);
}

/* Completion generator for the "set" command */
static char *
mailvar_set_generator (const char *text, int state)
{
  return mailvar_generator (MAILVAR_ITR_WRITABLE, text, state);
}

/* Completion generator for the "unset" command */
static char *
mailvar_unset_generator (const char *text, int state)
{
  return mailvar_generator (MAILVAR_ITR_SET | MAILVAR_ITR_WRITABLE,
			    text, state);
}

/* Completion generator for the "variable" command */
static char *
mailvar_variable_generator (const char *text, int state)
{
  return mailvar_generator (MAILVAR_ITR_ALL, text, state);
}

/* Completion function for all three commands */
char **
mailvar_set_compl (int argc, char **argv, int point)
{
  /* Possible values for argv[0] are: set, unset, variable */
  char **matches =
    rl_completion_matches (point & COMPL_WS ? "" : argv[argc-1],
			   argv[0][0] == 'u'
			    ? mailvar_unset_generator
			    : argv[0][0] == 's'
			       ? mailvar_set_generator
			       : mailvar_variable_generator);
  ml_attempted_completion_over ();
  if (matches && matches[1] == NULL) /* Exact match */
    {
      switch (argv[0][0])
	{
	case 'u':
	  ml_set_completion_append_character (' ');
	  break;

	case 's':
	  {
	    struct mailvar_symbol *sym = find_mailvar_symbol (matches[0]);
	    if (sym &&
		(sym->flags & (MAILVAR_TYPEMASK (mailvar_type_string)
			       | MAILVAR_TYPEMASK (mailvar_type_number))))
	      ml_set_completion_append_character ('=');
	    else
	      ml_set_completion_append_character (' ');
	  }
	  break;

	default:
	  ml_set_completion_append_character (0);
	}
    }

  return matches;
}

#endif
