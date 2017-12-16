/* Simple LF -> CRLF filter */
#include <stdio.h>

int
main (void)
{
  int c;
  while ((c = getchar ()) != EOF)
    {
      if (c == '\n')
	putchar ('\r');
      putchar (c);
    }
  return 0;
}

		 
