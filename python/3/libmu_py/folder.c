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

#define PY_MODULE "folder"
#define PY_CSNAME "FolderType"

static PyObject *
_repr (PyObject *self)
{
  char buf[80];
  sprintf (buf, "<" PY_MODULE "." PY_CSNAME " instance at %p>", self);
  return PyUnicode_FromString (buf);
}

static PyTypeObject PyFolderType = {
  .ob_base = { PyObject_HEAD_INIT(NULL) },
  .tp_name = PY_MODULE "." PY_CSNAME,
  .tp_basicsize = sizeof (PyFolder),
  .tp_dealloc = (destructor)_py_dealloc,
  .tp_repr = _repr,
  .tp_str = _repr,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "",
};

PyFolder *
PyFolder_NEW ()
{
  return (PyFolder *)PyObject_NEW (PyFolder, &PyFolderType);
}

static PyObject *
api_folder_create (PyObject *self, PyObject *args)
{
  int status;
  char *name;
  PyFolder *py_folder;

  if (!PyArg_ParseTuple (args, "O!s", &PyFolderType, &py_folder, &name))
    return NULL;

  status = mu_folder_create (&py_folder->folder, name);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_folder_destroy (PyObject *self, PyObject *args)
{
  PyFolder *py_folder;

  if (!PyArg_ParseTuple (args, "O!", &PyFolderType, &py_folder))
    return NULL;

  mu_folder_destroy (&py_folder->folder);
  return _ro (Py_None);
}

static PyObject *
api_folder_open (PyObject *self, PyObject *args)
{
  int status;
  PyFolder *py_folder;

  if (!PyArg_ParseTuple (args, "O!", &PyFolderType, &py_folder))
    return NULL;

  status = mu_folder_open (py_folder->folder, MU_STREAM_READ);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_folder_close (PyObject *self, PyObject *args)
{
  int status;
  PyFolder *py_folder;

  if (!PyArg_ParseTuple (args, "O!", &PyFolderType, &py_folder))
    return NULL;

  status = mu_folder_close (py_folder->folder);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_folder_get_authority (PyObject *self, PyObject *args)
{
  int status;
  PyFolder *py_folder;
  PyAuthority *py_auth = PyAuthority_NEW ();

  if (!PyArg_ParseTuple (args, "O!", &PyFolderType, &py_folder))
    return NULL;

  Py_INCREF (py_auth);

  status = mu_folder_get_authority (py_folder->folder, &py_auth->auth);
  return status_object (status, (PyObject *)py_auth);
}

static PyObject *
api_folder_set_authority (PyObject *self, PyObject *args)
{
  int status;
  PyFolder *py_folder;
  PyAuthority *py_auth;

  if (!PyArg_ParseTuple (args, "O!O", &PyFolderType, &py_folder, &py_auth))
    return NULL;

  if (!PyAuthority_Check ((PyObject *)py_auth))
    {
      PyErr_SetString (PyExc_TypeError, "");
      return NULL;
    }

  status = mu_folder_set_authority (py_folder->folder, py_auth->auth);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_folder_get_url (PyObject *self, PyObject *args)
{
  int status;
  PyFolder *py_folder;
  PyUrl *py_url = PyUrl_NEW ();

  if (!PyArg_ParseTuple (args, "O!", &PyFolderType, &py_folder))
    return NULL;

  Py_INCREF (py_url);

  status = mu_folder_get_url (py_folder->folder, &py_url->url);
  return status_object (status, (PyObject *)py_url);
}

static int
folderdata_extractor (void *data, PyObject **dst)
{
  struct mu_list_response *resp = (struct mu_list_response *)data;
  char separator[4];

  char *attr = (resp->type & MU_FOLDER_ATTRIBUTE_DIRECTORY) ? "d" :
    (resp->type & MU_FOLDER_ATTRIBUTE_FILE) ? "f" : "-";

  snprintf (separator, sizeof (separator), "%c", resp->separator);

  *dst = PyTuple_New (4);
  PyTuple_SetItem (*dst, 0, PyUnicode_FromString (attr));
  PyTuple_SetItem (*dst, 1, PyLong_FromLong (resp->level));
  PyTuple_SetItem (*dst, 2, PyUnicode_FromString (separator));
  PyTuple_SetItem (*dst, 3, PyUnicode_FromString (resp->name));
  return 0;
}

static PyObject *
api_folder_list (PyObject *self, PyObject *args)
{
  int status = 0;
  Py_ssize_t max_level = 0;
  char *dirname, *pattern;
  PyFolder *py_folder;
  PyObject *py_list;
  mu_list_t c_list = NULL;

  if (!PyArg_ParseTuple (args, "O!zs|n", &PyFolderType, &py_folder,
			 &dirname, &pattern, &max_level))
    return NULL;
  if (max_level < 0)
    {
      PyErr_SetString (PyExc_RuntimeError, "max level out of range");
      return NULL;
    }
  status = mu_folder_list (py_folder->folder, dirname, pattern, max_level,
			   &c_list);

  if (c_list)
    py_list = mu_py_mulist_to_pylist (c_list, folderdata_extractor);
  else
    py_list = PyTuple_New (0);

  return status_object (status, py_list);
}

static PyMethodDef methods[] = {
  { "create", (PyCFunction) api_folder_create, METH_VARARGS,
    "" },

  { "destroy", (PyCFunction) api_folder_destroy, METH_VARARGS,
    "" },

  { "open", (PyCFunction) api_folder_open, METH_VARARGS,
    "" },

  { "close", (PyCFunction) api_folder_close, METH_VARARGS,
    "" },

  { "get_authority", (PyCFunction) api_folder_get_authority, METH_VARARGS,
    "" },

  { "set_authority", (PyCFunction) api_folder_set_authority, METH_VARARGS,
    "" },

  { "get_url", (PyCFunction) api_folder_get_url, METH_VARARGS,
    "" },

  { "list", (PyCFunction) api_folder_list, METH_VARARGS,
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
mu_py_init_folder (void)
{
  PyFolderType.tp_new = PyType_GenericNew;
  return PyType_Ready (&PyFolderType);
}

void
_mu_py_attach_folder (void)
{
  PyObject *m;
  if ((m = _mu_py_attach_module (&moduledef)))
    {
      Py_INCREF (&PyFolderType);
      PyModule_AddObject (m, PY_CSNAME, (PyObject *)&PyFolderType);
    }
}
