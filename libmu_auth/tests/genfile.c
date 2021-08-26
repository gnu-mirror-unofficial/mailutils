#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>

char const *progname;

static void
usage (FILE *fp)
{
  fprintf (fp, "usage: %s [-hp] [-f FILE] SIZE\n", progname);
}

int
main (int argc, char **argv)
{
  char *suf;
  unsigned long length, i;
  int c;
  char *filename = NULL;
  int printable = 0;
  FILE *fp;
  unsigned char ch;

  progname = argv[0];
  while ((c = getopt (argc, argv, "hf:p")) != EOF)
    {
      switch (c)
	{
	case 'f':
	  filename = optarg;
	  break;

	case 'p':
	  printable = 1;
	  break;
	  
	case 'h':
	  usage (stdout);
	  exit (0);

	default:
	  exit (1);
	}
    }

  if (optind + 1 != argc)
    {
      usage (stderr);
      exit (1);
    }

  length = strtoul (argv[optind], &suf, 10);
  if (suf[0] && suf[1])
    {
      fprintf (stderr, "%s: unknown size suffix: %s\n", progname, suf);
      exit (1);
    }

# define KMUL(n)							\
  do									\
    {									\
      if (UINT_MAX / 1024 < n)						\
	{								\
	  fprintf (stderr, "%s: size out of allowed range\n", progname); \
	  exit (1);							\
	}								\
      n *= 1024;							\
    }									\
  while (0)
    
  switch (*suf)
    {
    case 0:
      break;
    case 'g':
    case 'G':
      KMUL (length);
    case 'm':
    case 'M':
      KMUL (length);
    case 'k':
    case 'K':
      KMUL (length);
      break;
    default:
      fprintf (stderr, "%s: unknown size suffix: %s\n", progname, suf);
      exit (1);
    }

  if (filename)
    {
      fp = fopen (filename, "w");
      if (!fp)
	{
	  perror (filename);
	  exit (1);
	}
    }
  else
    fp = stdout;

  ch = 0;
  for (i = 0; i < length; i++)
    {
      while (printable && !isprint (ch))
	ch = (ch + 1) % 128;
      fputc (ch, fp);
      ch++;
    }

  if (ferror (fp))
    {
      fprintf (stderr, "%s: write error\n", progname);
      exit (1);
    }
  fclose (fp);
  exit (0);
}
