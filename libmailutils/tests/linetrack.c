#include <mailutils/mailutils.h>
#include <mailutils/locus.h>
#include "tesh.h"

static int
getnum (char const *arg, unsigned *ret)
{
  char *end;
  unsigned long x = strtoul (arg, &end, 10);
  if (*end)
    {
      mu_error ("bad number: %s", arg);
      return -1;
    }
  *ret = x;
  return 0;
}

static int
com_retreat (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_linetrack_t trk = env;
  unsigned x;
  if (getnum (argv[1], &x) == 0)
    {
      int rc = mu_linetrack_retreat (trk, x);
      if (rc == ERANGE)
	mu_error ("retreat count too big");
      else if (rc)
	mu_diag_funcall (MU_DIAG_ERROR, "mu_linetrack_retreat", argv[1], rc);
    }
  return 0;
}

static int
com_origin (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_linetrack_t trk = env;
  int rc;
  struct mu_locus_point pt;

  pt.mu_file = argv[1];
  if (getnum (argv[2], &pt.mu_line))
    return 0;
  if (getnum (argv[3], &pt.mu_col))
    return 0;
  rc = mu_linetrack_origin (trk, &pt);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_linetrack_origin", NULL, rc);
  return 0;
}

static int
com_line (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_linetrack_t trk = env;
  int rc;
  struct mu_locus_point pt = MU_LOCUS_POINT_INITIALIZER;

  if (getnum (argv[1], &pt.mu_line))
    return 0;
  rc = mu_linetrack_origin (trk, &pt);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_linetrack_origin", NULL, rc);
  return 0;
}

static int
com_rebase (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_linetrack_t trk = env;
  int rc;
  struct mu_locus_point pt;

  pt.mu_file = argv[1];
  if (getnum (argv[2], &pt.mu_line))
    return 0;
  if (getnum (argv[3], &pt.mu_col))
    return 0;
  rc = mu_linetrack_rebase (trk, &pt);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_linetrack_rebase", NULL, rc);
  return 0;
}

static int
com_point (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_linetrack_t trk = env;
  struct mu_locus_range lr = MU_LOCUS_RANGE_INITIALIZER;
  int rc;
  
  rc = mu_linetrack_locus (trk, &lr.beg);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_linetrack_locus", NULL, rc);
  else
    {
      mu_stream_lprintf (mu_strout, &lr, "%s\n", argv[0]);
      mu_locus_range_deinit (&lr);
    }
  return 0;
}

static int
com_bol_p (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_linetrack_t trk = env;
  mu_printf ("%d\n", mu_linetrack_at_bol (trk));
  return 0;
}

static int
com_stat (int argc, char **argv, mu_assoc_t options, void *env)
{
  mu_linetrack_t trk = env;
  int rc;
  struct mu_linetrack_stat st;
  
  rc = mu_linetrack_stat (trk, &st);
  if (rc)
    mu_diag_funcall (MU_DIAG_ERROR, "mu_linetrack_stat", NULL, rc);
  else
    {
      mu_printf ("n_files=%zu\n", st.n_files);
      mu_printf ("n_lines=%zu\n", st.n_lines);
      mu_printf ("n_chars=%zu\n", st.n_chars);
    }
  return 0;
}

static int
lineproc (int argc, char **argv, mu_assoc_t options, void *env)
{
  char *buf = argv[0];
  mu_linetrack_t trk = env;
  struct mu_locus_range lr = MU_LOCUS_RANGE_INITIALIZER;
  char *tok;

  if (buf[0] == 0)
    return 0;
  if (buf[0] == '.')
    {
      /* command escape */
      memmove (buf, buf + 1, strlen (buf));
      return MU_ERR_USER0;
    }
  
  mu_c_str_unescape (buf, "\\\n", "\\n", &tok);
  mu_linetrack_advance (trk, &lr, tok, strlen (tok));
  free (tok);
  mu_stream_lprintf (mu_strout, &lr, "%s\n", buf);
  mu_locus_range_deinit (&lr);
  return 0;
}


static struct mu_tesh_command comtab[] = {
  { "__LINEPROC__", "", lineproc },
  { "retreat",   "COUNT", com_retreat },
  { "origin",    "FILE LINE COL", com_origin },
  { "line",      "NUMBER", com_line },
  { "point",     "NUMBER", com_point },
  { "rebase",    "FILE LINE COL", com_rebase },
  { "bol",       "", com_bol_p },
  { "stat",      "", com_stat },
  { NULL }
};

int
main (int argc, char **argv)
{
  unsigned long max_lines;
  char *end;
  mu_linetrack_t trk;

  mu_tesh_init (argv[0]);
  if (argc != 3)
    {
      mu_error ("usage: %s FILE LINES", mu_program_name);
      return 1;
    }
  max_lines = strtoul (argv[2], &end, 10);
  if (*end || max_lines == 0)
    {
      mu_error ("invalid number of lines");
      return 1;
    }

  MU_ASSERT (mu_linetrack_create (&trk, argv[1], max_lines));

  mu_tesh_read_and_eval (argc - 3, argv + 3, comtab, trk);

  mu_linetrack_destroy (&trk);
  return 0;
}

  
