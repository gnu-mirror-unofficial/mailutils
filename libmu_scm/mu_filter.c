/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2018-2020 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library.  If not, see
   <http://www.gnu.org/licenses/>. */

#include "mu_scm.h"
#include <mailutils/filter.h>
#include <mailutils/argcv.h>

static void
argv_free (void *p)
{
  mu_argv_free ((char**)p);
}

static SCM
make_filter_port (SCM port, SCM name, SCM args,
		  int filter_mode, char const *func_name)
/* The FUNC_NAME macro is used by SCM_VALIDATE_REST_ARGUMENT */
#define FUNC_NAME func_name
{
  char *fltname;
  mu_stream_t filter;
  mu_stream_t instr;
  size_t argc = 0;
  char **argv = NULL;
  int rc;
  int flags = 0;
  char *port_mode;
  
  SCM_ASSERT (scm_port_p (port), port, SCM_ARG1, FUNC_NAME);
  SCM_ASSERT (scm_is_string (name), name, SCM_ARG2, FUNC_NAME);
  SCM_VALIDATE_REST_ARGUMENT (args);

  port_mode = scm_to_locale_string (scm_port_mode (port));
  if (strchr (port_mode, 'r'))
    flags |= MU_STREAM_READ;
  if (strchr (port_mode, 'w'))
    flags |= MU_STREAM_WRITE;
  free (port_mode);
  if (!flags
      || ((flags & (MU_STREAM_READ|MU_STREAM_WRITE))
	  == (MU_STREAM_READ|MU_STREAM_WRITE)))
    scm_out_of_range (FUNC_NAME, port); //FIXME
  
  scm_dynwind_begin (0);
  
  fltname = scm_to_locale_string (name);
  scm_dynwind_free (fltname);
  
  rc = mu_scm_port_stream_create (&instr, port);
  if (rc)
    {
      mu_scm_error (FUNC_NAME, rc,
		    "Failed to convert transport port ~A",
		    scm_list_1 (port));
    }
  
  if (!scm_is_null (args))
    {
      size_t n;

      argc = scm_to_size_t (scm_length (args)) + 1;
      argv = calloc (argc + 1, sizeof (argv[0]));
      if (!argv)
	mu_scm_error (FUNC_NAME, ENOMEM, "Cannot allocate memory", SCM_BOOL_F);

      argv[0] = strdup (fltname);
      n = 1;
      for (; !scm_is_null (args); args = SCM_CDR (args))
	{
	  SCM arg = SCM_CAR (args);
	  SCM_ASSERT (scm_is_string (arg), arg, SCM_ARGn, FUNC_NAME);
	  argv[n] = scm_to_locale_string (arg);
	  n++;
	}
      argv[n] = NULL;
      scm_dynwind_unwind_handler (argv_free, argv, SCM_F_WIND_EXPLICITLY);
    }

  rc = mu_filter_create_args (&filter, instr,
			      fltname, argc, (const char**) argv,
			      filter_mode, flags);
  if (rc)
    {
      mu_scm_error (FUNC_NAME, rc,
		    "Failed to create filter ~A",
		    scm_list_1 (name));
    }

  scm_dynwind_end ();
  return mu_port_make_from_stream (filter,
				   flags == MU_STREAM_READ
				     ? SCM_RDNG
				     : SCM_WRTNG);
}
#undef FUNC_NAME

SCM_DEFINE_PUBLIC (scm_mu_encoder_port, "mu-encoder-port", 2, 0, 1,
		   (SCM port, SCM name, SCM args),
"Create encoding port using Mailutils filter @var{name} with optional arguments\n"
"@var{args}. The @var{port} argument must be a port opened either for\n"
"writing or for reading, but not both. The returned port will have the same\n"
"mode as @var{port}."
"\n\n"
"If @var{port} is open for reading, data will be read from it, passed through the\n"
"filter and returned. If it is open for writing, data written to the returned\n"
"port will be passed through filter and its output will be written to @var{port}.\n")		   
#define FUNC_NAME s_scm_mu_encoder_port
{
  return make_filter_port (port, name, args, MU_FILTER_ENCODE, FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE_PUBLIC (scm_mu_decoder_port, "mu-decoder-port", 2, 0, 1,
		   (SCM port, SCM name, SCM args),
"Create a decoding port using Mailutils filter @var{name} with optional arguments\n"
"@var{args}. The @var{port} argument must be a port opened either for\n"
"writing or for reading, but not both. The returned port will have the same\n"
"mode as @var{port}."
"\n\n"
"If @var{port} is open for reading, data will be read from it, passed through the\n"
"filter and returned. If it is open for writing, data written to the returned\n"
"port will be passed through filter and its output will be written to @var{port}.\n")		   
#define FUNC_NAME s_scm_mu_decoder_port
{
  return make_filter_port (port, name, args, MU_FILTER_DECODE, FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE_PUBLIC (scm_mu_header_decode, "mu-header-decode", 1, 1, 0,
		   (SCM hdr, SCM charset),
"Decode the header value @var{hdr}, encoded as per RFC 2047.\n"
"Optional @var{charset} defaults to @samp{utf-8}.\n")
#define FUNC_NAME s_scm_mu_header_decode
{
  char *c_hdr, *c_charset, *c_res;
  int rc;
  SCM res;
  
  SCM_ASSERT (scm_is_string (hdr), hdr, SCM_ARG1, FUNC_NAME);

  scm_dynwind_begin (0);
  if (SCM_UNBNDP (charset))
    c_charset = "utf-8";
  else
    {
      SCM_ASSERT (scm_is_string (charset), charset, SCM_ARG2, FUNC_NAME);
      c_charset = scm_to_locale_string (charset);
      scm_dynwind_free (c_charset);
    }
  c_hdr = scm_to_locale_string (hdr);
  scm_dynwind_free (c_hdr);

  rc = mu_rfc2047_decode (c_charset, c_hdr, &c_res);
  if (rc)
    mu_scm_error (FUNC_NAME, rc,
		  "Can't convert header value", SCM_BOOL_F);
  
  scm_dynwind_end ();

  res = scm_from_locale_string (c_res);
  free (c_res);

  return res;
}
#undef FUNC_NAME

SCM_DEFINE_PUBLIC (scm_mu_header_encode, "mu-header-encode", 1, 2, 0,
		   (SCM hdr, SCM encoding, SCM charset),
"Encode the string @var{hdr} as per RFC 2047.\n"
"Both @var{encoding} and @var{charset} are optional.\n"		   
"Allowed values for @var{encoding} are @samp{base64} and @samp{quoted-printable}.\n"
"Default is selected depending on number of printable characters in @var{hdr}.\n"
"Optional @var{charset} defaults to @samp{utf-8}.\n")
#define FUNC_NAME s_scm_mu_header_encode
{
  char *c_hdr, *c_charset, *c_encoding, *c_res;
  int rc;
  SCM res;
  
  SCM_ASSERT (scm_is_string (hdr), hdr, SCM_ARG1, FUNC_NAME);

  scm_dynwind_begin (0);
  
  if (SCM_UNBNDP (encoding))
    c_encoding = NULL;
  else
    {
      SCM_ASSERT (scm_is_string (encoding), encoding, SCM_ARG2, FUNC_NAME);
      c_encoding = scm_to_locale_string (encoding);
      scm_dynwind_free (c_encoding);
    }

  if (SCM_UNBNDP (charset))
    c_charset = "utf-8";
  else
    {
      SCM_ASSERT (scm_is_string (charset), charset, SCM_ARG3, FUNC_NAME);
      c_charset = scm_to_locale_string (charset);
      scm_dynwind_free (c_charset);
    }
  c_hdr = scm_to_locale_string (hdr);
  scm_dynwind_free (c_hdr);

  if (!c_encoding)
    {
      size_t len = strlen (c_hdr);
      size_t i, enc;

      enc = 0;
      for (i = 0; i < len; i++)
	if (!mu_isprint (c_hdr[i]))
	  enc++;
      c_encoding = (enc > len / 2) ? "base64" : "quoted-printable";
    }

  rc = mu_rfc2047_encode (c_charset, c_encoding, c_hdr, &c_res);
  if (rc)
    mu_scm_error (FUNC_NAME, rc,
		  "Can't encode header value", SCM_BOOL_F);
  
  scm_dynwind_end ();

  res = scm_from_locale_string (c_res);
  free (c_res);

  return res;
}
#undef FUNC_NAME

void
mu_scm_filter_init (void)
{
#include "mu_filter.x"
}
