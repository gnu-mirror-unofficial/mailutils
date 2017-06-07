#include <stdlib.h>
#include <mailutils/types.h>
#include <mailutils/assoc.h>
#include <mailutils/locus.h>
#include <mailutils/error.h>
#include <mailutils/errno.h>
#include <mailutils/diag.h>
#include <mailutils/list.h>

struct mu_ident_ref
{
  size_t count;
};

static mu_assoc_t nametab;

int
mu_ident_ref (char const *name, char const **refname)
{
  int rc;
  struct mu_ident_ref *ref, **refptr;
  
  if (!name)
    return EINVAL;
  if (!refname)
    return MU_ERR_OUT_PTR_NULL;
  
  if (!nametab)
    {
      rc = mu_assoc_create (&nametab, 0);
      if (rc)
	{
	  mu_diag_funcall (MU_DIAG_ERROR, "mu_assoc_create", NULL, rc);
	  return rc;
	}
      mu_assoc_set_destroy_item (nametab, mu_list_free_item);
    }
  rc = mu_assoc_install_ref2 (nametab, name, &refptr, refname);
  switch (rc)
    {
    case 0:
      ref = malloc (sizeof *ref);
      if (!ref)
	{
	  rc = errno;
	  mu_assoc_remove (nametab, name);
	  return rc;
	}
      ref->count = 0;
      break;
      
    case MU_ERR_EXISTS:
      ref = *refptr;
      break;
      
    default:
      mu_diag_funcall (MU_DIAG_ERROR, "mu_assoc_install_ref2", name, rc);
      return rc;
    }

  ref->count++;
  return 0;
}

int
mu_ident_deref (char const *name)
{
  struct mu_ident_ref *ref;
  int rc;

  if (!name)
    return EINVAL;
  if (!nametab)
    return 0;
  
  rc = mu_assoc_lookup (nametab, name, &ref);
  switch (rc)
    {
    case 0:
      if (--ref->count == 0)
	mu_assoc_remove (nametab, name);
      break;

    case MU_ERR_NOENT:
      break;

    default:
      mu_diag_funcall (MU_DIAG_ERROR, "mu_assoc_lookup", name, rc);
      return rc;
    }

  return 0;
}
  
    

