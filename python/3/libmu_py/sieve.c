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

#define PY_MODULE "sieve"
#define PY_CSNAME "SieveMachineType"

static PyObject *
_repr (PyObject *self)
{
  char buf[80];
  sprintf (buf, "<" PY_MODULE "." PY_CSNAME " instance at %p>", self);
  return PyUnicode_FromString (buf);
}

static PyTypeObject PySieveMachineType = {
  .ob_base = { PyObject_HEAD_INIT(NULL) },
  .tp_name = PY_MODULE "." PY_CSNAME,
  .tp_basicsize = sizeof (PySieveMachine),
  .tp_dealloc = (destructor)_py_dealloc,
  .tp_repr = _repr,
  .tp_str = _repr,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "",
};

PySieveMachine *
PySieveMachine_NEW ()
{
  return (PySieveMachine *)PyObject_NEW (PySieveMachine, &PySieveMachineType);
}

int
PySieveMachine_Check (PyObject *x)
{
  return x->ob_type == &PySieveMachineType;
}

struct _mu_py_sieve_logger {
  PyObject *py_debug_printer;
  PyObject *py_error_printer;
  PyObject *py_parse_error_printer;
  PyObject *py_action_printer;
};

static PyObject *
api_sieve_machine_init (PyObject *self, PyObject *args)
{
  int status;
  PySieveMachine *py_mach;
  mu_stream_t str, estr;
  
  if (!PyArg_ParseTuple (args, "O!", &PySieveMachineType, &py_mach))
    return NULL;

  status = mu_memory_stream_create (&str, MU_STREAM_RDWR);
  if (status)
    return _ro (PyLong_FromLong (status));
  status = mu_log_stream_create (&estr, str);
  mu_stream_unref (str);
  if (status)
    return _ro (PyLong_FromLong (status));
  
  status = mu_sieve_machine_create (&py_mach->mach);
  if (status == 0)
    mu_sieve_set_diag_stream (py_mach->mach, estr);
  mu_stream_unref (estr);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_sieve_machine_error_text (PyObject *self, PyObject *args)
{
  int status;
  PySieveMachine *py_mach;
  mu_stream_t estr;
  mu_transport_t trans[2];
  PyObject *retval;
  mu_off_t length = 0;
  
  if (!PyArg_ParseTuple (args, "O!", &PySieveMachineType, &py_mach))
    return NULL;
  if (!py_mach->mach)
    PyErr_SetString (PyExc_RuntimeError, "Uninitialized Sieve machine");
      
  mu_sieve_get_diag_stream (py_mach->mach, &estr);
  status = mu_stream_ioctl (estr, MU_IOCTL_TRANSPORT, MU_IOCTL_OP_GET,
			    trans);
  if (status == 0)
    {
      mu_stream_t str = (mu_stream_t) trans[0];

      mu_stream_size (str, &length);
      status = mu_stream_ioctl (str, MU_IOCTL_TRANSPORT, MU_IOCTL_OP_GET,
				trans);
      mu_stream_truncate (str, 0);
    }
  
  if (status)
    PyErr_SetString (PyExc_RuntimeError, mu_strerror (status));
  retval = PyUnicode_FromStringAndSize ((char*) trans[0], length);

  mu_stream_unref (estr);
  return _ro (retval);
}

static PyObject *
api_sieve_machine_destroy (PyObject *self, PyObject *args)
{
  PySieveMachine *py_mach;

  if (!PyArg_ParseTuple (args, "O!", &PySieveMachineType, &py_mach))
    return NULL;

  if (py_mach->mach)
    {
      struct _mu_py_sieve_logger *s = mu_sieve_get_data (py_mach->mach);
      if (s)
	free (s);
      mu_sieve_machine_destroy (&py_mach->mach);
    }
  return _ro (Py_None);
}

static PyObject *
api_sieve_compile (PyObject *self, PyObject *args)
{
  int status;
  char *name;
  PySieveMachine *py_mach;

  if (!PyArg_ParseTuple (args, "O!s", &PySieveMachineType, &py_mach, &name))
    return NULL;

  status = mu_sieve_compile (py_mach->mach, name);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_sieve_disass (PyObject *self, PyObject *args)
{
  int status;
  PySieveMachine *py_mach;

  if (!PyArg_ParseTuple (args, "O!", &PySieveMachineType, &py_mach))
    return NULL;

  status = mu_sieve_disass (py_mach->mach);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_sieve_mailbox (PyObject *self, PyObject *args)
{
  int status;
  PySieveMachine *py_mach;
  PyMailbox *py_mbox;

  if (!PyArg_ParseTuple (args, "O!O", &PySieveMachineType, &py_mach, &py_mbox))
    return NULL;

  if (!PyMailbox_Check ((PyObject *)py_mbox))
    {
      PyErr_SetString (PyExc_TypeError, "");
      return NULL;
    }

  status = mu_sieve_mailbox (py_mach->mach, py_mbox->mbox);
  return _ro (PyLong_FromLong (status));
}

static PyObject *
api_sieve_message (PyObject *self, PyObject *args)
{
  int status;
  PySieveMachine *py_mach;
  PyMessage *py_msg;

  if (!PyArg_ParseTuple (args, "O!O", &PySieveMachineType, &py_mach, &py_msg))
    return NULL;

  if (!PyMessage_Check ((PyObject *)py_msg))
    {
      PyErr_SetString (PyExc_TypeError, "");
      return NULL;
    }

  status = mu_sieve_message (py_mach->mach, py_msg->msg);
  return _ro (PyLong_FromLong (status));
}

static void
_sieve_action_printer (mu_sieve_machine_t mach,
		       const char *action, const char *fmt, va_list ap)
{
  PyObject *py_args;
  PyObject *py_dict = PyDict_New ();
  PyStream *py_stm;

  if (!py_dict)
    return;
  py_stm = PyStream_NEW ();
  if (py_stm)
    {
      PyMessage *py_msg = PyMessage_NEW ();
      char *buf = NULL;
      size_t buflen = 0;

      if (py_msg)
	{
	  PyStream *py_stm = PyStream_NEW ();
	  if (py_stm)
	    {
	      size_t msgno;
	      
	      mu_sieve_get_diag_stream (mach, &py_stm->stm);
	      msgno = mu_sieve_get_message_num (mach);
	      
	      py_msg->msg = mu_sieve_get_message (mach);
	      Py_INCREF (py_msg);

	      PyDict_SetItemString (py_dict, "msgno",
				    PyLong_FromSize_t (msgno));
	      PyDict_SetItemString (py_dict, "msg", (PyObject *)py_msg);
	      PyDict_SetItemString (py_dict, "action",
				    PyUnicode_FromString (mu_prstr (action)));

	      if (mu_vasnprintf (&buf, &buflen, fmt, ap))
		{
		  mu_stream_destroy (&py_stm->stm);
		  return;
		}
	      PyDict_SetItemString (py_dict, "text",
				    PyUnicode_FromString (mu_prstr (buf)));
	      free (buf);

	      py_args = PyTuple_New (1);
	      if (py_args)
		{
		  struct _mu_py_sieve_logger *s = mu_sieve_get_data (mach);
		  PyObject *py_fnc = s->py_action_printer;

		  Py_INCREF (py_dict);
		  PyTuple_SetItem (py_args, 0, py_dict);
		  
		  if (py_fnc && PyCallable_Check (py_fnc))
		    PyObject_CallObject (py_fnc, py_args);

		  Py_DECREF (py_dict);
		  Py_DECREF (py_args);
		}
	    }
	}
    }
}

static PyObject *
api_sieve_set_logger (PyObject *self, PyObject *args)
{
  PySieveMachine *py_mach;
  PyObject *py_fnc;

  if (!PyArg_ParseTuple (args, "O!O", &PySieveMachineType, &py_mach, &py_fnc))
    return NULL;

  if (py_fnc && PyCallable_Check (py_fnc)) {
    mu_sieve_machine_t mach = py_mach->mach;
    struct _mu_py_sieve_logger *s = mu_sieve_get_data (mach);
    s->py_action_printer = py_fnc;
    Py_INCREF (py_fnc);
    mu_sieve_set_logger (py_mach->mach, _sieve_action_printer);
  }
  else {
      PyErr_SetString (PyExc_TypeError, "");
      return NULL;
  }
  return _ro (Py_None);
}

static PyMethodDef methods[] = {
  { "machine_init", (PyCFunction) api_sieve_machine_init, METH_VARARGS,
    "Create and initialize new Sieve 'machine'." },

  { "machine_destroy", (PyCFunction) api_sieve_machine_destroy, METH_VARARGS,
    "Destroy Sieve 'machine'." },

  { "errortext", (PyCFunction) api_sieve_machine_error_text, METH_VARARGS,
    "Return the most recent error description." },
  
  { "compile", (PyCFunction) api_sieve_compile, METH_VARARGS,
    "Compile the sieve script from the file 'name'." },

  { "disass", (PyCFunction) api_sieve_disass, METH_VARARGS,
    "Dump the disassembled code of the sieve machine 'mach'." },

  { "mailbox", (PyCFunction) api_sieve_mailbox, METH_VARARGS,
    "Execute the code from the given instance of sieve machine "
    "'mach' over each message in the mailbox 'mbox'." },

  { "message", (PyCFunction) api_sieve_message, METH_VARARGS,
    "Execute the code from the given instance of sieve machine "
    "'mach' over the 'message'. " },

  { "set_logger", (PyCFunction) api_sieve_set_logger, METH_VARARGS,
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
mu_py_init_sieve (void)
{
  PySieveMachineType.tp_new = PyType_GenericNew;
  return PyType_Ready (&PySieveMachineType);
}

void
_mu_py_attach_sieve (void)
{
  PyObject *m;
  if ((m = _mu_py_attach_module (&moduledef)))
    {
      Py_INCREF (&PySieveMachineType);
      PyModule_AddObject (m, PY_CSNAME, (PyObject *)&PySieveMachineType);
    }
}
