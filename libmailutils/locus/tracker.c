#include <stdlib.h>
#include <errno.h>
#include <mailutils/types.h>
#include <mailutils/locus.h>
#include <mailutils/error.h>

struct mu_locus_track
{
  char const *file_name;     /* Name of the source file */
  size_t max_lines;          /* Max. number of lines history kept by tracker */
  size_t head;               /* Bottom of stack */
  size_t level;              /* Number of elements on stack */
  unsigned hline;            /* Number of line corresponding to cols[head] */
  unsigned *cols;            /* Cyclic stack */
};

int
mu_locus_track_create (mu_locus_track_t *ret,
		       char const *file_name, size_t max_lines)
{
  int rc;
  struct mu_locus_track *trk;
  
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
  trk->level = 0;
  trk->hline = 1;
  trk->cols[0] = 0;
  
  *ret = trk;
  return 0;
}

void
mu_locus_track_free (mu_locus_track_t trk)
{
  if (trk)
    {
      mu_ident_deref (trk->file_name);
      free (trk->cols);
      free (trk);
    }
}

void
mu_locus_track_destroy (mu_locus_track_t *trk)
{
  if (trk)
    {
      mu_locus_track_free (*trk);
      *trk = NULL;
    }
}   

size_t
mu_locus_track_level (mu_locus_track_t trk)
{
  return trk->level;
}

static inline unsigned *
cols_tos_ptr (mu_locus_track_t trk)
{
  return &trk->cols[(trk->head + trk->level) % trk->max_lines];
}

static inline unsigned
cols_peek (mu_locus_track_t trk, size_t n)
{
  return trk->cols[(trk->head + n) % trk->max_lines];
}

static inline unsigned *
push (mu_locus_track_t trk)
{
  unsigned *ptr;
  if (trk->level == trk->max_lines)
    {
      trk->head++;
      trk->hline++;
    }
  else
    trk->level++;
  *(ptr = cols_tos_ptr (trk)) = 0;
  return ptr;
}

static inline unsigned *
pop (mu_locus_track_t trk)
{
  if (trk->level == 0)
    return NULL;
  trk->level--;
  return cols_tos_ptr (trk);
}

#ifndef SIZE_MAX
# define SIZE_MAX (~((size_t)0))
#endif

int
mu_locus_tracker_stat (struct mu_locus_track *trk,
		       struct mu_locus_track_stat *st)
{
  size_t i, nch = 0;

  for (i = 0; i <= trk->level; i++)
    {
      unsigned n = cols_peek (trk, i);
      if (SIZE_MAX - nch < n)
	return ERANGE;
      nch += n;
    }
  
  st->start_line = trk->hline;
  st->n_lines = trk->level;
  st->n_chars = nch;
}
    
void
mu_locus_tracker_advance (struct mu_locus_track *trk,
			  struct mu_locus_range *loc,
			  char const *text, size_t leng)
{
  unsigned *ptr;

  if (text == NULL || leng == 0)
    return;
  
  loc->beg.mu_file = loc->end.mu_file = trk->file_name;
  loc->beg.mu_line = trk->hline + trk->level;
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
      loc->end.mu_line = trk->hline + trk->level;
      loc->end.mu_col = *ptr;
    }
  else
    {
      /* Text ends with a newline.  Keep the previos line number. */
      loc->end.mu_line = trk->hline + trk->level - 1;
      loc->end.mu_col = cols_peek (trk, trk->level - 1) - 1;
      if (loc->end.mu_col + 1 == loc->beg.mu_col)
	{
	  /* This happens if the previous line contained only newline. */
	  loc->beg.mu_col = loc->end.mu_col;
	}	  
   }
}

int
mu_locus_tracker_retreat (struct mu_locus_track *trk, size_t n)
{
  struct mu_locus_track_stat st;

  mu_locus_tracker_stat (trk, &st);
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
    
  
