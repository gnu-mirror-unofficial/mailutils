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

#define PY_MODULE "mailer"
#define PY_CSNAME "MailerType"

static PyObject *
_repr (PyObject *self)
{
  char buf[80];
  sprintf (buf, "<" PY_MODULE "." PY_CSNAME " instance at %p>", self);
  return PyUnicode_FromString (buf);
}

static PyTypeObject PyMailerType = {
  .ob_base = { PyObject_HEAD_INIT(NULL) },
  .tp_name = PY_MODULE "." PY_CSNAME,
  .tp_basicsize = sizeof (PyMailer),
  .tp_dealloc = (destructor)_py_dealloc,
  .tp_repr = _repr,
  .tp_str = _repr, 
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "",
};

PyMailer *
PyMailer_NEW ()
{
  return (PyMailer *)PyObject_NEW (PyMailer, &PyMailerType);
}

static PyObject *
api_mailer_create (PyObject *self, PyObject *args)
{
  int status;
  char *url;
  PyMailer *py_mlr;

  if (!PyArg_ParseTuple (args, "O!s", &PyMailerType, &py_mlr, &url))
    return NULL;

  status = mu_mailer_create (&py_mlr->mlr, url);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_mailer_destroy (PyObject *self, PyObject *args)
{
  PyMailer *py_mlr;

  if (!PyArg_ParseTuple (args, "O!", &PyMailerType, &py_mlr))
    return NULL;

  mu_mailer_destroy (&py_mlr->mlr);
  return _ro (Py_None);
}

static PyObject *
api_mailer_open (PyObject *self, PyObject *args)
{
  int status, flags;
  PyMailer *py_mlr;

  if (!PyArg_ParseTuple (args, "O!i", &PyMailerType, &py_mlr, &flags))
    return NULL;

  status = mu_mailer_open (py_mlr->mlr, flags);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_mailer_close (PyObject *self, PyObject *args)
{
  int status;
  PyMailer *py_mlr;

  if (!PyArg_ParseTuple (args, "O!", &PyMailerType, &py_mlr))
    return NULL;

  status = mu_mailer_close (py_mlr->mlr);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_mailer_send_message (PyObject *self, PyObject *args)
{
  int status;
  PyMailer *py_mlr;
  PyMessage *py_msg;
  PyAddress *py_from, *py_to;
  mu_address_t c_from = NULL, c_to = NULL;

  if (!PyArg_ParseTuple (args, "O!OOO", &PyMailerType, &py_mlr,
			 &py_msg, &py_from, &py_to))
    return NULL;

  if (!PyMessage_Check ((PyObject *)py_msg))
    {
      PyErr_SetString (PyExc_TypeError, "");
      return NULL;
    }
  if (!PyAddress_Check ((PyObject *)py_from) &&
      (PyObject *)py_from != Py_None)
    {
      PyErr_SetString (PyExc_TypeError, "");
      return NULL;
    }
  if (!PyAddress_Check ((PyObject *)py_to) &&
      (PyObject *)py_to != Py_None)
    {
      PyErr_SetString (PyExc_TypeError, "");
      return NULL;
    }
  if ((PyObject *)py_from != Py_None)
    c_from = py_from->addr;
  if ((PyObject *)py_to != Py_None)
    c_to = py_to->addr;

  status = mu_mailer_send_message (py_mlr->mlr, py_msg->msg,
				   c_from, c_to);
  return _ro (PyLong_FromLong (status));
}

static PyMethodDef methods[] = {
  { "create", (PyCFunction) api_mailer_create, METH_VARARGS,
    "Create mailer." },

  { "destroy", (PyCFunction) api_mailer_destroy, METH_VARARGS,
    "The resources allocate for 'msg' are freed." },

  { "open", (PyCFunction) api_mailer_open, METH_VARARGS,
    "" },

  { "close", (PyCFunction) api_mailer_close, METH_VARARGS,
    "" },

  { "send_message", (PyCFunction) api_mailer_send_message, METH_VARARGS,
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
mu_py_init_mailer (void)
{
  PyMailerType.tp_new = PyType_GenericNew;
  return PyType_Ready (&PyMailerType);
}

void
_mu_py_attach_mailer (void)
{
  PyObject *m;
  if ((m = _mu_py_attach_module (&moduledef)))
    {
      Py_INCREF (&PyMailerType);
      PyModule_AddObject (m, PY_CSNAME, (PyObject *)&PyMailerType);
    }
}
