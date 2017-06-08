#include <stdlib.h>
#include <errno.h>
#include <mailutils/types.h>
#include <mailutils/locus.h>
#include <mailutils/error.h>

/* The line-tracker structure keeps track of the last N lines read from a
   text input file.  For each line read it keeps the number of characters
   in that line including the newline.  This information is stored in a
   syclic stack of N elements.  Top of stack always represents the current
   line.  For the purpose of line tracker, current line is the line that is
   being visited, such that its final newline character has not yet been
   seen.  Once the newline is seen, the line is pushed on stack, and a new
   current line is assumed.

   The value of N must not be less than 2.
*/
struct mu_linetrack
{
  char const *file_name; /* Name of the source file */
  size_t max_lines;      /* Max. number of lines history kept by tracker (N) */
  size_t head;           /* Index of the eldest element on stack */
  size_t tos;            /* Index of the most recent element on stack
			    (< max_lines) */
  unsigned hline;        /* Number of line corresponding to cols[head] */
  unsigned *cols;        /* Cyclic stack or character counts.
			    Number of characters in line (hline + n) is
			    cols[head + n] (0 <= n <= tos). */
};

int
mu_linetrack_create (mu_linetrack_t *ret,
		       char const *file_name, size_t max_lines)
{
  int rc;
  struct mu_linetrack *trk;
  
  trk = malloc (sizeof *trk);
  if (!trk)
    return errno;
  trk->cols = calloc (max_lines, sizeof (trk->cols[0]));
  if (!trk->cols)
    {
      rc = errno;
      free (trk);
      return rc;
    }
  rc = mu_ident_ref (file_name, &trk->file_name);
  if (rc)
    {
      free (trk->cols);
      free (trk);
      return rc;
    }

  if (max_lines < 2)
    max_lines = 2;
  trk->max_lines = max_lines;
  trk->head = 0;
  trk->tos = 0;
  trk->hline = 1;
  trk->cols[0] = 0;
  
  *ret = trk;
  return 0;
}

void
mu_linetrack_free (mu_linetrack_t trk)
{
  if (trk)
    {
      mu_ident_deref (trk->file_name);
      free (trk->cols);
      free (trk);
    }
}

void
mu_linetrack_destroy (mu_linetrack_t *trk)
{
  if (trk)
    {
      mu_linetrack_free (*trk);
      *trk = NULL;
    }
}   

static inline unsigned *
cols_tos_ptr (mu_linetrack_t trk)
{
  return &trk->cols[(trk->head + trk->tos) % trk->max_lines];
}

static inline unsigned
cols_peek (mu_linetrack_t trk, size_t n)
{
  return trk->cols[(trk->head + n) % trk->max_lines];
}

static inline unsigned *
push (mu_linetrack_t trk)
{
  unsigned *ptr;
  if (trk->tos == trk->max_lines - 1)
    {
      trk->head++;
      trk->hline++;
    }
  else
    trk->tos++;
  *(ptr = cols_tos_ptr (trk)) = 0;
  return ptr;
}

static inline unsigned *
pop (mu_linetrack_t trk)
{
  if (trk->tos == 0)
    return NULL;
  trk->tos--;
  return cols_tos_ptr (trk);
}

#ifndef SIZE_MAX
# define SIZE_MAX (~((size_t)0))
#endif

int
mu_linetrack_stat (struct mu_linetrack *trk, struct mu_linetrack_stat *st)
{
  size_t i, nch = 0;

  for (i = 0; i <= trk->tos; i++)
    {
      unsigned n = cols_peek (trk, i);
      if (SIZE_MAX - nch < n)
	return ERANGE;
      nch += n;
    }
  
  st->start_line = trk->hline;
  st->n_lines = trk->tos + 1;
  st->n_chars = nch;

  return 0;
}
    
void
mu_linetrack_advance (struct mu_linetrack *trk,
		      struct mu_locus_range *loc,
		      char const *text, size_t leng)
{
  unsigned *ptr;

  if (text == NULL || leng == 0)
    return;
  
  loc->beg.mu_file = loc->end.mu_file = trk->file_name;
  loc->beg.mu_line = trk->hline + trk->tos;
  ptr = cols_tos_ptr (trk);
  loc->beg.mu_col = *ptr + 1;
  while (leng--)
    {
      (*ptr)++;
      if (*text == '\n')
	ptr = push (trk);
      text++;
    }
  if (*ptr)
    {
      loc->end.mu_line = trk->hline + trk->tos;
      loc->end.mu_col = *ptr;
    }
  else
    {
      /* Text ends with a newline.  Keep the previos line number. */
      loc->end.mu_line = trk->hline + trk->tos - 1;
      loc->end.mu_col = cols_peek (trk, trk->tos - 1) - 1;
      if (loc->end.mu_col + 1 == loc->beg.mu_col)
	{
	  /* This happens if the previous line contained only newline. */
	  loc->beg.mu_col = loc->end.mu_col;
	}	  
   }
}

int
mu_linetrack_retreat (struct mu_linetrack *trk, size_t n)
{
  struct mu_linetrack_stat st;

  mu_linetrack_stat (trk, &st);
  if (n > st.n_chars)
    return ERANGE;
  else
    {
      unsigned *ptr = cols_tos_ptr (trk);
      while (n--)
	{
	  if (*ptr == 0)
	    {
	      ptr = pop (trk);
	      if (!ptr || *ptr == 0)
		{
		  mu_error ("%s:%d: INTERNAL ERROR: out of pop back\n",
			    __FILE__, __LINE__);
		  return ERANGE;
		}
	    }
	  --*ptr;
	}
    }
  return 0;
}
    
  
