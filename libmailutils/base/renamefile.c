#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <mailutils/stream.h>
#include <mailutils/util.h>
#include <mailutils/diag.h>
#include <mailutils/error.h>
#include <mailutils/errno.h>
#include <mailutils/nls.h>

int
mu_rename_file (const char *oldpath, const char *newpath)
{
  int rc;

  if (rename (oldpath, newpath) == 0)
    return 0;

  if (errno == EXDEV)
    {
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
		(_("cannot rename %s to %s: %s"),
		 oldpath, newpath, mu_strerror (errno)));
      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_TRACE1,
		(_("attempting copy")));
      
      rc = mu_copy_file (oldpath, newpath, MU_COPY_MODE|MU_COPY_OWNER);
      if (rc == 0)
	{
	  if (unlink (oldpath))
	    {
	      mu_debug (MU_DEBCAT_STREAM, MU_DEBUG_ERROR,
			(_("copied %s to %s, but failed to remove the source: %s"),
			 oldpath, newpath, mu_strerror (errno)));
	    }
	}
    }
  return rc;
}
