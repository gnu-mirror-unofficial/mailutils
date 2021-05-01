/* 
   NAME
     lck - mailutils locking test
     
   SYNOPSIS
     lck [-akpuv?] [-eCOMMAND] [-f SECONDS] [-H SECONDS] [-r N] [-t SECONDS]
         [--abandon] [--delay=SECONDS] [--expire=SECONDS]
         [--external[=COMMAND]] [--help] [--hold=SECONDS] [--kernel]
         [--pid-check] [--retry=N] [--show-config-options] [--unlock]
         [--usage] [--verbose] FILE

   DESCRIPTION
     Tests the mailutils locking mechanism.  Unless --hold (-H) or --abandon
     (-a) option is used, the tool locks the FILE and exits.  If the --unlock
     option is given, existing file lock is removed instead.

     The --hold and --abandon options are used to simulate locking conflict
     conditions.  Both options cause lck to fork a child process, which
     will attempt to lock the file using the same options as the main (master)
     process.  After obtaining the lock, the child notifies the master process
     and, if --hold=N option was given, sleeps for N seconds before releasing
     the lock.  If the --abandon option is given, the lock is not released.
     After that, the child terminates.
     
     The master waits for child to successfully lock the file and attempts to
     obtain the lock.  If successful, it exits with the 0 status.  On errors,
     the termination status is:

     1    An error other than described below occurred.  This includes
          usage errors.
     2    Unlock requested, but file is not locked.
     3    Lock requested, but file is already locked by another program.
     4    Insufficient permissions.

   OPTIONS
      Locking type (default: dotlock):

      -e, --external[=COMMAND]
          Use the external locker command.  If COMMAND is omitted,
	  the compiled-in default will be used (see
	  MU_LOCKER_DEFAULT_EXT_LOCKER in mailutils/locker.h.
	  
      -k, --kernel
          Use kernel locking (fnctl).

      Locking parameters

      -p, --pid-check
          Check if the PID of lock owner is still active.
	  
      -r, --retry=N
          Retry the lock N times.
	  
      -t, --delay=SECONDS
          Delay between two successive locking attempts.
	  
      -f, --expire=SECONDS
          Expire the lock after that many seconds.

      Child operation modifiers

      -a, --abandon
          Abandon lock in child.
	  
      -H, --hold=SECONDS
          Hold the lock for that many seconds.

      Operation modifiers

      -u, --unlock
          Release the existing lock.

      -v, --verbose
          If the lock (or unlock) operation fails, print the error
	  message on the stderr in addition to exiting with the error
	  status.
	  
      Informational options

      --show-config-options
          Show compilation options.

      -?, --help
          Give a short help list.

      --usage
          Give a short usage message.

  AUTHOR
    Sergey Poznyakoff <gray@gnu.org>
    
  LICENSE
    This program is part of GNU Mailutils testsuite.
    Copyright (C) 2020-2021 Free Software Foundation, Inc.

    GNU Mailutils is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3, or (at your option)
    any later version.

    GNU Mailutils is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>.
    
*/
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <mailutils/mailutils.h>

static mu_locker_hints_t hints = { .flags = 0 };
static int unlock;
static int pid_check;
static unsigned hold_time;
static int abandon_lock;
static int verbose;

static void
cli_type (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  switch (opt->opt_short)
    {
    case 'k':
      hints.flags |= MU_LOCKER_FLAG_TYPE;
      hints.type  = MU_LOCKER_TYPE_KERNEL;
      break;

    case 'e':
      hints.flags |= MU_LOCKER_FLAG_TYPE;
      hints.type  = MU_LOCKER_TYPE_EXTERNAL;
      if (arg)
	{
	  hints.flags |= MU_LOCKER_FLAG_EXT_LOCKER;
	  hints.ext_locker = strdup (arg);
	}
      break;

    default:
      abort ();
    }
}

struct mu_option options[] = {
  MU_OPTION_GROUP ("Locking type (default: dotlock)"),

  { "kernel", 'k', NULL, MU_OPTION_DEFAULT,
    "use kernel locking", mu_c_void, NULL, cli_type },
  { "external", 'e', "COMMAND", MU_OPTION_ARG_OPTIONAL,
    "use external locker command", mu_c_void, NULL, cli_type },
  
  MU_OPTION_GROUP ("Locking parameters"),
  { "retry",  'r', "N", MU_OPTION_DEFAULT,
    "retry the lock N times",
    mu_c_uint, &hints.retry_count },
  { "delay",  't', "SECONDS", MU_OPTION_DEFAULT,
    "delay between two successive locking attempts (in seconds)",
    mu_c_uint, &hints.retry_sleep },
  { "pid-check", 'p', NULL, MU_OPTION_DEFAULT,
    "check if the PID of lock owner is still active",
    mu_c_bool, &pid_check },
  { "expire", 'f', "SECONDS", MU_OPTION_DEFAULT,
    "expire the lock after that many seconds",
    mu_c_uint, &hints.expire_time },
  
  MU_OPTION_GROUP ("Child operation modifiers"),
  { "hold",   'H', "SECONDS", MU_OPTION_DEFAULT,
    "hold lock for that many seconds",
    mu_c_uint, &hold_time },
  { "abandon",'a', NULL, MU_OPTION_DEFAULT,
    "abandon lock in child",
    mu_c_bool, &abandon_lock },

  MU_OPTION_GROUP ("Operation modifiers"),
  { "unlock", 'u', NULL, MU_OPTION_DEFAULT,
    "unlock", mu_c_bool, &unlock },
  
  { "verbose", 'v', NULL, MU_OPTION_DEFAULT,
    "verbosely list errors", mu_c_bool, &verbose },
  MU_OPTION_END
};

static pid_t child_pid;
static int child_status;
static int time_out;

void
sighan (int sig)
{
  switch (sig)
    {
    case SIGCHLD:
      if (waitpid (child_pid, &child_status, WNOHANG) == child_pid)
	child_pid = 0;
      break;

    case SIGALRM:
      time_out = 1;
      break;
    }
}

static void
errcheck (int rc)
{
  switch (rc)
    {
    case 0:
      break;
      
    case MU_ERR_LOCK_CONFLICT:
      exit (MU_DL_EX_EXIST);

    case MU_ERR_LOCK_NOT_HELD:
      exit (MU_DL_EX_NEXIST);
      
    case EPERM:
    case EACCES:
      exit (MU_DL_EX_PERM);
      
    default:
      exit (MU_DL_EX_ERROR);
    }
}

int
main (int argc, char **argv)
{
  mu_locker_t lck;
  char const *file;
  int rc;
  
  mu_cli_simple (argc, argv,
                 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC, "locking test tool",
		 MU_CLI_OPTION_PROG_ARGS, "FILE",
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
		 MU_CLI_OPTION_RETURN_ARGV, &argv,
		 MU_CLI_OPTION_END);
  
  if (argc != 1)
    {
      mu_error ("bad arguments; try %s --help for more info", mu_program_name);
      return MU_DL_EX_ERROR;
    }
  file = argv[0];
  
  if (hints.expire_time)
    hints.flags |= MU_LOCKER_FLAG_EXPIRE_TIME;
  if (hints.retry_count || hints.retry_sleep)
    hints.flags |= MU_LOCKER_FLAG_RETRY;
  if (pid_check)
    hints.flags |= MU_LOCKER_FLAG_CHECK_PID;
    
  MU_ASSERT (mu_locker_create_ext (&lck, file, hints.flags > 0 ? &hints : NULL));

  if (hold_time || abandon_lock)
    {
      FILE *fp;
      int p[2];

      signal (SIGCHLD, sighan);
      if (pipe (p))
	{
	  mu_diag_funcall (MU_DIAG_CRIT, "pipe", NULL, errno);
	  return MU_DL_EX_ERROR;
	}
      
      child_pid = fork ();
      if (child_pid == -1)
	{
	  mu_diag_funcall (MU_DIAG_CRIT, "fork", NULL, errno);
	  return MU_DL_EX_ERROR;
	}
      
      if (child_pid == 0)
	{
	  /* child */
	  signal (SIGCHLD, SIG_IGN);
	  
	  fp = fdopen (p[1], "w");
	  close (p[0]);
	  
	  rc = mu_locker_lock (lck);
	  fprintf (fp, "L%d\n", rc);
	  fclose (fp);
	  errcheck (rc);
	  if (hold_time)
	    sleep (hold_time);
	  if (abandon_lock)
	    rc = 0;
	  else
	    rc = mu_locker_remove_lock (lck);
	  exit (rc != 0);
	}
      
      /* master */
      fp = fdopen (p[0], "r");
      close (p[1]);

      signal (SIGALRM, sighan);
      alarm (5);
      if (fscanf (fp, "L%d", &rc) != 1)
	{
	  if (time_out)
	    mu_error ("child didn't respond");
	  else
	    mu_error ("bad response from child");
	  if (child_pid)
	    kill (child_pid, SIGKILL);
	  return 1;
	}
      alarm (0);
      
      if (rc)
	{
	  mu_error ("child lock failed");
	  if (child_pid)
	    kill (child_pid, SIGKILL);
	  return 1;
	}
    }

  if (unlock)
    rc = mu_locker_remove_lock (lck);
  else
    rc = mu_locker_lock (lck);
  if (rc && verbose)
    mu_diag_funcall (MU_DIAG_ERROR,
		     unlock ? "mu_locker_remove_lock" : "mu_locker_lock",
		     NULL, rc);

  mu_locker_destroy (&lck);
  
  if (child_pid > 0)
    {
      if (waitpid (child_pid, &child_status, WNOHANG) != child_pid)
	{
	  if (child_pid)
	    kill (child_pid, SIGKILL);
	  waitpid (child_pid, &child_status, 0);
	  child_status = 0;
	}
    }

  if (WIFEXITED (child_status))
    {
      int status = WEXITSTATUS (child_status);
      if (status != 0)
	{
	  mu_error ("child terminated with status %d", status);
	  return MU_DL_EX_ERROR;
	}
    }
  else if (WIFSIGNALED (child_status))
    {
      mu_error ("child terminated on signal %d", WTERMSIG (child_status));
      return MU_DL_EX_ERROR;
    }
  else
    {
      mu_error ("child terminated with unhandled status %d", child_status);
      return MU_DL_EX_ERROR;
    }

  errcheck (rc);
  return 0;
}
