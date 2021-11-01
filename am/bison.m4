# bison.m4 serial 3
AC_DEFUN([MU_PROG_BISON],
[
  AC_PATH_PROG([BISON],[bison],[\$(SHELL) \$(top_srcdir)/build-aux/missing bison])
])

