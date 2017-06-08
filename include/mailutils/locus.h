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

struct mu_locus_range
{
  struct mu_locus_point beg;
  struct mu_locus_point end;
};

typedef struct mu_locus_track *mu_locus_track_t;

struct mu_locus_track_stat
{
  unsigned start_line;
  size_t n_lines;
  size_t n_chars;
};
  
int mu_ident_ref (char const *name, char const **refname);
int mu_ident_deref (char const *);

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

int mu_locus_track_create (mu_locus_track_t *ret,
			   char const *file_name, size_t max_lines);
void mu_locus_track_free (mu_locus_track_t trk);
void mu_locus_track_destroy (mu_locus_track_t *trk);
size_t mu_locus_track_level (mu_locus_track_t trk);
void mu_locus_tracker_advance (struct mu_locus_track *trk,
			       struct mu_locus_range *loc,
			       char const *text, size_t leng);
int mu_locus_tracker_retreat (struct mu_locus_track *trk, size_t n);
int mu_locus_tracker_stat (struct mu_locus_track *trk,
			   struct mu_locus_track_stat *st);


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
