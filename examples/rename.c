#include <mailutils/mailutils.h>

int copy_option;
int owner_option;
int mode_option;

static struct mu_option rename_options[] = {
  { "copy", 'c', NULL, MU_OPTION_DEFAULT,
    "copy the file",
    mu_c_bool, &copy_option },
  { "owner", 'u', NULL, MU_OPTION_DEFAULT,
    "copy ownership",
    mu_c_bool, &owner_option },
  { "mode", 'm', NULL, MU_OPTION_DEFAULT,
    "copy mode",
    mu_c_bool, &mode_option },
  MU_OPTION_END
}, *options[] = { rename_options, NULL };
  
struct mu_cli_setup cli = {
  options,
  NULL,
  "copy or rename file",
  "SRC DST"
};

static char *capa[] = {
  "debug",
  NULL
};

int
main (int argc, char **argv)
{
  int rc;
  
  mu_cli (argc, argv, &cli, capa, NULL, &argc, &argv);

  if (argc != 2)
    {
      mu_error ("wrong number of arguments");
      return 1;
    }

  if (copy_option)
    {
      int flags = (owner_option ? MU_COPY_OWNER : 0)
	        | (mode_option ? MU_COPY_MODE : 0);
      rc = mu_copy_file (argv[0], argv[1], flags);
    }
  else
    rc = mu_rename_file (argv[0], argv[1]);

  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_rename_file", NULL, rc);

  return !!rc;
}

      
