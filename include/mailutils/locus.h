#ifndef _MAILUTILS_LOCUS_H
#define _MAILUTILS_LOCUS_H

#include <string.h>
#include <stdarg.h>

struct mu_locus_point
{
  char const *mu_file;
  unsigned mu_line;
  unsigned mu_col;
};

#define MU_LOCUS_POINT_INITIALIZER { NULL, 0, 0 }

struct mu_locus_range
{
  struct mu_locus_point beg;
  struct mu_locus_point end;
};

#define MU_LOCUS_RANGE_INITIALIZER \
  { MU_LOCUS_POINT_INITIALIZER, MU_LOCUS_POINT_INITIALIZER }

typedef struct mu_linetrack *mu_linetrack_t;

struct mu_linetrack_stat
{
  unsigned start_line;  /* Start line number (1-based) */
  size_t n_lines;       /* Number of lines, including the recent (incomplete)
			   one */
  size_t n_chars;       /* Total number of characters */
};
  
int mu_ident_ref (char const *name, char const **refname);
int mu_ident_deref (char const *);

int mu_locus_point_set_file (struct mu_locus_point *pt, const char *filename);
int mu_locus_point_init (struct mu_locus_point *pt, const char *filename);
void mu_locus_point_deinit (struct mu_locus_point *pt);
int mu_locus_point_copy (struct mu_locus_point *dest,
			 struct mu_locus_point const *src);

void mu_locus_range_deinit (struct mu_locus_range *lr);

static inline int
mu_locus_point_same_file (struct mu_locus_point const *a,
			  struct mu_locus_point const *b)
{
  return a->mu_file == b->mu_file
         || (a->mu_file && b->mu_file && strcmp(a->mu_file, b->mu_file) == 0);
}

static inline int
mu_locus_point_same_line (struct mu_locus_point const *a,
			  struct mu_locus_point const *b)
{
  return mu_locus_point_same_file (a, b) && a->mu_line == b->mu_line;
}

void mu_lrange_debug (struct mu_locus_range const *loc,
		      char const *fmt, ...);

int mu_linetrack_create (mu_linetrack_t *ret,
			   char const *file_name, size_t max_lines);
void mu_linetrack_free (mu_linetrack_t trk);
void mu_linetrack_destroy (mu_linetrack_t *trk);
void mu_linetrack_advance (mu_linetrack_t trk,
			   struct mu_locus_range *loc,
			   char const *text, size_t leng);
int mu_linetrack_retreat (mu_linetrack_t trk, size_t n);

int mu_linetrack_locus (struct mu_linetrack *trk, struct mu_locus_point *lp);
int mu_linetrack_stat (mu_linetrack_t trk, struct mu_linetrack_stat *st);
int mu_linetrack_at_bol (struct mu_linetrack *trk);


void mu_stream_print_locus_range (mu_stream_t stream,
				  struct mu_locus_range const *loc);

void mu_stream_vlprintf (mu_stream_t stream,
			 struct mu_locus_range const *loc,
			 char const *fmt, va_list ap);
void mu_stream_lprintf (mu_stream_t stream,
			struct mu_locus_range const *loc,
			char const *fmt, ...);
void mu_lrange_debug (struct mu_locus_range const *loc,
		      char const *fmt, ...);


#endif
