/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2012, 2014-2018 Free Software Foundation, Inc.

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

PyObject *
_ro (PyObject *obj)
{
  Py_INCREF (obj);
  return obj;
}

void
_py_dealloc (PyObject *self)
{
  self->ob_type->tp_free (self);
}

PyObject *
status_object (int status, PyObject *py_obj)
{
  PyObject *py_ret = PyTuple_New (2);
  PyTuple_SetItem (py_ret, 0, PyLong_FromLong (status));
  PyTuple_SetItem (py_ret, 1, py_obj);
  return _ro (py_ret);
}

static PyObject *package;
static PyObject *all;

#define MU_MODULE_ROOT PY_PACKAGE_NAME "." PY_ROOT_NAME "."

static char *
_mu_py_module_name (const char *nm)
{
  char *buf = malloc (sizeof (MU_MODULE_ROOT) + strlen (nm));
  if (!buf)
    abort ();
  return strcat (strcpy (buf, MU_MODULE_ROOT), nm);
}
  
PyObject *
_mu_py_attach_module (struct PyModuleDef *mdef)
{
  PyObject *module, *sysmodules;
  char *ns = _mu_py_module_name (mdef->m_name);
  
  mdef->m_name = ns;
  module = PyModule_Create (mdef);
  if (!module)
    return NULL;
  sysmodules = PyImport_GetModuleDict ();
  PyMapping_SetItemString (sysmodules, mdef->m_name, module);
  PyList_Append (all, PyUnicode_FromString (mdef->m_name));
  //  free (ns);
  return module;
}

void
mu_py_init (void)
{
  mu_py_init_address ();
  mu_py_init_attribute ();
  mu_py_init_auth ();
  mu_py_init_body ();
  mu_py_init_envelope ();
  mu_py_init_header ();
  mu_py_init_folder ();
  mu_py_init_mailer ();
  mu_py_init_mailbox ();
  mu_py_init_mailcap ();
  mu_py_init_message ();
  mu_py_init_mime ();
  mu_py_init_secret ();
  mu_py_init_sieve ();
  mu_py_init_stream ();
  mu_py_init_url ();
}

static PyMethodDef nomethods[] = {
  { NULL, NULL }
};

PyObject *
mu_py_attach_modules (void)
{
  static struct PyModuleDef def = {
    PyModuleDef_HEAD_INIT,
    PY_ROOT_NAME,
    NULL,
    -1,
    nomethods
  };
  package = PyModule_Create (&def);
  if (package)
    {
      PyModule_AddStringConstant (package, "__version__", PY_PACKAGE_VERSION);
      if (!PyModule_AddObject (package, "__all__", _ro (PyList_New (0))))
	{
	  all = PyObject_GetAttrString (package, "__all__");
	  if (!all || !PyList_Check (all))
	    return NULL;
	}

      _mu_py_attach_error ();
      _mu_py_attach_address ();
      _mu_py_attach_attribute ();
      _mu_py_attach_auth ();
      _mu_py_attach_body ();
      _mu_py_attach_envelope ();
      _mu_py_attach_errno ();
      _mu_py_attach_header ();
      _mu_py_attach_filter ();
      _mu_py_attach_folder ();
      _mu_py_attach_mailer ();
      _mu_py_attach_mailbox ();
      _mu_py_attach_mailcap ();
      _mu_py_attach_message ();
      _mu_py_attach_mime ();
      _mu_py_attach_nls ();
      _mu_py_attach_registrar ();
      _mu_py_attach_secret ();
      _mu_py_attach_sieve ();
      _mu_py_attach_stream ();
      _mu_py_attach_url ();
      _mu_py_attach_util ();
    }
  return package;
}
