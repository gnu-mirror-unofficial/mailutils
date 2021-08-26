/* NAME
     tlscpy - test mailutils TLS streams

   SYNOPSIS
     tlscpy [-aFv] [-C FILE] [-i NAME] [-k FILE] [-o FILE] [-p STRING]
            [-t SECONDS] [--ca-file=FILE] [--cert-file=FILE]
	    [--full-buffer] [--implementation=STRING] [--key-file=FILE]
	    [--output=FILE] [--priorities=STRING] [--source=NAME]
	    [--handshake-timeout=SECONDS] [FILE...]

   DESCRIPTION
     Starts a sub-process and installs a TLS connection between in and
     the master process using the supplied certificate and key files.
     Once the connection is established, sends the FILEs listed in the
     command line over it.  The direction to send is defined by the
     --source option.  If --source=client (the default), files are sent
     from the main process (client) to the subprocess (server).  If
     --source=server, the direction is reversed.

     Unless the -o (--output) option is specified, the received data
     are written to the named outpuf file.  Otherwise, they are discarded.

   OPTIONS

     For normal operation, at least -c (--cert-file) and -k (--key-file)
     should be given.
   
     -a, --append 
             Append to the output file instead of overwriting it.
	     
     -C, --ca-file=FILE
             Certificate authority file name.
	     
     -c, --cert-file=FILE
             Certificate file name.
	     
     -F, --full-buffer
             Enable full buffering.  Default is line buffering.
	     
     -i, --implementation=STRING
             Select TLS implementation to use.  Valid values for STRING
	     are: "old" which means to use the mu_tls_stream_create
	     function, and "new", which means to use the
	     mu_tlsfd_stream_create (introduced in 9571268d27).  "new"
	     is the default.
	     
     -k, --key-file=FILE
             Certificate key file name.
	     
     -o, --output=FILE
             Output file name.  By default, output goes to null stream.
	     
     -p, --priorities=STRING
             Priorities to use.  Default is "NORMAL".

     -s, --source=NAME
             Sets data source.  Allowed values for NAME are: "server"
	     and "client" (default).

     -t, --handshake-timeout=SECONDS
             Set TLS handshake timeout.  Default is 10 seconds.
	     
     -v, --verbose
             Increase output verbosity.  This displays the selected
	     cipher at the beginning and total run time at the end
	     of the rum.
   
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <mailutils/mailutils.h>

static struct mu_tls_config tls_conf; /* TLS configuration */

static char *outfile;          /* Output file name. */
static mu_stream_t outstream;  /* Output file stream. */
static int append_option;      /* If set, append data to the output. */
static int verbose_option;     /* Additional verbosity. */
static int full_option;        /* Use full buffering. */
static char *source_option = "client"; /* Source: client or server. */

static void
abquit (char const *func, char const *arg, int err, char const *prefix)
{
  if (prefix)
    {
      if (err)
	mu_diag_output (MU_DIAG_ERROR, "%s: %s(%s) failed: %s", prefix,
			func, mu_prstr (arg),
			mu_strerror (err));
      else
	mu_diag_output (MU_DIAG_ERROR, "%s: %s(%s) failed", prefix,
			func, mu_prstr (arg));
    }
  else
    mu_diag_funcall (MU_DIAG_ERROR, func, arg, err);
  exit (1);
}

static void
log_cipher (mu_stream_t stream)
{
  mu_property_t prop;
  int rc = mu_stream_ioctl (stream, MU_IOCTL_TLSSTREAM,
			    MU_IOCTL_TLS_GET_CIPHER_INFO, &prop);
  if (rc)
    {
      mu_diag_output (MU_DIAG_INFO, "TLS established");
      mu_error ("can't get TLS details: %s", mu_strerror (rc));
    }
  else
    {
      char const *cipher, *mac, *proto;
      if (mu_property_sget_value (prop, "cipher", &cipher))
	cipher = "UNKNOWN";	
      if (mu_property_sget_value (prop, "mac", &mac))
	mac = "UNKNOWN";
      if (mu_property_sget_value (prop, "protocol", &proto))
	proto = "UNKNOWN";
      
      mu_diag_output (MU_DIAG_INFO, "TLS established using %s-%s (%s)",
		      cipher, mac, proto);
      
      mu_property_destroy (&prop);
    }
}

enum { OLD, NEW };

static int client_impl = NEW;
static int server_impl = NEW;

static char *typestr[] = {
  [MU_TLS_CLIENT] = "client",
  [MU_TLS_SERVER] = "server"
};

static mu_stream_t
tls_create_new (int fd, int type)
{
  mu_stream_t str;
  int rc = mu_tlsfd_stream_create (&str, fd, fd, &tls_conf, type);
  if (rc)
    abquit ("mu_tlsfd_stream_create", NULL, rc, typestr[type]);
  return str;
}

static mu_stream_t
tls_create_old (int fd, int type)
{
  mu_stream_t str, istream, ostream;
  int rc;
  
  rc = mu_stdio_stream_create (&istream, fd, MU_STREAM_READ);
  if (rc)
    abquit ("mu_stdio_stream_create", "istream", rc, typestr[type]);
  mu_stream_set_buffer (istream, mu_buffer_line, 0);

  rc = mu_stdio_stream_create (&ostream, fd, MU_STREAM_WRITE);
  if (rc)
    abquit ("mu_stdio_stream_create", "ostream", rc, typestr[type]);
  mu_stream_set_buffer (ostream, mu_buffer_line, 0);

  rc = mu_tls_stream_create (&str, istream, ostream, &tls_conf, type, 0);
  if (rc)
    abquit ("mu_tls_stream_create", NULL, rc, typestr[type]);
  
  return str;
}

typedef mu_stream_t (*create_fn) (int, int);

static create_fn tls_create[2] = {
  [OLD] = tls_create_old,
  [NEW] = tls_create_new
};

static void
server (int fd, void (*func) (mu_stream_t, void *), void *data)
{
  pid_t pid;
  
  pid = fork ();
  if (pid == -1)
    abquit ("fork", NULL, errno, NULL);

  if (pid == 0)
    {
      mu_stream_t str;
      
      str = tls_create[server_impl] (fd, MU_TLS_SERVER);
      if (full_option)
	mu_stream_set_buffer (str, mu_buffer_full, 0);

      if (verbose_option)
	log_cipher (str);

      func (str, data);
      mu_stream_destroy (&str);
      _exit (0);
    }
}

static void
client (int fd, void (*func) (mu_stream_t, void *), void *data)
{
  mu_stream_t str;
  
  str = tls_create[client_impl] (fd, MU_TLS_CLIENT);
  if (full_option)
    mu_stream_set_buffer (str, mu_buffer_full, 0);
  func (str, data);
  mu_stream_destroy (&str);
}

void
send_files (mu_stream_t str, void *data)
{
  int i, rc;
  char **argv = data;
  
  for (i = 0; argv[i]; i++)
    {
      mu_stream_t input;

      rc = mu_file_stream_create (&input, argv[i], MU_STREAM_READ);
      if (rc)
	abquit ("mu_file_stream_create", argv[i], rc, typestr[MU_TLS_CLIENT]);

      rc = mu_stream_copy (str, input, 0, NULL);
      if (rc)
	abquit ("mu_stream_copy", argv[i], rc, typestr[MU_TLS_CLIENT]);

      mu_stream_destroy (&input);
    }
}  

void
recv_files (mu_stream_t str, void *data)
{
  int rc;
  rc = mu_stream_copy (outstream, str, 0, NULL);
  if (rc)
    abquit ("mu_stream_copy", NULL, rc, NULL);
}

static struct timespec
timespec_sub (struct timespec a, struct timespec b)
{
  struct timespec d;

  d.tv_sec = a.tv_sec - b.tv_sec;
  d.tv_nsec = a.tv_nsec - b.tv_nsec;
  if (d.tv_nsec < 0)
    {
      --d.tv_sec;
      d.tv_nsec += 1e9;
    }

  return d;
}

static int
str2impl (char const *str)
{
  if (strncmp (str, "new", 3) == 0)
    return NEW;
  else if (strncmp (str, "old", 3) == 0)
    return OLD;
  return -1;
}

static void
cb_impl (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  int n;
  
  n = str2impl (arg);
  if (n == -1)
    {
      mu_parseopt_error (po, "bad implementation type near %s", arg);
      exit (po->po_exit_error);
    }
  arg += 3;
  if (*arg == 0)
    {
      client_impl = server_impl = n;
      return;
    }
  else if (*arg != ':')
    {
      mu_parseopt_error (po, "bad delimiter near %s", arg);
      exit (po->po_exit_error);
    }

  client_impl = n;

  arg++;
  n = str2impl (arg);
  if (n == -1)
    {
      mu_parseopt_error (po, "bad implementation type near %s", arg);
      exit (po->po_exit_error);
    }
  else if (arg[3] != 0)
    {
      mu_parseopt_error (po, "garbage in argument near %s", arg + 3);
      exit (po->po_exit_error);
    }
  server_impl = n;
}
    
static struct mu_option options[] = {
  { "ca-file", 'C', "FILE", MU_OPTION_DEFAULT,
    "certificate authority file name",
    mu_c_string, &tls_conf.ca_file },
  { "cert-file", 'c', "FILE", MU_OPTION_DEFAULT,
    "certificate file name",
    mu_c_string, &tls_conf.cert_file },
  { "key-file", 'k', "FILE", MU_OPTION_DEFAULT,
    "certificate key file name",
    mu_c_string, &tls_conf.key_file },
  { "priorities", 'p', "STRING", MU_OPTION_DEFAULT,
    "priorities to use",
    mu_c_string, &tls_conf.priorities },
  { "handshake-timeout", 't', "SECONDS", MU_OPTION_DEFAULT,
    "TLS handshake timeout",
    mu_c_uint, &tls_conf.handshake_timeout },
  { "output", 'o', "FILE", MU_OPTION_DEFAULT,
    "output file name",
    mu_c_string, &outfile },
  { "append", 'a', NULL, MU_OPTION_DEFAULT,
    "append to the output file",
    mu_c_incr, &append_option },
  { "implementation", 'i', "STRING", MU_OPTION_DEFAULT,
    "select TLS implementation to use",
    mu_c_string, NULL, cb_impl },
  { "verbose", 'v', NULL, MU_OPTION_DEFAULT,
    "increase output verbosity",
    mu_c_incr, &verbose_option },
  { "source", 's', "server|client", MU_OPTION_DEFAULT,
    "set source",
    mu_c_string, &source_option },
  { "full-buffer", 'F', NULL, MU_OPTION_DEFAULT,
    "full buffering",
    mu_c_incr, &full_option },
  MU_OPTION_END
};

int
main (int argc, char **argv)
{
  int rc;
  struct timespec ts_start, ts_stop, rt;
  void (*func[2]) (mu_stream_t, void *);
  int sv[2];
  
  mu_set_program_name (argv[0]);

  mu_tls_key_file_checks = MU_FILE_SAFETY_NONE;
  mu_tls_cert_file_checks = MU_FILE_SAFETY_NONE;
  mu_tls_ca_file_checks = MU_FILE_SAFETY_NONE;
  
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_PROG_DOC, "Test mailutils TLS implementation",
		 MU_CLI_OPTION_PROG_ARGS, "[FILE ...]",
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
		 MU_CLI_OPTION_RETURN_ARGV, &argv,
		 MU_CLI_OPTION_END);

  if (mu_c_strcasecmp (source_option, "server") == 0)
    {
      func[MU_TLS_SERVER] = send_files;
      func[MU_TLS_CLIENT] = recv_files;
    }
  else if (mu_c_strcasecmp (source_option, "client") == 0)
    {
      func[MU_TLS_SERVER] = recv_files;
      func[MU_TLS_CLIENT] = send_files;
    }
  else
    {
      mu_error ("bad source value");
      exit (2);
    }
      
  
  if (outfile)
    {
      rc = mu_file_stream_create (&outstream, outfile,
				  MU_STREAM_CREAT |
				  (append_option
				      ? MU_STREAM_APPEND
				      : MU_STREAM_WRITE));
      if (rc)
	abquit ("mu_file_stream_create", outfile, rc, NULL);
    }
  else
    {
      rc = mu_nullstream_create (&outstream, MU_STREAM_WRITE);
      if (rc)
	abquit ("mu_nullstream_create", NULL, rc, NULL);
    }

  if (socketpair (AF_UNIX, SOCK_STREAM, 0, sv))
    abquit ("socketpair", NULL, errno, NULL);
  
  clock_gettime (CLOCK_MONOTONIC, &ts_start);  
  server (sv[1], func[MU_TLS_SERVER], argv);
  client (sv[0], func[MU_TLS_CLIENT], argv);
  mu_stream_destroy (&outstream);

  clock_gettime (CLOCK_MONOTONIC, &ts_stop);
  rt = timespec_sub (ts_stop, ts_start);
  if (verbose_option)
    mu_diag_output (MU_DIAG_INFO, "%ld.09%ld", rt.tv_sec, rt.tv_nsec);

  return 0;
}
