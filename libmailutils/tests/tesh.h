/* Definitions of test shell functions for GNU Mailutils.
   Copyright (C) 2019-2021 Free Software Foundation, Inc.

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

/* Test shell commands are implemented via mu_tesh_function_t functions.
   Arguments:
    argc    - argument count
    argv    - argument vector
    options - assoc of options
    env     - evaluation environment (closure)
*/
typedef int (*mu_tesh_function_t)(int argc, char **argv, mu_assoc_t options,
				  void *env);

/* Option argument types */
enum mu_tesh_arg
  {
    mu_tesh_noarg,         /* No argument allowed */
    mu_tesh_arg_required,  /* Argument is required */
    mu_tesh_arg_optional   /* Argument is optional */
  };

/* Test shell command definition */
struct mu_tesh_command
{
  /* The following fields must be filled by the caller */
  char *verb;              /* Command verb [1] */
  char *args;              /* Description of command arguments [2] */
  mu_tesh_function_t func; /* Command handler function */

  /* The fields below are computed and used by the shell itself.
     The caller MAY NOT initialize them.
  */

  int param_min;        /* Minimum number of parameters */
  int param_max;        /* Maximum number of parameters. -1 for variadic */
  mu_assoc_t options;   /* Assoc array of the allowed options, indexed by
			   option names without the leading dash. Values
			   are of enum mu_tesh_arg type. */
};

/* Notes:

   [1] The command verb "help" is implicitly defined. It produces a short
       summary of the available shell commands. The special commands

       The following special command verbs can be used:
	 __LINEPROC__
	    Called before word splitting. The entire command line is passed
	    as argv[0]. The argc is 1, and the options argument is NULL.

	    The function returns 0 to indicate that it has processed the
	    input. In this case, no further input handing will be performed
	    and the shell will proceed to next line from the input.
	    Otherwise, the shell will perform word splitting and initiate
	    further processing.

	    The function is free to modify or reassign argv[0] if it needs
	    so. The modified value will serve as input for further processing.
	    If the function chooses to reassign argv[0], it may not free the
	    current value, and the caller remains responsible for any memory
	    management needed. For example, if a newly allocated memory is
	    assigned to argv[0], this pointer must be saved somewhere in the
	    'env' to be subsequently freed by the __ENVFINI__ handler.

	 __NOCMD__
	    Called if the first token in the command line does not match
	    any defined command. The result of word splitting is passed in
	    argc and argv. The options argument is NULL. The function must
	    return 0 if it was able to process the input, and any non-zero
	    value otherwise. In the latter case, the shell will issue the
	    "no such command" diagnostics.

	 __ENVINIT__
	    Initialize the environment prior to running the command. The
	    argc, argv, and options arguments contain the result of command
	    line parsing.

	    The command must return 0 on success.

	 __ENVFINI__
	    Do any post-command cleanup of the environment. Arguments are
	    the same as to __ENVINIT__.

	    Return value is ignored.

	 __HELPINIT__
	    Print initial part of the help output. This function is called
	    by mu_tesh_help prior to printing the command summary.
	    The argc parameter is 0, argv and options are NULL.

	    Return value is ignored.

	 __HELPFINI__
	    Print final part of the help output. This function is called
	    by mu_tesh_help prior after printing the command summary.

	    Arguments are the same as for __HELPINIT__. Return value is
	    ignored.

       Apart from these, no command verbs may begin and end with two
       underscore characters.

   [2] The args field in the above structure serves two purposes. First,
       it describes the options and arguments to the shell command in
       unambiguous way and is used during command line parsing. Secondly,
       it is output by mu_tesh_help as a human-readable command line
       syntax summary.

       The syntax of this field is described by the BNF below:

	   <args> ::= <arglist> | <arglist> <ws> "..." |
		      <arglist> "..."
	   <arglist> ::= <argdef> | <arglist> <ws> <argdef>
	   <argdef> ::= <optdef> | <optarg> | STRING
	   <optdef> ::= "[-" STRING "]" |
			"[-" STRING "=" STRING "]" |
			"[-" STRING "[=" STRING "]]"
	   <optarg> ::= "[" string-list "]"
	   <string-list> ::= STRING | <string-list> <ws> STRING
	   <ws> ::= 1*WS

       STRING is any contiguous sequence of printable characters, other
       than whitespace, '[', or ']'. WS is horizontal space or tab
       character.

       Ellipsis can appear immediately before the closing "]" of an <optdef>
       or <optarg>.

       The <optdef> token defines options that can be supplied to the
       command. The <optdef>s can be intermixed with other tokens in an
       <args> string. However, in actual command invocation, options must
       precede positional arguments. The "--" argument can be given to
       explicitly stop option processing.

       Both trailing ellipsis and <optarg> indicate variable number of
       arguments to the command.
*/


/* Initialize the shell. */
void mu_tesh_init (char const *argv0);

/* Main command evaluation loop.

   If argc > 0, commands are read from argv. They must be delimited by
   semicolons (obviously, if argc,argv are obtained from the shell command
   line, the user must take care to escape each ';' to prevent it from
   being processed by the shell). A semicolon may appear either as a separate
   argument or as a last character of command's argument, e.g. supposing that
   't' is a command line utility using tesh, both invocations below are valid:

      t foo bar \; baz qux
      t foo 'bar;' baz qux

   If argc == 0, commands are read from the standard input. The '#' character
   begins an inline comment.
 */
void mu_tesh_read_and_eval (int argc, char **argv,
			    struct mu_tesh_command *cmd,
			    void *env);

/* Print a short summary of shell commands. */
void mu_tesh_help (struct mu_tesh_command *cmd, void *env);
