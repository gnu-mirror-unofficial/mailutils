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

void
mu_py_script_init (int argc, char *argv[])
{
  wchar_t **wargv;
  int i;
  
  if (!Py_IsInitialized ())
    Py_Initialize ();
  wargv = PyMem_Calloc (argc, sizeof (wargv[0]));
  if (!wargv)
    {
      PyErr_SetNone (PyExc_MemoryError);
      return; /* FIXME */
    }
  for (i = 0; i < argc; i++)
    {
      wargv[i] = Py_DecodeLocale (argv[i], NULL);
      if (!wargv[i])
	{
	  PyErr_SetNone (PyExc_MemoryError);
	  return; /* FIXME */
	}
    }
  PySys_SetArgv (argc, wargv);
}

void
mu_py_script_finish (void)
{
  Py_Finalize ();
}

static PyMethodDef nomethods[] = {
  { NULL, NULL }
};

int
mu_py_script_run (const char *python_filename, mu_py_script_data *data)
{
  FILE *fp;
  PyObject *py_module, *sysmodules;
  struct PyModuleDef moddef = { .m_base = PyModuleDef_HEAD_INIT };
  
  if (!python_filename)
    return MU_ERR_OUT_PTR_NULL;

  fp = fopen (python_filename, "r");
  if (!fp)
    return errno;

  moddef.m_name = data->module_name;
  moddef.m_doc = "";
  moddef.m_size = -1;
  moddef.m_methods = nomethods;
  
  py_module = PyModule_Create (&moddef);
  if (!py_module)
    return MU_ERR_FAILURE;

  for (; data->attrs->name; data->attrs++)
    PyObject_SetAttrString (py_module, data->attrs->name, data->attrs->obj);

  sysmodules = PyImport_GetModuleDict ();
  PyMapping_SetItemString (sysmodules, moddef.m_name, py_module);
  
  if (PyRun_SimpleFile (fp, python_filename))
    return MU_ERR_FAILURE;

  fclose (fp);
  return 0;
}

int
mu_py_script_process_mailbox (int argc, char *argv[],
			      const char *python_filename,
			      const char *module_name,
			      mu_mailbox_t mbox)
{
  int status;
  PyMailbox *py_mbox;
  mu_py_dict dict[2];
  mu_py_script_data data[1];

  mu_py_script_init (argc, argv);

  py_mbox = PyMailbox_NEW ();
  py_mbox->mbox = mbox;
  Py_INCREF (py_mbox);

  dict[0].name = "mailbox";
  dict[0].obj  = (PyObject *)py_mbox;
  dict[1].name = NULL;

  data[0].module_name = module_name;
  data[0].attrs = dict;

  status = mu_py_script_run (python_filename, data);
  mu_py_script_finish ();
  return status;
}
