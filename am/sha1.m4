# sha1.m4 serial 7
dnl Copyright (C) 2002-2021 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_SHA1],
[
  MU_LIBOBJ([sha1])

  dnl Prerequisites of lib/sha1.c.
  AC_REQUIRE([AC_C_BIGENDIAN])
  :
])
