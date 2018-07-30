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

#define PY_MODULE "secret"
#define PY_CSNAME "SecretType"

static PyObject *
_repr (PyObject *self)
{
  char buf[80];
  sprintf (buf, "<" PY_MODULE "." PY_CSNAME " instance at %p>", self);
  return PyUnicode_FromString (buf);
}

static PyTypeObject PySecretType = {
  .ob_base = { PyObject_HEAD_INIT(NULL) },
  .tp_name = PY_MODULE "." PY_CSNAME,
  .tp_basicsize = sizeof (PySecret),
  .tp_dealloc = (destructor)_py_dealloc,
  .tp_repr = _repr,
  .tp_str = _repr,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "",
};

PySecret *
PySecret_NEW ()
{
  return (PySecret *)PyObject_NEW (PySecret, &PySecretType);
}

int
PySecret_Check (PyObject *x)
{
  return x->ob_type == &PySecretType;
}

static PyObject *
api_secret_create (PyObject *self, PyObject *args)
{
  int status;
  char *str;
  Py_ssize_t len;
  PySecret *py_secret;

  if (!PyArg_ParseTuple (args, "O!sn", &PySecretType, &py_secret,
			 &str, &len))
    return NULL;
  if (len <= 0)
    {
      PyErr_SetString (PyExc_RuntimeError, "secret length out of range");
      return NULL;
    }
  status = mu_secret_create (&py_secret->secret, str, len);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_secret_destroy (PyObject *self, PyObject *args)
{
  PySecret *py_secret;

  if (!PyArg_ParseTuple (args, "O!", &PySecretType, &py_secret))
    return NULL;

  mu_secret_destroy (&py_secret->secret);
  return _ro (Py_None);
}

static PyObject *
api_secret_password (PyObject *self, PyObject *args)
{
  const char *pass;
  PySecret *py_secret;

  if (!PyArg_ParseTuple (args, "O!", &PySecretType, &py_secret))
    return NULL;

  pass = mu_secret_password (py_secret->secret);
  return _ro (PyUnicode_FromString (mu_prstr (pass)));
}

static PyObject *
api_secret_password_unref (PyObject *self, PyObject *args)
{
  PySecret *py_secret;

  if (!PyArg_ParseTuple (args, "O!", &PySecretType, &py_secret))
    return NULL;

  mu_secret_password_unref (py_secret->secret);
  return _ro (Py_None);
}

static PyObject *
api_clear_passwd (PyObject *self, PyObject *args)
{
  char *p;

  if (!PyArg_ParseTuple (args, "s", &p))
    return NULL;

  while (*p)
    *p++ = 0;
  return _ro (Py_None);
}

static PyMethodDef methods[] = {
  { "create", (PyCFunction) api_secret_create, METH_VARARGS,
    "Create the secret data structure." },

  { "destroy", (PyCFunction) api_secret_destroy, METH_VARARGS,
    "Destroy the secret and free its resources." },

  { "password", (PyCFunction) api_secret_password, METH_VARARGS,
    "" },

  { "password_unref", (PyCFunction) api_secret_password_unref, METH_VARARGS,
    "" },

  { "clear_passwd", (PyCFunction) api_clear_passwd, METH_VARARGS,
    "" },

  { NULL, NULL, 0, NULL }
};

static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  PY_MODULE,
  NULL,
  -1,
  methods
};

int
mu_py_init_secret (void)
{
  PySecretType.tp_new = PyType_GenericNew;
  return PyType_Ready (&PySecretType);
}

void
_mu_py_attach_secret (void)
{
  PyObject *m;
  if ((m = _mu_py_attach_module (&moduledef)))
    {
      Py_INCREF (&PySecretType);
      PyModule_AddObject (m, PY_CSNAME, (PyObject *)&PySecretType);
    }
}
