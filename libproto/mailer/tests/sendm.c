/*
 * NAME
 *   sendm - send a message sequentially to several recipients in multiple
 *           transactions
 *
 * SYNOPSIS
 *   sendm MAILER_URL FILE RCPT [RCPT...]
 *
 * DESCRIPTION
 *   Creates a mailer as requested by MAILER_URL.  Reads email message
 *   from the FILE and sends it sequentially to each RCPT from the command
 *   line.
 *
 *   A new transaction is opened for each message.
 *
 * LICENCE
 *   Copyright (C) 2020 Free Software Foundation, inc.
 *   License GPLv3+: GNU GPL version 3 or later
 *   <http://gnu.org/licenses/gpl.html>
 *   This is free software: you are free to change and redistribute it.
 *   There is NO WARRANTY, to the extent permitted by law.
 */
#include <config.h>
#include <mailutils/mailutils.h>

int
main (int argc, char **argv)
{
  mu_mailer_t mailer;
  mu_stream_t str;
  mu_message_t msg;
  char const *mailer_url, *filename;
  int i;
  int rc;
  static struct mu_address hint = { .domain = "localhost" };
  static int hflags = MU_ADDR_HINT_DOMAIN;
  
  mu_set_program_name (argv[0]);
  mu_register_all_mailer_formats ();

  if (argc < 4)
    abort ();
  mailer_url = argv[1];
  filename = argv[2];

  if ((rc = mu_mailer_create (&mailer, mailer_url)) != 0)
    {
      mu_diag_funcall (MU_DIAG_CRIT, "mu_mailer_create", mailer_url, rc);
      return 1;
    }

  if ((rc = mu_file_stream_create (&str, filename, MU_STREAM_READ)) != 0)
    {
      mu_diag_funcall (MU_DIAG_CRIT, "mu_file_stream_create", filename, rc);
      return 1;
    }
    
  if ((rc = mu_stream_to_message (str, &msg)) != 0)
    {
      mu_diag_funcall (MU_DIAG_CRIT, "mu_stream_to_message", filename, rc);
      return 1;
    }
      
  mu_stream_unref (str);

  for (i = 3; i < argc; i++)
    {
      mu_address_t rcpt;

      if ((rc = mu_address_create_hint (&rcpt, argv[i], &hint, hflags)) != 0)
	{
	  mu_diag_funcall (MU_DIAG_CRIT, "mu_address_create", argv[i], rc);
	  return 1;
	}
      
      if ((rc = mu_mailer_open (mailer, MU_STREAM_RDWR)) != 0)
	{
	  mu_diag_funcall (MU_DIAG_CRIT, "mu_mailer_open", NULL, rc);
	  return 1;
	}
	  
      if ((rc = mu_mailer_send_message (mailer, msg, NULL, rcpt)) != 0)
	{
	  mu_diag_funcall (MU_DIAG_CRIT, "mu_mailer_send_message", argv[i], rc);
	  return 1;
	}	

      mu_mailer_close (mailer);
      
      mu_address_destroy (&rcpt);
    }
  mu_message_unref (msg);
  mu_mailer_destroy (&mailer);
  return 0;
}
