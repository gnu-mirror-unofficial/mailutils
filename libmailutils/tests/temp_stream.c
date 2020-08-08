/*
 */
#include <mailutils/mailutils.h>
#include <mailutils/sys/stream.h>
#include <mailutils/sys/temp_stream.h>

#define MAXMEM 32

extern int mu_temp_stream_create (mu_stream_t *pstream, size_t max_size);

static void
verify (mu_stream_t str, int len)
{
  char buf[2*MAXMEM];
  int i;
  
  MU_ASSERT (mu_stream_seek (str, 0, MU_SEEK_SET, NULL));
  MU_ASSERT (mu_stream_read (str, buf, len, NULL));
  for (i = 0; i < len; i++)
    {
      if (buf[i] != i)
	{
	  mu_error ("bad octet %d: %d", i, buf[i]);
	  exit (1);
	}
    }
}

static int
is_file_backed_stream (mu_stream_t str)
{
  int state;
  return mu_stream_ioctl (str, MU_IOCTL_FD, MU_IOCTL_FD_GET_BORROW, &state)
         == 0;
}

int
main (int argc, char **argv)
{
  mu_stream_t str;
  char i;
  
  MU_ASSERT (mu_temp_stream_create (&str, MAXMEM));
  for (i = 0; i < MAXMEM; i++)
    {
      MU_ASSERT (mu_stream_write (str, &i, 1, NULL));
    }

  verify (str, MAXMEM);

  if (is_file_backed_stream (str))
    {
      mu_error ("stream switched to file backend too early");
      return 1;
    }

  MU_ASSERT (mu_stream_write (str, &i, 1, NULL));
  ++i;
  if (!is_file_backed_stream (str))
    {
      mu_error ("stream failed to switch to file backend");
      return 1;
    }
      
  for (; i < 2*MAXMEM; i++)
    {
      MU_ASSERT (mu_stream_write (str, &i, 1, NULL));
    }

  verify (str, 2*MAXMEM);

  mu_stream_destroy (&str);

  return 0;
}
