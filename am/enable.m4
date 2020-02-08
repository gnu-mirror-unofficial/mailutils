dnl This file is part of GNU mailutils.
dnl Copyright (C) 2002-2020 Free Software Foundation, Inc.
dnl
dnl GNU Mailutils is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl GNU Mailutils is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>.

dnl MU_ENABLE_SUPPORT(feature, [action-if-true], [action-if-false],
dnl                   [additional-cond], [default-value])

AC_DEFUN([MU_ENABLE_SUPPORT], [
	m4_pushdef([mu_upcase],m4_translit($1,[a-z+-],[A-ZX_]))
	m4_pushdef([mu_cache_var],[mu_cv_enable_]m4_translit($1,[+-],[x_]))
	m4_pushdef([mu_cond],[MU_COND_SUPPORT_]mu_upcase)

       	m4_if([$4],,,[if test $4; then])
	  AC_ARG_ENABLE($1, 
	                AC_HELP_STRING([--disable-]$1,
		                       [disable ]$1[ support]),
	                [
	  case "${enableval}" in
		yes) mu_cache_var=yes;;
                no)  mu_cache_var=no;;
	        *)   AC_MSG_ERROR([bad value ${enableval} for --disable-$1]) ;;
          esac],
                      [mu_cache_var=m4_if([$5],,yes,[$5])])

	  if test "[$]mu_cache_var" = "yes"; then
		  m4_if([$2],,:,[$2])
 	  m4_if([$3],,,else
                 [$3])
	  fi
	m4_if([$4],,,[else
		mu_cache_var=no
        fi])
	if test "[$]mu_cache_var" = "yes"; then
		AC_DEFINE([ENABLE_]mu_upcase,1,[Define this if you enable $1 support])
        fi
	AM_CONDITIONAL(mu_cond, [test "[$]mu_cache_var" = "yes"])
	m4_popdef([mu_cond])
	m4_popdef([mu_cache_var])
	m4_popdef([mu_upcase])
])

dnl MU_ENABLE_BUILD(feature, [action-if-true], [action-if-false],
dnl                 [additional-cond], [default-value], [listvar])
AC_DEFUN([MU_ENABLE_BUILD], [
	m4_pushdef([mu_upcase],m4_translit($1,[a-z+-],[A-ZX_]))
	m4_pushdef([mu_cache_var],[mu_cv_enable_build_]m4_translit($1,[+-],[x_]))
	m4_pushdef([mu_cond],[MU_COND_]mu_upcase)

	AC_ARG_ENABLE(build-$1, 
	              AC_HELP_STRING([--disable-build-]$1,
	                             [do not build ]$1),
	                [
  	  case "${enableval}" in
		yes) mu_cache_var=yes;;
                no)  mu_cache_var=no;;
	        *)   AC_MSG_ERROR([bad value ${enableval} for --disable-$1]) ;;
          esac],
                      [m4_if([$4],,[mu_cache_var=m4_if([$5],,yes,[$5])],
		              [if test $4; then
			         mu_cache_var=yes
			       else
			         mu_cache_var=no
			       fi])])

	if test "[$]mu_cache_var" = "yes"; then
		  m4_if([$2],,:,[$2])
		  m4_if([$6],,,[$6="$[]$6 $1"])
	m4_if([$3],,,else
               [$3])
	fi
	if test "[$]mu_cache_var" = "yes"; then
	  AC_DEFINE([MU_BUILD_]mu_upcase,1,[Define this if you build $1])
        fi
	  
	AM_CONDITIONAL(mu_cond,
	               [test "[$]mu_cache_var" = "yes"])
	
	m4_popdef([mu_upcase])
	m4_popdef([mu_cache_var])
	m4_popdef([mu_cond])
])

AC_CONFIG_COMMANDS_PRE([
AC_SUBST([MU_LIB_LOCAL_MAILBOX])
AC_SUBST([MU_LIB_REMOTE_MAILBOX])
AC_SUBST([MU_LIB_MAILBOX],['$(MU_LIB_LOCAL_MAILBOX) $(MU_LIB_REMOTE_MAILBOX)'])
])

dnl MU_ENABLE_MAILBOX_FORMAT([CATEGORY], [SCHEME],
dnl                   [action-if-true], [action-if-false],
dnl                   [additional-cond], [default-value])

AC_DEFUN([MU_ENABLE_MAILBOX_FORMAT], [
 m4_pushdef([LIBNAME],[[MU_LIB_]translit($2,[a-z+-],[A-ZX_])])
 AC_SUBST(LIBNAME)
 MU_ENABLE_SUPPORT([$2],
  [LIBNAME='${top_builddir}/libproto/$2/libmu_$2.la'
   $3],
  m4_shift(m4_shift(m4_shift($@))))
  AC_CONFIG_FILES([libproto/$2/Makefile])
  m4_syscmd([test -f libproto/$2/tests/Makefile.am -a -f libproto/$2/tests/atlocal.in])
  m4_if(sysval,0,[MU_CONFIG_TESTSUITE([libproto/$2])])
  [MU_LIB_]$1[_MAILBOX]="${[MU_LIB_]$1[_MAILBOX]} \$(LIBNAME)"
  m4_popdef([LIBNAME])
])

AC_DEFUN([MU_ENABLE_LOCAL_MAILBOX_FORMAT],
 [MU_ENABLE_MAILBOX_FORMAT([LOCAL],$@)])

AC_DEFUN([MU_ENABLE_REMOTE_MAILBOX_FORMAT],
 [MU_ENABLE_MAILBOX_FORMAT([REMOTE],$@)])
