#include <mailutils/mailutils.h>
#include <mailutils/locus.h>

int
main (int argc, char **argv)
{
  unsigned long max_lines;
  char *end;
  mu_locus_track_t trk;
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

  MU_ASSERT (mu_locus_track_create (&trk, argv[1], max_lines));
  while ((rc = mu_stream_getline (mu_strin, &buf, &size, &n)) == 0 && n > 0)
    {
      struct mu_locus_range lr;
      char *tok;
      
      n = mu_rtrim_class (buf, MU_CTYPE_SPACE);
      if (buf[0] == '\\')
	{
	  long x = strtol (buf+1, &end, 10);
	  if (*end || x == 0)
	    {
	      mu_error ("bad number");
	      continue;
	    }
	  mu_locus_tracker_retreat (trk, x);
	}
      else
	{
	  mu_c_str_unescape (buf, "\\\n", "\\n", &tok);
	  mu_locus_tracker_advance (trk, &lr, tok, strlen (tok));
	  free (tok);
	  mu_lrange_debug (&lr, "%s", buf);
	}
    }
  return 0;
}

  
