/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2020 Free Software Foundation, Inc.

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

/* This header converts wordsplit to mailutils namespace by prefixing each
   exported identifier with mu_ (or MU_, for macros).
   The canonical wordsplit.h header is located in mailutils/sys.
*/

#ifndef __MAILUTILS_WORDSPLIT_H
#define __MAILUTILS_WORDSPLIT_H

# define wordsplit           mu_wordsplit

# define wordsplit_t         mu_wordsplit_t

# define wordsplit_len               mu_wordsplit_len
# define wordsplit_free              mu_wordsplit_free
# define wordsplit_free_words        mu_wordsplit_free_words
# define wordsplit_free_envbuf       mu_wordsplit_free_envbuf
# define wordsplit_free_parambuf     mu_wordsplit_free_parambuf
# define wordsplit_getwords          mu_wordsplit_getwords
# define wordsplit_get_words         mu_wordsplit_get_words
# define wordsplit_append            mu_wordsplit_append
# define wordsplit_c_unquote_char    mu_wordsplit_c_unquote_char
# define wordsplit_c_quote_char	     mu_wordsplit_c_quote_char
# define wordsplit_c_quoted_length   mu_wordsplit_c_quoted_length
# define wordsplit_c_quote_copy	     mu_wordsplit_c_quote_copy
# define wordsplit_perror	     mu_wordsplit_perror
# define wordsplit_strerror	     mu_wordsplit_strerror
# define wordsplit_clearerr          mu_wordsplit_clearerr

# include <mailutils/sys/wordsplit.h>

# define MU_WORDSPLIT_ENV_INIT	     WORDSPLIT_ENV_INIT
# define MU_WRDSF_APPEND	     WRDSF_APPEND
# define MU_WRDSF_DOOFFS	     WRDSF_DOOFFS
# define MU_WRDSF_NOCMD		     WRDSF_NOCMD
# define MU_WRDSF_REUSE		     WRDSF_REUSE
# define MU_WRDSF_SHOWERR	     WRDSF_SHOWERR
# define MU_WRDSF_UNDEF		     WRDSF_UNDEF
# define MU_WRDSF_NOVAR		     WRDSF_NOVAR
# define MU_WRDSF_ENOMEMABRT	     WRDSF_ENOMEMABRT
# define MU_WRDSF_WS		     WRDSF_WS
# define MU_WRDSF_SQUOTE	     WRDSF_SQUOTE
# define MU_WRDSF_DQUOTE	     WRDSF_DQUOTE
# define MU_WRDSF_QUOTE		     WRDSF_QUOTE
# define MU_WRDSF_SQUEEZE_DELIMS     WRDSF_SQUEEZE_DELIMS
# define MU_WRDSF_RETURN_DELIMS	     WRDSF_RETURN_DELIMS
# define MU_WRDSF_SED_EXPR	     WRDSF_SED_EXPR
# define MU_WRDSF_DELIM		     WRDSF_DELIM
# define MU_WRDSF_COMMENT	     WRDSF_COMMENT
# define MU_WRDSF_ALLOC_DIE	     WRDSF_ALLOC_DIE
# define MU_WRDSF_ERROR		     WRDSF_ERROR
# define MU_WRDSF_DEBUG		     WRDSF_DEBUG
# define MU_WRDSF_ENV                WRDSF_ENV
# define MU_WRDSF_GETVAR             WRDSF_GETVAR
# define MU_WRDSF_SHOWDBG            WRDSF_SHOWDBG
# define MU_WRDSF_NOSPLIT            WRDSF_NOSPLIT
# define MU_WRDSF_KEEPUNDEF          WRDSF_KEEPUNDEF
# define MU_WRDSF_WARNUNDEF          WRDSF_WARNUNDEF
# define MU_WRDSF_CESCAPES           WRDSF_CESCAPES
# define MU_WRDSF_CLOSURE            WRDSF_CLOSURE
# define MU_WRDSF_ENV_KV             WRDSF_ENV_KV
# define MU_WRDSF_ESCAPE             WRDSF_ESCAPE
# define MU_WRDSF_INCREMENTAL        WRDSF_INCREMENTAL
# define MU_WRDSF_PATHEXPAND         WRDSF_PATHEXPAND
# define MU_WRDSF_OPTIONS            WRDSF_OPTIONS
# define MU_WRDSF_DEFFLAGS           WRDSF_DEFFLAGS

# define MU_WRDSO_NULLGLOB           WRDSO_NULLGLOB
# define MU_WRDSO_FAILGLOB           WRDSO_FAILGLOB
# define MU_WRDSO_DOTGLOB            WRDSO_DOTGLOB
# define MU_WRDSO_GETVARPREF         WRDSO_GETVARPREF
# define MU_WRDSO_BSKEEP_WORD        WRDSO_BSKEEP_WORD
# define MU_WRDSO_OESC_WORD          WRDSO_OESC_WORD
# define MU_WRDSO_XESC_WORD          WRDSO_XESC_WORD
# define MU_WRDSO_MAXWORDS           WRDSO_MAXWORDS
# define MU_WRDSO_BSKEEP_QUOTE       WRDSO_BSKEEP_QUOTE
# define MU_WRDSO_OESC_QUOTE         WRDSO_OESC_QUOTE
# define MU_WRDSO_XESC_QUOTE         WRDSO_XESC_QUOTE
# define MU_WRDSO_NOVARSPLIT         WRDSO_NOVARSPLIT
# define MU_WRDSO_NOCMDSPLIT         WRDSO_NOCMDSPLIT
# define MU_WRDSO_PARAMV             WRDSO_PARAMV
# define MU_WRDSO_PARAM_NEGIDX       WRDSO_PARAM_NEGIDX
# define MU_WRDSO_BSKEEP             WRDSO_BSKEEP
# define MU_WRDSO_OESC               WRDSO_OESC
# define MU_WRDSO_XESC               WRDSO_XESC

# define MU_WRDSO_ESC_SET            WRDSO_ESC_SET
# define MU_WRDSO_ESC_TEST           WRDSO_ESC_TEST

# define MU_WRDSX_WORD               WRDSX_WORD
# define MU_WRDSX_QUOTE              WRDSX_QUOTE

# define MU_WRDSE_OK                 WRDSE_OK
# define MU_WRDSE_EOF                WRDSE_EOF
# define MU_WRDSE_QUOTE              WRDSE_QUOTE
# define MU_WRDSE_NOSPACE            WRDSE_NOSPACE
# define MU_WRDSE_USAGE              WRDSE_USAGE
# define MU_WRDSE_CBRACE             WRDSE_CBRACE
# define MU_WRDSE_UNDEF              WRDSE_UNDEF
# define MU_WRDSE_NOINPUT            WRDSE_NOINPUT
# define MU_WRDSE_PAREN              WRDSE_PAREN
# define MU_WRDSE_GLOBERR            WRDSE_GLOBERR
# define MU_WRDSE_USERERR            WRDSE_USERERR
# define MU_WRDSE_BADPARAM           WRDSE_BADPARAM

#endif
