/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2020 Free Software Foundation, Inc.

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

#define PY_MODULE "envelope"
#define PY_CSNAME "EnvelopeType"

static PyObject *
_repr (PyObject *self)
{
  char buf[80];
  sprintf (buf, "<" PY_MODULE "." PY_CSNAME " instance at %p>", self);
  return PyUnicode_FromString (buf);
}

static PyTypeObject PyEnvelopeType = {
  .ob_base = { PyObject_HEAD_INIT(NULL) },
  .tp_name = PY_MODULE "." PY_CSNAME,
  .tp_basicsize = sizeof (PyEnvelope),
  .tp_dealloc = (destructor)_py_dealloc, 
  .tp_repr = _repr,
  .tp_str = _repr,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "",
};

PyEnvelope *
PyEnvelope_NEW ()
{
  return (PyEnvelope *)PyObject_NEW (PyEnvelope, &PyEnvelopeType);
}

static PyObject *
api_envelope_create (PyObject *self, PyObject *args)
{
  int status;
  PyEnvelope *py_env;

  if (!PyArg_ParseTuple (args, "O!", &PyEnvelopeType, &py_env))
    return NULL;

  status = mu_envelope_create (&py_env->env, NULL);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_envelope_destroy (PyObject *self, PyObject *args)
{
  PyEnvelope *py_env;

  if (!PyArg_ParseTuple (args, "O!", &PyEnvelopeType, &py_env))
    return NULL;

  mu_envelope_destroy (&py_env->env, NULL);
  return _ro (Py_None);
}

static PyObject *
api_envelope_get_sender (PyObject *self, PyObject *args)
{
  int status;
  const char *sender = NULL;
  PyEnvelope *py_env;

  if (!PyArg_ParseTuple (args, "O!", &PyEnvelopeType, &py_env))
    return NULL;

  status = mu_envelope_sget_sender (py_env->env, &sender);
  return status_object (status, PyUnicode_FromString (mu_prstr (sender)));
}

static PyObject *
api_envelope_get_date (PyObject *self, PyObject *args)
{
  int status;
  const char *date = NULL;
  PyEnvelope *py_env;

  if (!PyArg_ParseTuple (args, "O!", &PyEnvelopeType, &py_env))
    return NULL;

  status = mu_envelope_sget_date (py_env->env, &date);
  return status_object (status, PyUnicode_FromString (mu_prstr (date)));
}

static PyMethodDef methods[] = {
  { "create", (PyCFunction) api_envelope_create, METH_VARARGS,
    "" },

  { "destroy", (PyCFunction) api_envelope_destroy, METH_VARARGS,
    "" },

  { "get_sender", (PyCFunction) api_envelope_get_sender, METH_VARARGS,
    "Get the address that this message was reportedly received from." },

  { "get_date", (PyCFunction) api_envelope_get_date, METH_VARARGS,
    "Get the date that the message was delivered to the mailbox." },

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
mu_py_init_envelope ()
{
  PyEnvelopeType.tp_new = PyType_GenericNew;
  return PyType_Ready (&PyEnvelopeType);
}

void
_mu_py_attach_envelope (void)
{
  PyObject *m;
  if ((m = _mu_py_attach_module (&moduledef)))
    {
      Py_INCREF (&PyEnvelopeType);
      PyModule_AddObject (m, PY_CSNAME, (PyObject *)&PyEnvelopeType);
    }
}
