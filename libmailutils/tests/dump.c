/* Simple hex dumper. */
#include <stdio.h>
#include <string.h>
#include <mailutils/cctype.h>

enum {
  /* Nibbles per hex byte: */
  HEXLEN      = 2,
  /* Number of characters to dump per line: */
  NDUMP       = 16,
  /* Emit extra whitespace in the middle of the line: */
  EXTRAOFF    = ((NDUMP / 2) - 1),
  /* Start of literal output: */
  LITOFF      = ((HEXLEN + 1) * NDUMP + 2),
  /* Size of the required buffer: add one character for extra whitespace
     in the middle of literal output part, and one more for the trailing \n */
  DUMPBUFSIZE = (LITOFF+NDUMP+2)
};

static int
rtrim (char *str, int n)
{
  while (n > 0 && str[n-1] == ' ')
    n--;
  str[n] = '\n';
  return n;
}

int
main (int argc, char **argv)
{
  char vbuf[DUMPBUFSIZE];
  char *p, *q;
  int i;
  int c;
  int n;
  unsigned long off = 0;
  static char xchar[] = "0123456789ABCDEF";
  
#define REWIND {				\
    p = vbuf;					\
    q = vbuf + LITOFF;				\
    i = 0;					\
    memset (vbuf, ' ', DUMPBUFSIZE-1);		\
  }

  REWIND;
  while ((c = getchar ()) != EOF)
    {
      if (i == NDUMP)
	{
	  fprintf (stdout, "%08lX: ", off);
	  n = rtrim (vbuf, q - vbuf);
	  fwrite (vbuf, 1, n+1, stdout);
	  off += i;
	  REWIND;
	}

      *p++ = xchar[c>>4];
      *p++ = xchar[c&0xf];
      *p++ = ' ';

      *q++ = mu_isprint (c) ? c : '.';
      if (i == EXTRAOFF)
	{
	  *p++ = ' ';
	  *q++ = ' ';
	}
      i++;
    }
  if (i)
    {
      fprintf (stdout, "%08lX: ", off);
      n = rtrim (vbuf, q - vbuf);
      fwrite (vbuf, 1, n+1, stdout);
    }
  return 0;
}
      
