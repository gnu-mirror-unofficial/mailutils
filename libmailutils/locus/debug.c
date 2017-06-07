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
  
  if (loc->beg.mu_col == 0)					       
    mu_debug_log_begin ("%s:%u", loc->beg.mu_file, loc->beg.mu_line);
  else if (!mu_locus_point_same_file (&loc->beg, &loc->end))
    mu_debug_log_begin ("%s:%u.%u-%s:%u.%u",
			loc->beg.mu_file,
			loc->beg.mu_line, loc->beg.mu_col,
			loc->end.mu_file,
			loc->end.mu_line, loc->end.mu_col);
  else if (loc->beg.mu_line != loc->end.mu_line)
    mu_debug_log_begin ("%s:%u.%u-%u.%u",
			loc->beg.mu_file,
			loc->beg.mu_line, loc->beg.mu_col,
			loc->end.mu_line, loc->end.mu_col);
  else if (loc->beg.mu_col != loc->end.mu_col)
    mu_debug_log_begin ("%s:%u.%u-%u",
			loc->beg.mu_file,
			loc->beg.mu_line, loc->beg.mu_col,
			loc->end.mu_col);
  else
    mu_debug_log_begin ("%s:%u.%u",
			loc->beg.mu_file,
			loc->beg.mu_line, loc->beg.mu_col);
  
  mu_stream_write (mu_strerr, ": ", 2, NULL);
  
  va_start (ap, fmt);
  mu_stream_vprintf (mu_strerr, fmt, ap);
  va_end (ap);
  mu_debug_log_nl ();
  if (rc == 0)
    mu_stream_ioctl (mu_strerr, MU_IOCTL_LOGSTREAM,
		     MU_IOCTL_LOGSTREAM_SET_MODE, &mode);
}
