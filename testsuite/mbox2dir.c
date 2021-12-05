/*
  NAME
    mbox2dir - convert mbox to maildir or MH format

  SYNOPSIS
    mbox2dir [-i FILE] [-h NAME] [-v VALUE] [-emnpu] PATH MBOX [NAMES...]

  DESCRIPTION
    Creates a maildir or (if used with the -m option) MH mailbox in PATH
    and populates it with the messages from the UNIX mbox file MBOX.

    When maildir output is requested, the following apply:
    
    If NAMES arguments are given, each successive message from MBOX
    is stored in the file named with the corresponding NAME.

    Otherwise, if the -i option is given, message names are read from
    FILE, which must list single message name on each line.

    In both cases, each name must begin with "new/" or "cur/" directory
    prefix.

    If nether -i, nor NAMES are given, message names will be generated
    automatically in the "cur" subdirectory in PATH.  Name generation
    is governed by three options.  The -n option instructs mbox2dir to
    store messages in "new", instead of "cur".  The -u option stores
    the UID of each message in the message file name.  Obviously, -n
    and -u cannot be used together.  The -h option supplies the hostname
    to use in names (default is "localhost").

    If MH output is requested (the -m option), the options -u, -h, and -n
    have no effect.  Message names supplied in FILE or command line must
    be positive integers.
    
  OPTIONS

    -e
       Read envelope information from the From_ line of each input
       message and store it in the pair of Return-Path and Received
       headers in the output file.
       
    -h NAME
       Hostname for use in maildir message names.
       
    -i FILE
       Read message file names from FILE (one name per line).  Convert
       as many messages as there are names in FILE.
       
    -m
       Create mailbox in MH format.
       
    -n
       Store messages in the "new" subdirectory.

    -p
       Create the ".mu-prop" file.  Unless -v is given (see below),
       the uidvalidity value will be set to the number of seconds since
       Epoch.
       
    -u
       Add UID to the message names.

    -v VALUE
       Set uidvalidity to VALUE.  Implies -p.

  AUTHOR
    Sergey Poznyakoff <gray@gnu.org>

  BUGS
    The program is not intended for production use, therefore these
    are rather features, and intentional ones, too.  Anyway:

    1. PATH may not exit.
    2. Buffer space for message name generation is of fixed size.
    3. Default hostname is "localhost", instead of the actual machine
       name.
    4. Random number generator is not initialized.
    5. If -e option is used, the time of the created Received: header
       is not in RFC 5322 format.
    
  LICENSE
    This program is part of GNU Mailutils testsuite.
    Copyright (C) 2020-2021 Free Software Foundation, Inc.

    Mbox2dir is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3, or (at your option)
    any later version.

    Mbox2dir is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sysexits.h>
#include <assert.h>
#include <ctype.h>
#include <sys/time.h>

static char *progname;

enum
  {
    F_MAILDIR,
    F_MH
  };

int format = F_MAILDIR;
int new_option;
int uid_option;
int prop_option;
int e_option;
char *hostname = "localhost";
char propfile[] = ".mu-prop";
unsigned long uidvalidity = 0;


enum
  {
    INPUT_UNDEF = -1,
    INPUT_GENERATE,
    INPUT_ARGV,
    INPUT_FILE
  };

typedef struct name_input
{
  int type;
  union
  {
    struct             /* Structure for INPUT_FILE */
    {
      FILE *file;      /* Input file */
      char *bufptr;    /* Input buffer */
      size_t bufsize;  /* Buffer size */
    };
    struct             /* Structure for INPUT_ARGV */
    {
      int argc;        /* Number of arguments */
      char **argv;     /* Argument vector */
      int idx;         /* Index of the next argument in argv */
    };
    unsigned long uid; /* Last assigned UID - for INPUT_GENERATE */
  };
} NAME_INPUT;


struct format_info
{
  void (*init_dir) (int, char const *);
  char *(*generate_name) (NAME_INPUT *);
  void (*validate_name) (char const *);
};

static char *mh_generate_name (NAME_INPUT *);
static char *maildir_generate_name (NAME_INPUT *);
static void maildir_init_dir (int, char const *);
static void mh_validate_name (char const *name);
static void maildir_validate_name (char const *name);

static struct format_info format_info[] = {
  { maildir_init_dir, maildir_generate_name, maildir_validate_name },
  { NULL, mh_generate_name, mh_validate_name },
};

static void
mh_validate_name (char const *name)
{
  if (name [strspn (name, "0123456789")])
    {
      fprintf (stderr, "%s: name must be numeric: %s\n",
	       progname, name);
      exit (EX_DATAERR);
    }
}

static char *
mh_generate_name (NAME_INPUT *input)
{
  unsigned long n = input->uid;
  char buf[11];
  //  char dig[] = "0123456789";
  char *p = buf + sizeof (buf);
  *--p = 0;
  do
    {
      if (p == buf)
	abort ();
      *--p = n % 10 + '0';
    }
  while ((n /= 10) != 0);
  return strdup (p);      
}

static char *subdirs[] = { "cur", "new", "tmp" };

static int
starts_with_subdir (char const *name)
{
  int i;
  size_t n = strlen (name);

  for (i = 0; i < sizeof (subdirs) / sizeof (subdirs[0]); i++)
    {
      size_t l = strlen (subdirs[i]);
      if (n > l + 1 && memcmp (name, subdirs[i], l) == 0 && name[l] == '/')
	return 1;
    }
  return 0;
}

static void
maildir_init_dir (int fd, char const *name)
{
  int i;
      
  for (i = 0; i < sizeof (subdirs) / sizeof (subdirs[0]); i++)
    {
      if (mkdirat (fd, subdirs[i], 0755))
	{
	  fprintf (stderr, "%s: can't create %s/%s: %s\n",
		   progname, name, subdirs[i], strerror (errno));
	  exit (EX_CANTCREAT);
	}
    }
}

static char *
maildir_generate_name (NAME_INPUT *input)
{
  char buf[4096];
  size_t i = 0;
  struct timeval tv;

  i += snprintf (buf + i, sizeof (buf) - i, "%s/", new_option ? "new" : "cur");
  assert (i < sizeof (buf));
  
  gettimeofday (&tv, NULL);
  i += snprintf (buf + i, sizeof (buf) - i, "%lu", (unsigned long) tv.tv_sec);
  assert (i < sizeof (buf));

  i += snprintf (buf + i, sizeof (buf) - i, ".R%lu", random ());
  assert (i < sizeof (buf));

  i += snprintf (buf + i, sizeof (buf) - i, "M%lu", (unsigned long) tv.tv_usec);
  assert (i < sizeof (buf));

  i += snprintf (buf + i, sizeof (buf) - i, "P%lu", (unsigned long) getpid ());
  assert (i < sizeof (buf));

  i += snprintf (buf + i, sizeof (buf) - i, "Q%lu", input->uid-1);
  assert (i < sizeof (buf));

  i += snprintf (buf + i, sizeof (buf) - i, ".%s", hostname);
  assert (i < sizeof (buf));

  if (uid_option)
    {
      i += snprintf (buf + i, sizeof (buf) - i, ",u=%lu", input->uid);
      assert (i < sizeof (buf));
    }
  if (!new_option)
    {
      i += snprintf (buf + i, sizeof (buf) - i, ":2,");
      assert (i < sizeof (buf));
    }      
  return strdup (buf);
}

static void
maildir_validate_name (char const *name)
{
  if (!starts_with_subdir (name))
    {
      fprintf (stderr, "%s: name must start with a subdir: %s\n",
	       progname, name);
      exit (EX_DATAERR);
    }
}

static char *
next_name_from_argv (NAME_INPUT *input)
{
  if (input->idx == input->argc)
    return NULL;
  return input->argv[input->idx++];
}

static char *
next_name_from_file (NAME_INPUT *input)
{
  ssize_t off = 0;
  do
    {
      if (off + 1 >= input->bufsize)
	{
	  size_t size;
	  char *buf;
	  if (input->bufsize == 0)
	    {
	      size = 64;
	    }
	  else
	    {
	      if ((size_t) -1 / 3 * 2 <= size)
		{
		  fprintf (stderr, "%s: out of memory\n", progname);
		  exit (EX_OSERR);
		}
	      size += (size + 1) / 2;
	    }
	  buf = realloc (input->bufptr, size);
	  if (!buf)
	    {
	      fprintf (stderr, "%s: out of memory\n", progname);
	      exit (EX_OSERR);
	    }
	  input->bufptr = buf;
	  input->bufsize = size;
	}
      if (!fgets(input->bufptr + off, input->bufsize - off, input->file))
	{
	  if (feof (input->file))
	    {
	      if (off == 0)
		return NULL;
	      break;
	    }
	  else
	    {
	      fprintf (stderr, "%s: read error: %s\n",
		       progname, strerror (errno));
	      exit (EX_OSERR);
	    }	      
        }
      off += strlen(input->bufptr + off);
    }
  while (input->bufptr[off - 1] != '\n');
  input->bufptr[--off] = 0;
  return input->bufptr;
}

static char *
next_name_generate (NAME_INPUT *input)
{
  ++input->uid;
  return format_info[format].generate_name(input);
}

static char *
next_name (NAME_INPUT *input)  
{
  static char *(*nextfn[]) (NAME_INPUT *) = {
    next_name_generate,
    next_name_from_argv,
    next_name_from_file
  };
  return nextfn[input->type] (input);
}

static void
mkdir_r (char *name)
{
  size_t namelen = strlen (name);
  size_t i;
  int rc = 0;
  
  while (mkdir (name, 0755))
    {
      if (errno == ENOENT)
	{
	  char *p = strrchr (name, '/');
	  if (!p)
	    abort ();
	  *p = 0;
	}
      else
	{
	  fprintf (stderr, "%s: can't create %s: %s\n",
		   progname, name, strerror (errno));
	  rc = 1;
	  break;
	}
    }

  while ((i = strlen (name)) < namelen)
    {
      name[i] = '/';
      if (rc)
	continue;
      if (mkdir (name, 0755))
	{
	  fprintf (stderr, "%s: can't create %s: %s\n",
		   progname, name, strerror (errno));
	  rc = 1;
	}
    }

  if (rc)
    {
      fprintf (stderr, "%s: while attempting to create %s\n",
	       progname, name);
      exit (EX_CANTCREAT);
    }
}

static void
validate_name (char const *name)
{
  return format_info[format].validate_name (name);
}
      

static int
mkhier (char *name)
{
  int fd;
  
  mkdir_r (name);
  fd = open (name, O_RDONLY | O_NONBLOCK | O_DIRECTORY);
  if (fd == -1)
    {
      fprintf (stderr, "%s: can't open directory %s: %s\n",
	       progname, name, strerror (errno));
      exit (EX_UNAVAILABLE);
    }

  if (format_info[format].init_dir)
    format_info[format].init_dir (fd, name);
  
  return fd;
}

#define __cat2__(a,b) a ## _ ## b
#define DCL_H(n,s)				\
  static char __cat2__(n,h)[] = s;		\
  static size_t __cat2__(n,l) = sizeof (__cat2__(n,h)) - 1

DCL_H (from, "From ");

static int
is_valid_mbox (FILE *in)
{
  char buf[sizeof (from_h)];

  if (fread (buf, from_l, 1, in) == 1)
    return memcmp (buf, from_h, from_l) == 0;
  else if (ferror (in))
    {
      fprintf (stderr, "%s: read error: %s\n",
	       progname, strerror (errno));
      exit (EX_IOERR);
    }
  return 0;
}

/*
 * Copy single message from IN to OUT.  If e_option is set, omit eventual
 * Return-Path header.
 * On input, file pointer in IN must point past the From_ line.
 * On output, file pointer is left at the beginning of the next From_ line.
 * Return value: 0 if there are more messages to copy and -1 otherwise.
 */
static int
copy_message (FILE *in, FILE *out)
{
  DCL_H (return_path, "return-path:");
  char buf[sizeof (return_path_h)];
  int c = 0, la = 0;
  enum { HDR, HLINE, HSKIP, BODY, BNL } state = HDR;

  while (1)
    {
      if (c != 0)
	fputc (c, out);
      if (la != 0)
	{
	  c = la;
	  la = 0;
	}
      else
	c = fgetc (in);
      
      switch (state)
	{
	case HDR:
	  if (c == EOF)
	    return -1;
	  if (c == '\n')
	    state = BODY;
	  else if (e_option && tolower (c) == return_path_h[0])
	    {
	      buf[0] = c;
	      if (fread (buf + 1, return_path_l - 1, 1, in) == 1)
		{
		  if (strncasecmp (buf, return_path_h, return_path_l) == 0)
		    {
		      state = HSKIP;
		    }
		  else
		    {
		      if (fwrite (buf, return_path_l, 1, out) != 1)
			return -1;
		      state = HLINE;
		    }
		  c = 0;
		}
	      else if (ferror (in) || feof (in))
		return -1;
	      else
		state = HLINE;
	    }
	  else
	    state = HLINE;
	  break;

	case HLINE:
	  if (c == EOF)
	    return -1;
	  if (c == '\n')
	    state = HDR;
	  break;

	case HSKIP:
	  if (c == EOF)
	    return -1;
	  if (c == '\n')
	    state = HDR;
	  c = 0;
	  break;

	case BODY:
	  if (c == EOF)
	    return -1;
	  if (c == '\n')
	    {
	      state = BNL;
	      c = 0;
	    }
	  break;

	case BNL:
	  if (c == EOF)
	    {
	      fputc ('\n', out);
	      return -1;
	    }
	  if (c != '\n')
	    {
	      if (c == from_h[0])
		{
		  buf[0] = c;
		  if (fread (buf + 1, from_l - 1, 1, in) == 1)
		    {
		      if (memcmp (buf, from_h, from_l) == 0)
			return 0; /* start of next message */
		      else
			{
			  fputc ('\n', out);
			  if (fwrite (buf, from_l, 1, out) != 1)
			    return -1;
			}
		    }
		  else if (ferror (in) || feof (in))
		    return -1;
		  c = 0;
		}
	      else
		fputc ('\n', out);
	      state = BODY;
	    }
	  break;

	default:
	  abort ();
	}
    }
}

static void
skip_line (FILE *fp)
{
  int c;
  while ((c = fgetc (fp)) != EOF && c != '\n')
    ;
}
    
static int
convert_envelope (FILE *in, FILE *out)
{
  DCL_H (return_path, "Return-Path: ");
  DCL_H (received, "Received: from localhost\n\tby localhost; ");
  int c;

  if (!e_option)
    {
      skip_line (in);
      return 0;
    }
  
  fwrite (return_path_h, return_path_l, 1, out);
  while ((c = fgetc (in)) != EOF)
    {
      if (c == ' ' || c == '\t')
	break;
      fputc (c, out);
    }
  fputc ('\n', out);
  
  if (c == EOF)
    return -1;

  fwrite (received_h, received_l, 1, out);
  while ((c = fgetc (in)) != EOF)
    {
      fputc (c, out);
      if (c == '\n')
	return 0;
    }
  return -1;
}

static int
store_file (int dirfd, char const *name, FILE *input)
{
  FILE *fp;
  int rc;
  int fd;
  
  fd = openat (dirfd, name, O_CREAT | O_TRUNC | O_WRONLY, 0600);
  if (fd == -1)
    {
      fprintf (stderr, "%s: can't create file %s: %s\n",
	       progname, name, strerror (errno));
      exit (EX_CANTCREAT);
    }
  
  fp = fdopen (fd, "w");
  if (!fp)
    abort ();

  if (convert_envelope (input, fp))
    return -1;
  rc = copy_message (input, fp);
  fclose (fp);
  return rc;
}

static void
create_prop_file (int dirfd)
{
  int fd;
  FILE *fp;
  
  fd = openat (dirfd, propfile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
  if (fd == -1)
    {
      fprintf (stderr, "%s: can't create file %s: %s\n",
	       progname, propfile, strerror (errno));
      exit (EX_CANTCREAT);
    }
  fp = fdopen (fd, "w");
  if (!fp)
    abort ();
  if (uidvalidity == 0)
    {
      struct timeval tv;
      gettimeofday (&tv, NULL);
      uidvalidity = tv.tv_sec;
    }
  fprintf (fp, "version: %s\n", PACKAGE_VERSION);  
  fprintf (fp, "uid-validity: %lu\n", uidvalidity);
  fclose (fp);
}

static void
usage (FILE *fp)
{
  fprintf (fp, "usage: %s [-i FILE] [-h NAME] [-v UIDVALIDITY] [-enmpuv] PATH FILE [NAMES...]\n",
	   progname);
  fprintf (fp, "converts UNIX mbox file FILE to maildir or MH format.\n");
}

int
main (int argc, char **argv)
{
  NAME_INPUT input = { INPUT_UNDEF };
  char *name;
  FILE *fp;
  int fd;
  int c;
  
  progname = argv[0];

  while ((c = getopt (argc, argv, "ei:h:nmpuv:")) != EOF)
    {
      switch (c)
	{
	case 'e':
	  e_option = 1;
	  break;
	  
	case 'h':
	  hostname = optarg;
	  break;
	  
	case 'm':
	  format = F_MH;
	  break;

	case 'n':
	  new_option = 1;
	  break;

	case 'u':
	  uid_option = 1;
	  break;

	case 'p':
	  prop_option = 1;
	  break;

	case 'v':
	  uidvalidity = strtoul (optarg, NULL, 10);
	  prop_option = 1;
	  break;
	  
	case 'i':
	  if (strcmp (optarg, "-") == 0)
	    {
	      input.file = stdin;
	    }
	  else if ((input.file = fopen (optarg, "r")) == NULL)
	    {
	      fprintf (stderr, "%s: can't open input file %s: %s\n",
		       progname, optarg, strerror (errno));
	      exit (EX_OSERR);
	    }
	  input.type = INPUT_FILE;
	  input.bufptr = NULL;
	  input.bufsize = 0;
	  break;

	default:
	  usage (stderr);
	  exit (EX_USAGE);
	}
    }
  if (new_option)
    uid_option = 0;
  
  argc -= optind;
  argv += optind;
  
  switch (argc)
    {
    case 0:
    case 1:
      usage (stderr);
      exit (EX_USAGE);
      
    case 2:
      if (input.type == INPUT_UNDEF)
	{
	  input.type = INPUT_GENERATE;
	  input.uid = 0;
	}
      break;

      /* fall through */
    default:
      if (input.type == INPUT_FILE)
	{
	  fprintf (stderr, "%s: NAMES can't be given together with -i\n",
		   progname);
	  exit (EX_USAGE);
	}
      input.type = INPUT_ARGV;
      input.argc = argc - 2;
      input.argv = argv + 2;
      input.idx = 0;
      break;
    }

  fd = mkhier (argv[0]);

  fp = fopen (argv[1], "r");
  if (!fp)
    {
      fprintf (stderr, "%s: can't open %s: %s\n",
	       progname, argv[1], strerror (errno));
      exit (EX_OSERR);
    }

  if (!is_valid_mbox (fp))
    {
      fprintf (stderr, "%s: can't find any messages in %s\n",
	       progname, argv[1]);
      exit (EX_NOINPUT);
    }
  
  while ((name = next_name (&input)) != NULL)
    {
      validate_name (name);
      if (store_file (fd, name, fp))
	break;
    }
  if (input.type != INPUT_GENERATE && (name = next_name (&input)) != NULL)
    {
      fprintf (stderr, "%s: extra names ignored (started at %s)\n",
	       progname, name);
      free (name);
    }

  if (prop_option)
    create_prop_file (fd);
  
  return 0;
}
