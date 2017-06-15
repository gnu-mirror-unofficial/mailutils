#include <mailutils/mailutils.h>
#include <mailutils/locus.h>

int
main (int argc, char **argv)
{
  unsigned long max_lines;
  char *end;
  mu_linetrack_t trk;
  int rc;
  char *buf = NULL;
  size_t size, n;
  
  mu_set_program_name (argv[0]);
  mu_stdstream_setup (MU_STDSTREAM_RESET_NONE);

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
  while ((rc = mu_stream_getline (mu_strin, &buf, &size, &n)) == 0 && n > 0)
    {
      struct mu_locus_range lr = MU_LOCUS_RANGE_INITIALIZER;
      char *tok;
      
      n = mu_rtrim_class (buf, MU_CTYPE_SPACE);
      if (n == 0)
	continue;
      if (buf[0] == '\\' && buf[1] == '-')
	{
	  long x = strtol (buf+2, &end, 10);
	  if (*end || x == 0)
	    {
	      mu_error ("bad number");
	      continue;
	    }
	  rc = mu_linetrack_retreat (trk, x);
	  if (rc == ERANGE)
	    mu_error ("retreat count too big");
	  else if (rc)
	    mu_diag_funcall (MU_DIAG_ERROR, "mu_linetrack_retreat", buf+2,
			     rc);
	}
      else
	{
	  mu_c_str_unescape (buf, "\\\n", "\\n", &tok);
	  mu_linetrack_advance (trk, &lr, tok, strlen (tok));
	  free (tok);
	  mu_stream_lprintf (mu_strout, &lr, "%s\n", buf);
	}
      mu_locus_range_deinit (&lr);
    }
  mu_linetrack_destroy (&trk);
  return 0;
}

  
