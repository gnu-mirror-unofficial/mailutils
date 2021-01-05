/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2005-2021 Free Software Foundation, Inc.

   GNU Mailutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Mailutils is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <mailutils/assoc.h>
#include <mailutils/header.h>
#include <mailutils/message.h>
#include <mailutils/mime.h>
#include <mailutils/iterator.h>
#include <mailutils/stream.h>
#include <mailutils/stdstream.h>
#include <mailutils/util.h>
#include <mailutils/cstr.h>
#include <mailutils/cctype.h>
#include <mailutils/error.h>
#include <mailutils/errno.h>
#include <mailutils/cli.h>

static int
sort_names (char const *aname, void const *adata,
	    char const *bname, void const *bdata, void *data)
{
  return mu_c_strcasecmp (aname, bname);
}

static int
print_param (const char *name, void *item, void *data)
{
  struct mu_mime_param *param = item;
  
  mu_printf ("%s", name);
  if (param->lang)
    mu_printf ("(lang:%s/%s)", param->lang, param->cset);
  mu_printf ("=%s\n", param->value);
  return 0;
}

static void
cli_debug (struct mu_parseopt *po, struct mu_option *opt, char const *arg)
{
  mu_debug_parse_spec (arg);
}

char *charset;
char *header_name;
unsigned long width = 76;

struct mu_option options[] = {
  { "debug", 0, "SPEC", MU_OPTION_DEFAULT,
    "set debug level", mu_c_string, NULL, cli_debug },
  { "charset", 0, "NAME", MU_OPTION_DEFAULT,
    "convert values to this charset", mu_c_string, &charset },
  { "header",  0, "NAME", MU_OPTION_DEFAULT,
    "set header name", mu_c_string, &header_name },
  { "width",   0, "N", MU_OPTION_DEFAULT,
    "output width", mu_c_ulong, &width },
  MU_OPTION_END
};

int
main (int argc, char **argv)
{
  int rc;
  mu_stream_t tmp;
  mu_transport_t trans[2];
  char *value;
  mu_assoc_t assoc;
  
  mu_set_program_name (argv[0]);
  mu_cli_simple (argc, argv,
		 MU_CLI_OPTION_OPTIONS, options,
		 MU_CLI_OPTION_SINGLE_DASH,
		 MU_CLI_OPTION_PROG_DOC, "mu_mime_header_parse test",
		 MU_CLI_OPTION_PROG_ARGS, "[PARAM...]",
		 MU_CLI_OPTION_EXTRA_INFO, "If arguments (PARAM) are "
		 "specified, only the matching parameters will be displayed.",
		 MU_CLI_OPTION_RETURN_ARGC, &argc,
		 MU_CLI_OPTION_RETURN_ARGV, &argv,
		 MU_CLI_OPTION_END);
	    
  MU_ASSERT (mu_memory_stream_create (&tmp, MU_STREAM_RDWR));
  MU_ASSERT (mu_stream_copy (tmp, mu_strin, 0, NULL));
  MU_ASSERT (mu_stream_write (tmp, "", 1, NULL));
  MU_ASSERT (mu_stream_ioctl (tmp, MU_IOCTL_TRANSPORT, MU_IOCTL_OP_GET,
			      trans));

  if (argc)
    {
      int i;
      
      MU_ASSERT (mu_mime_param_assoc_create (&assoc));
      for (i = 0; i < argc; i++)
	mu_assoc_install (assoc, argv[i], NULL);
      rc = mu_mime_header_parse_subset ((char*)trans[0], charset, &value,
					assoc);
    }
  else
    {
      rc = mu_mime_header_parse ((char*)trans[0], charset, &value, &assoc);
    }
  if (rc)
    {
      mu_diag_funcall (MU_DIAG_ERROR, "mu_mime_header_parse", NULL, rc);
      return 2;
    }

  if (header_name)
    {
      mu_header_t hdr;
      mu_stream_t hstr;
      
      MU_ASSERT (mu_header_create (&hdr, NULL, 0));
      MU_ASSERT (mu_mime_header_set_w (hdr, header_name, value, assoc, width));
      MU_ASSERT (mu_header_get_streamref (hdr, &hstr));
      MU_ASSERT (mu_stream_copy (mu_strout, hstr, 0, NULL));
    }
  else
    {
      mu_printf ("%s\n", value);
      mu_assoc_sort_r (assoc, sort_names, NULL);
      mu_assoc_foreach (assoc, print_param, NULL);
    }
  
  return 0;
}
  
