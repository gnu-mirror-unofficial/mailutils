#include <stdlib.h>
#include <mailutils/alloc.h>
#include <mailutils/property.h>
#include <mailutils/diag.h>
#include <mailutils/mh.h>

#define MBOP_RECORD mu_mh_record
#define MBOP_SCHEME "mh"
#define MBOP_PRE_OPEN_HOOK mbop_pre_open_hook
static void
mbop_pre_open_hook (void)
{
  char *env = getenv ("MH");
  if (env && env[0])
    {
      struct mu_mh_prop *mhprop;
      int rc;
  
      mhprop = mu_zalloc (sizeof (mhprop[0]));
      mhprop->filename = env;
      mhprop->ro = 0;
      rc = mu_property_create_init (&mu_mh_profile, mu_mh_property_init, mhprop);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_property_create_init", env, rc);
	  exit (1);
	}
    }
}
#include "testsuite/mbop.c"
