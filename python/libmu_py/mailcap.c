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

#define PY_MODULE  "mailcap"
#define PY_CSNAME1 "MailcapType"
#define PY_CSNAME2 "MailcapEntryType"

static PyObject *
_repr1 (PyObject *self)
{
  char buf[80];
  sprintf (buf, "<" PY_MODULE "." PY_CSNAME1 " instance at %p>", self);
  return PyUnicode_FromString (buf);
}

static PyTypeObject PyMailcapType = {
  .ob_base = { PyObject_HEAD_INIT(NULL) },
  .tp_name = PY_MODULE "." PY_CSNAME1,
  .tp_basicsize = sizeof (PyMailcap),
  .tp_dealloc = (destructor)_py_dealloc,
  .tp_repr = _repr1,
  .tp_str = _repr1,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "",
};

PyMailcap *
PyMailcap_NEW ()
{
  return (PyMailcap *)PyObject_NEW (PyMailcap, &PyMailcapType);
}

static PyObject *
_repr2 (PyObject *self)
{
  char buf[80];
  sprintf (buf, "<" PY_MODULE "." PY_CSNAME2 " instance at %p>", self);
  return PyUnicode_FromString (buf);
}

static PyTypeObject PyMailcapEntryType = {
  .ob_base = { PyObject_HEAD_INIT(NULL) },
  .tp_name = PY_MODULE "." PY_CSNAME2,
  .tp_basicsize = sizeof (PyMailcapEntry),
  .tp_dealloc = (destructor)_py_dealloc,
  .tp_repr = _repr2,
  .tp_str = _repr2,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "",
};

PyMailcapEntry *
PyMailcapEntry_NEW ()
{
  return (PyMailcapEntry *)PyObject_NEW (PyMailcapEntry,
					 &PyMailcapEntryType);
}


static PyObject *
api_mailcap_create (PyObject *self, PyObject *args)
{
  int status;
  PyMailcap *py_mc;
  PyStream *py_stm;

  if (!PyArg_ParseTuple (args, "O!O", &PyMailcapType, &py_mc, &py_stm))
    return NULL;

  if (!PyStream_Check ((PyObject *)py_stm))
    {
      PyErr_SetString (PyExc_TypeError, ((PyObject*)py_stm)->ob_type->tp_name);
      return NULL;
    }

  status = mu_mailcap_create (&py_mc->mc);
  if (status)
    return _ro (PyLong_FromLong (status));
  status = mu_mailcap_parse (py_mc->mc, py_stm->stm, NULL);
  if (status == MU_ERR_PARSE)
    status = 0; /* FIXME */
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_mailcap_destroy (PyObject *self, PyObject *args)
{
  PyMailcap *py_mc;

  if (!PyArg_ParseTuple (args, "O!", &PyMailcapType, &py_mc))
    return NULL;

  mu_mailcap_destroy (&py_mc->mc);
  return _ro (Py_None);
}

static PyObject *
api_mailcap_entries_count (PyObject *self, PyObject *args)
{
  int status;
  size_t count = 0;
  PyMailcap *py_mc;

  if (!PyArg_ParseTuple (args, "O!", &PyMailcapType, &py_mc))
    return NULL;

  status = mu_mailcap_get_count (py_mc->mc, &count);
  return status_object (status, PyLong_FromSize_t (count));
}

static PyObject *
api_mailcap_get_entry (PyObject *self, PyObject *args)
{
  int status;
  Py_ssize_t i;
  PyMailcap *py_mc;
  PyMailcapEntry *py_entry = PyMailcapEntry_NEW ();
  if (!PyArg_ParseTuple (args, "O!n", &PyMailcapType, &py_mc, &i))
    return NULL;
  ASSERT_INDEX_RANGE (i, "mailcap");
  status = mu_mailcap_get_entry (py_mc->mc, i, &py_entry->entry);

  Py_INCREF (py_entry);
  return status_object (status, (PyObject *)py_entry);
}

static PyObject *
api_mailcap_find_entry (PyObject *self, PyObject *args)
{
  int status;
  PyMailcap *py_mc;
  PyMailcapEntry *py_entry = PyMailcapEntry_NEW ();
  char *name;

  if (!PyArg_ParseTuple (args, "O!s", &PyMailcapType, &py_mc, &name))
    return NULL;
  status = mu_mailcap_find_entry (py_mc->mc, name, &py_entry->entry);

  Py_INCREF (py_entry);
  return status_object (status, (PyObject *)py_entry);
}

static PyObject *
api_mailcap_entry_fields_count (PyObject *self, PyObject *args)
{
  int status;
  size_t count;
  PyMailcapEntry *py_entry;

  if (!PyArg_ParseTuple (args, "O!", &PyMailcapEntryType, &py_entry))
    return NULL;

  status = mu_mailcap_entry_fields_count (py_entry->entry, &count);
  return status_object (status, PyLong_FromSize_t (count));
}

static PyObject *
api_mailcap_entry_get_field (PyObject *self, PyObject *args)
{
  int status;
  char *name;
  char const *value;
  PyMailcapEntry *py_entry;

  if (!PyArg_ParseTuple (args, "O!s", &PyMailcapEntryType, &py_entry, &name))
    return NULL;
  status = mu_mailcap_entry_sget_field (py_entry->entry, name, &value);
  return status_object (status,
			status == 0
			  ? (value ? PyUnicode_FromString (value)
				   : PyBool_FromLong (1))
			  : PyBool_FromLong (0));
}

static PyObject *
api_mailcap_entry_get_typefield (PyObject *self, PyObject *args)
{
  int status;
  char const *value;
  PyMailcapEntry *py_entry;

  if (!PyArg_ParseTuple (args, "O!", &PyMailcapEntryType, &py_entry))
    return NULL;

  status = mu_mailcap_entry_sget_type (py_entry->entry, &value);
  return status_object (status, PyUnicode_FromString (status == 0 ? value : ""));
}

static PyObject *
api_mailcap_entry_get_viewcommand (PyObject *self, PyObject *args)
{
  int status;
  char const *value;
  PyMailcapEntry *py_entry;

  if (!PyArg_ParseTuple (args, "O!", &PyMailcapEntryType, &py_entry))
    return NULL;

  status = mu_mailcap_entry_sget_command (py_entry->entry, &value);
  return status_object (status, PyUnicode_FromString (status == 0 ? value : ""));
}

static PyMethodDef methods[] = {
  { "create", (PyCFunction) api_mailcap_create, METH_VARARGS,
    "Allocate, parse the buffer from the 'stream' and initializes 'mailcap'." },

  { "destroy", (PyCFunction) api_mailcap_destroy, METH_VARARGS,
    "Release any resources from the mailcap object." },

  { "entries_count", (PyCFunction) api_mailcap_entries_count, METH_VARARGS,
    "Return the number of entries found in the mailcap." },

  { "get_entry", (PyCFunction) api_mailcap_get_entry, METH_VARARGS,
    "Return in 'entry' the mailcap entry of 'no'." },

  { "find_entry", (PyCFunction) api_mailcap_find_entry, METH_VARARGS,
    "Return in 'entry' the mailcap entry for given content-type." },

  { "entry_fields_count", (PyCFunction) api_mailcap_entry_fields_count,
    METH_VARARGS,
    "Return the number of fields found in the entry." },

  { "entry_get_field", (PyCFunction) api_mailcap_entry_get_field,
    METH_VARARGS,
    "" },

  { "entry_get_typefield", (PyCFunction) api_mailcap_entry_get_typefield,
    METH_VARARGS,
    "" },

  { "entry_get_viewcommand",
    (PyCFunction) api_mailcap_entry_get_viewcommand, METH_VARARGS,
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
mu_py_init_mailcap (void)
{
  PyMailcapType.tp_new = PyType_GenericNew;
  PyMailcapEntryType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&PyMailcapType) < 0)
    return -1;
  if (PyType_Ready (&PyMailcapEntryType) < 0)
    return -1;
  return 0;
}

void
_mu_py_attach_mailcap (void)
{
  PyObject *m;
  if ((m = _mu_py_attach_module (&moduledef)))
    {
      Py_INCREF (&PyMailcapType);
      Py_INCREF (&PyMailcapEntryType);
      PyModule_AddObject (m, PY_CSNAME1, (PyObject *)&PyMailcapType);
      PyModule_AddObject (m, PY_CSNAME2, (PyObject *)&PyMailcapEntryType);
    }
}
