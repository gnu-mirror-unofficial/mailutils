/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2009-2021 Free Software Foundation, Inc.

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

#define PY_MODULE "body"
#define PY_CSNAME "BodyType"

static PyObject *
_repr (PyObject *self)
{
  char buf[80];
  sprintf (buf, "<" PY_MODULE "." PY_CSNAME " instance at %p>", self);
  return PyUnicode_FromString (buf);
}

static PyTypeObject PyBodyType = {
  .ob_base = { PyObject_HEAD_INIT(NULL) },
  .tp_name = PY_MODULE "." PY_CSNAME,
  .tp_basicsize = sizeof (PyBody),
  .tp_dealloc = (destructor)_py_dealloc,
  .tp_repr = _repr,
  .tp_str = _repr,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "",                        
};

PyBody *
PyBody_NEW ()
{
  return (PyBody *)PyObject_NEW (PyBody, &PyBodyType);
}

static PyObject *
api_body_size (PyObject *self, PyObject *args)
{
  int status;
  size_t size;
  PyBody *py_body;

  if (!PyArg_ParseTuple (args, "O!", &PyBodyType, &py_body))
    return NULL;

  status = mu_body_size (py_body->body, &size);
  return status_object (status, PyLong_FromSize_t (size));
}

static PyObject *
api_body_lines (PyObject *self, PyObject *args)
{
  int status;
  size_t lines;
  PyBody *py_body;

  if (!PyArg_ParseTuple (args, "O!", &PyBodyType, &py_body))
    return NULL;

  status = mu_body_lines (py_body->body, &lines);
  return status_object (status, PyLong_FromSize_t (lines));
}

static PyObject *
api_body_get_stream (PyObject *self, PyObject *args)
{
  int status;
  PyBody *py_body;
  PyStream *py_stm = PyStream_NEW ();

  if (!PyArg_ParseTuple (args, "O!", &PyBodyType, &py_body))
    return NULL;

  Py_INCREF (py_stm);

  status = mu_body_get_streamref (py_body->body, &py_stm->stm);
  return status_object (status, (PyObject *)py_stm);
}

static PyMethodDef methods[] = {
  { "size", (PyCFunction) api_body_size, METH_VARARGS,
    "Retrieve 'body' size." },

  { "lines", (PyCFunction) api_body_lines, METH_VARARGS,
    "Retrieve 'body' number of lines." },

  { "get_stream", (PyCFunction) api_body_get_stream, METH_VARARGS,
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
mu_py_init_body (void)
{
  PyBodyType.tp_new = PyType_GenericNew;
  return PyType_Ready (&PyBodyType);
}

void
_mu_py_attach_body (void)
{
  PyObject *m;
  if ((m = _mu_py_attach_module (&moduledef)))
    {
      Py_INCREF (&PyBodyType);
      PyModule_AddObject (m, PY_CSNAME, (PyObject *)&PyBodyType);
    }
}
