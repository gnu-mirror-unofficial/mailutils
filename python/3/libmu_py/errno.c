/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2019 Free Software Foundation, Inc.

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

#include "libmu_py.h"
#include <mailutils/errno.h>

#define PY_MODULE "errno"

static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  PY_MODULE,
  NULL,
  -1,
  NULL
};

void
_mu_py_attach_errno (void)
{
  int i;
  PyObject *module = _mu_py_attach_module (&moduledef);

  for (i = MU_ERR_BASE; i < MU_ERR_LAST; i++)
    {
      const char *en = mu_errname (i);
      PyModule_AddIntConstant (module, en, i);
    }
}
