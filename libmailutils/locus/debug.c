#include <stdlib.h>
#include <stdarg.h>
#include <mailutils/types.h>
#include <mailutils/locus.h>
#include <mailutils/error.h>
#include <mailutils/errno.h>
#include <mailutils/diag.h>
#include <mailutils/stream.h>
#include <mailutils/stdstream.h>

void
mu_stream_print_locus_range (mu_stream_t stream,
			     struct mu_locus_range const *loc)
{
  if (loc->beg.mu_col == 0)
    {
      if (loc->end.mu_file
	  && (!mu_locus_point_same_file (&loc->beg, &loc->end)
	      || loc->beg.mu_line != loc->end.mu_line))
	mu_stream_printf (stream, "%s:%u-%u",
			  loc->beg.mu_file,
			  loc->beg.mu_line,
			  loc->end.mu_line);
      else
	mu_stream_printf (stream, "%s:%u",
			  loc->beg.mu_file,
			  loc->beg.mu_line);
    }
  else
    {
      if (loc->end.mu_file
	  && !mu_locus_point_same_file (&loc->beg, &loc->end))
	mu_stream_printf (stream, "%s:%u.%u-%s:%u.%u",
			  loc->beg.mu_file,
			  loc->beg.mu_line, loc->beg.mu_col,
			  loc->end.mu_file,
			  loc->end.mu_line, loc->end.mu_col);
      else if (loc->end.mu_file && loc->beg.mu_line != loc->end.mu_line)
	mu_stream_printf (stream, "%s:%u.%u-%u.%u",
			  loc->beg.mu_file,
			  loc->beg.mu_line, loc->beg.mu_col,
			  loc->end.mu_line, loc->end.mu_col);
      else if (loc->end.mu_file && loc->beg.mu_col != loc->end.mu_col)
	mu_stream_printf (stream, "%s:%u.%u-%u",
			  loc->beg.mu_file,
			  loc->beg.mu_line, loc->beg.mu_col,
			  loc->end.mu_col);
      else
	mu_stream_printf (stream, "%s:%u.%u",
			  loc->beg.mu_file,
			  loc->beg.mu_line, loc->beg.mu_col);
    }
}

void
mu_stream_vlprintf (mu_stream_t stream,
		    struct mu_locus_range const *loc,
		    char const *fmt, va_list ap)
{
  mu_stream_print_locus_range (stream, loc);
  mu_stream_write (stream, ": ", 2, NULL);
  mu_stream_vprintf (mu_strerr, fmt, ap);
  mu_stream_write (stream, "\n", 1, NULL);
}

void
mu_stream_lprintf (mu_stream_t stream,
		   struct mu_locus_range const *loc,
		   char const *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  mu_stream_vlprintf (stream, loc, fmt, ap);
  va_end (ap);
}

void
mu_lrange_debug (struct mu_locus_range const *loc,
		 char const *fmt, ...)
{
  va_list ap;
  int rc, mode;

  rc = mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
			MU_IOCTL_LOGSTREAM_GET_MODE, &mode);
  if (rc == 0)
    {
      int new_mode = mode & ~MU_LOGMODE_LOCUS;
      rc = mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
			    MU_IOCTL_LOGSTREAM_SET_MODE, &new_mode);
    }

  va_start (ap, fmt);
  mu_stream_lprintf (mu_strerr, loc, fmt, ap);
  va_end (ap);

  if (rc == 0)
    mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
		     MU_IOCTL_LOGSTREAM_SET_MODE, &mode);
  
}
