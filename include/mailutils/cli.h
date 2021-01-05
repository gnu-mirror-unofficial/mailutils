/* opt.h -- general-purpose command line option parser 
   Copyright (C) 2016-2021 Free Software Foundation, Inc.

   GNU Mailutils is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3, or (at
   your option) any later version.

   GNU Mailutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _MAILUTILS_CLI_H
#define _MAILUTILS_CLI_H
#include <stdio.h>
#include <mailutils/types.h>
#include <mailutils/cfg.h>
#include <mailutils/opt.h>

typedef void (*mu_cli_capa_commit_fp) (void *);

struct mu_cli_capa
{
  char *name;
  struct mu_option *opt;
  struct mu_cfg_param *cfg;
  mu_cfg_section_fp parser;
  mu_cli_capa_commit_fp commit;
};

void mu_cli_capa_init (void);
void mu_cli_capa_register (struct mu_cli_capa *capa);
void mu_cli_capa_extend_settings (char const *name, mu_list_t opts,
				  mu_list_t commits);

struct mu_cli_setup
{
  struct mu_option **optv;     /* Command-line options */
  struct mu_cfg_param *cfg;    /* Configuration parameters */
  char *prog_doc;              /* Program documentation string */
  char *prog_args;             /* Program arguments string */
  char const **prog_alt_args;  /* Alternative arguments string */
  char *prog_extra_doc;        /* Extra documentation.  This will be
				  displayed after options. */
  int ex_usage;                /* If not 0, exit code on usage errors */
  int ex_config;               /* If not 0, exit code on configuration
				  errors */
  int inorder:1;               /* Don't permute options and arguments */
  int server:1;                /* This is a server: don't read per-user
				  configuration files */
  void (*prog_doc_hook) (mu_stream_t);
};

extern const char mu_version_copyright[];
extern const char mu_general_help_text[];

void mu_version_hook (struct mu_parseopt *po, mu_stream_t stream);
void mu_cli (int argc, char **argv, struct mu_cli_setup *setup,
	     char **capa, void *data,
	     int *ret_argc, char ***ret_argv);
void mu_cli_ext (int argc, char **argv,
		 struct mu_cli_setup *setup,
		 struct mu_parseopt *pohint,
		 struct mu_cfg_parse_hints *cfhint,
		 char **capa,
		 void *data,
		 int *ret_argc, char ***ret_argv);

char *mu_site_config_file (void);

void mu_acl_cfg_init (void);

/* Simplified interface */
enum
  {
    MU_CLI_OPTION_END = -1,     /* End of options */

    /* Argument: struct mu_option * 
     * Description: Supplies the array of options.  Can be given mupliple
     * times. 
     * Ref: optv in struct mu_cli_setup.
     */
    MU_CLI_OPTION_OPTIONS,

    /* Argument: struct mu_cfg_param * 
     * Description: Supplies configuration definitions.
     * NULL argument is a no-op.
     * Ref: cfg in struct mu_cli_setup
     */
    MU_CLI_OPTION_CONFIG,

    /* Argument: char **
     * Description: NULL-terminated array of capability strings.
     * NULL argument is a no-op.
     * Ref: capa argument to mu_cli and mu_cli_ext.
     */
    MU_CLI_OPTION_CAPABILITIES,

    /* Argument: int
     * Description: Exit code to use on usage errors (default: EX_USAGE).
     * Ref: ex_usage member of struct mu_cli_setup.
     */
    MU_CLI_OPTION_EX_USAGE,

    /* Argument: int
     * Description: Exit code to use on configuration errors (default:
     * EX_CONFIG).
     * Ref: ex_config member of struct mu_cli_setup.
     */
    MU_CLI_OPTION_EX_CONFIG,

    /* Argument: none
     * Description: Ignore usage errors.
     * Ref: MU_PARSEOPT_IGNORE_ERRORS flag in opt.h
     */
    MU_CLI_OPTION_IGNORE_ERRORS,

    /* Argument: int *
     * Description: A pointer to store the number of command line
     * arguments in.
     * Ref: ret_argc argument in mu_cli and mu_cli_ext.
     */
    MU_CLI_OPTION_RETURN_ARGC,

    /* Argument: char ***
     * Description: A pointer to store the address of the array of
     * command line arguments.
     * Ref: ret_argv argument in mu_cli and mu_cli_ext.
     */
    MU_CLI_OPTION_RETURN_ARGV,

    /* Argument: none
     * Description: Do not reorder options and arguments.
     * Ref: The inorder member of struct mu_cli_setup and
     *      MU_PARSEOPT_IN_ORDER flag in opt.h
     */
    MU_CLI_OPTION_IN_ORDER,

    /* Argument: none
     * Description: Don't provide standard options: -h, --help, --usage,
     * --version.
     * Ref: MU_PARSEOPT_NO_STDOPT flag in opt.h
     */
    MU_CLI_OPTION_NO_STDOPT,

    /* Argument: none
     * Description: Don't exit on errors
     * Ref: MU_PARSEOPT_NO_ERREXIT flag in opt.h
     */
    MU_CLI_OPTION_NO_ERREXIT,

    /* Argument: none
     * Description: Apply all options immediately.
     * Ref: MU_PARSEOPT_IMMEDIATE
     */
    MU_CLI_OPTION_IMMEDIATE,

    /* Argument: none
     * Description: Don't sort options in help output.
     * Ref: MU_PARSEOPT_NO_SORT flag in opt.h.
     */
    MU_CLI_OPTION_NO_SORT,        

    /* Argument: char const *
     * Description: Override the name of the program to use in error
     * messages.
     * NULL argument is a no-op.
     * Ref: MU_PARSEOPT_PROG_NAME flag in opt.h
     */
    MU_CLI_OPTION_PROG_NAME,      

    /* Argument: char const *
     * Description: Documentation string for the program. It will be
     * displayed on the line following the usage summary.
     * NULL argument is a no-op.
     * Ref: MU_PARSEOPT_PROG_DOC
     */
    MU_CLI_OPTION_PROG_DOC,       

    /* Argument: char const *
     * Description: Names of arguments for the program. These are
     * displayed in the constructed usage summary.  For example, if
     * this keyword is set tp "A B" and the program name is "foo", then
     * the help output will begin with
     *    Usage: foo A B
     *
     * Multiple instances are allowed.
     * NULL argument is a no-op.
     * Ref: MU_PARSEOPT_PROG_ARGS flag in opt.h
     */
    MU_CLI_OPTION_PROG_ARGS,      

    /* Argument: char const *
     * Description: Bug address for the package.
     * Ref: MU_PARSEOPT_BUG_ADDRESS in opt.h
     */
    MU_CLI_OPTION_BUG_ADDRESS,    

    /* Argument: char const *
     * Description: Sets the PACKAGE_NAME.
     * NULL argument is a no-op.
     * Ref: MU_PARSEOPT_PACKAGE_NAME in opt.h
     */
    MU_CLI_OPTION_PACKAGE_NAME,   

    /* Argument: char const *
     * Description: URL of the package.
     * NULL argument is a no-op.
     * Ref: MU_PARSEOPT_PACKAGE_URL in opt.h
     */
    MU_CLI_OPTION_PACKAGE_URL,    

    /* Argument: char const *
     * Description: Extra help information.  This will be displayed after
     * the option list.
     * NULL argument is a no-op.
     * Ref: MU_PARSEOPT_EXTRA_INFO in opt.h
     */
    MU_CLI_OPTION_EXTRA_INFO,     

    /* Argument: void (*) (struct mu_parseopt *, mu_stream_t)
     * Description: Pointer to a function to be called as a part of the
     * --help option handling, after outputting the option list.
     * NULL argument is a no-op.
     * Ref: po_help_hook member of struct mu_parseopt and the
     * MU_PARSEOPT_HELP_HOOK flag in opt.h
     */
    MU_CLI_OPTION_HELP_HOOK,      

    /* Argument: void *
     * Description: Call-specific configuration data.  This will be passed
     * as the last argument to mu_cfg_tree_reduce.
     * 
     * Ref: mu_cfg_tree_reduce
     */
    MU_CLI_OPTION_DATA,           

    /* Argument: void (*) (struct mu_parseopt *, mu_stream_t)
     * Description: User function to be called on --version.
     * Ref: po_version_hook member of struct mu_parseopt and the
     * MU_PARSEOPT_VERSION_HOOK flag in opt.h
     */
    MU_CLI_OPTION_VERSION_HOOK,   

    /* Argument: void (*) (struct mu_parseopt *, mu_stream_t)
     * Description: Pointer to a function to be called as a part of the
     * --help option handling.  This function will be called after printing
     * initial program description (see MU_CLI_OPTION_PROG_DOC) and before
     * printing the option summary.
     * NULL argument is a no-op.
     * Ref: po_prog_doc_hook member of struct mu_parseopt and the
     * MU_PARSEOPT_PROG_DOC_HOOK in opt.h
     */
    MU_CLI_OPTION_PROG_DOC_HOOK,  

    /* Argument: none 
     * Description: Long options start with single dash (a la find).  Thid
     * Disables recognition of traditional short options.
     * Ref: MU_PARSEOPT_SINGLE_DASH flag in opt.h
     */
    MU_CLI_OPTION_SINGLE_DASH,    

    /* Argument: char const *
     * Description: Prefix that negates the value of a boolean option.
     * NULL argument is a no-op.
     * Ref: po_negation member of struct mu_parseopt and the
     * MU_PARSEOPT_NEGATION flag in opt.h
     */
    MU_CLI_OPTION_NEGATION,       

    /* Argument: char const *
     * Description: Descriptive names of special arguments. If given, this
     * will be printed in short usage summary after the regular options.
     * NULL argument is a no-op.
     * Ref: The po_special_args member of struct mu_parseopt and the
     * MU_PARSEOPT_SPECIAL_ARGS flag in opt.h
     */
    MU_CLI_OPTION_SPECIAL_ARGS,


    /* Argument: char const *
     * Description: Name of site-wide configuration file to parse.  If NULL,
     * the default site-wide configuration is assumed.
     * Ref: The site_file member of struct mu_cfg_parse_hints and the
     * MU_CFHINT_SITE_FILE flag in cfg.h
     */
    MU_CLI_OPTION_CONF_SITE_FILE,

    /* Argument: none
     * Description: Enable the use of per-user configuration files.
     * Ref: MU_CFHINT_PER_USER_FILE flag in cfg.h
     */
    MU_CLI_OPTION_CONF_PER_USER_FILE,

    /* Argument: none
     * Description:  Don't allow users to overide configuration settings
     * from the command line.
     * Ref: MU_CFHINT_NO_CONFIG_OVERRIDE flag in cfg.h
     */
    MU_CLI_OPTION_CONF_NO_OVERRIDE,

    /* Argument: char const *
     * Description: Identifier of the per-program section in the configuration
     * file or name of the per-program configuration file (used when
     * processing the "include" statement with a directory name as its
     * argument).  If NULL or not set, program name is used.  See also
     * MU_CLI_OPTION_PROG_NAME.
     * Ref: MU_CFHINT_PROGRAM flag in cfg.h
     */
    MU_CLI_OPTION_CONF_PROGNAME,
  };

void mu_cli_simple (int argc, char **argv, ...);

#endif
