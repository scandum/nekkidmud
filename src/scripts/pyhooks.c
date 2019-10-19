//*****************************************************************************
//
// pyhooks.c
//
// The wrapper around hooks for Python to interact with them.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../hooks.h"

#include "scripts.h"
#include "pyroom.h"
#include "pychar.h"
#include "pyobj.h"
#include "pyexit.h"
#include "pysocket.h"
#include "pyplugs.h"
#include "pyhooks.h"



//*****************************************************************************
// local variables and functions
//*****************************************************************************

// a list of methods to add to the hooks module
LIST *pyhooks_methods = NULL;

// a table of python hooks we have installed
HASHTABLE *pyhook_table = NULL;



//*****************************************************************************
// local methods
//*****************************************************************************
PyObject *PyHooks_BuildInfo(PyObject *self, PyObject *args) {
  // parse out our variables
  char           *format = NULL;
  PyObject         *vars = NULL;
  if(!PyArg_ParseTuple(args, "sO", &format, &vars)) {
    PyErr_Format(PyExc_TypeError,"PyHook_BuildInfo requies format and tuple.");
    return NULL;
  }

  // make sure we have a tuple
  if(!PyTuple_Check(vars)) {
    PyErr_Format(PyExc_TypeError, "Second argument to PyHooks_buildInfo must "
		 "be a tuple.");
    return NULL;
  }

  LIST *tokens = parse_strings(format, ' ');
  // make sure our tuple and tokens are of the same length
  if(listSize(tokens) != PyTuple_Size(vars)) {
    PyErr_Format(PyExc_TypeError, "unequal number of tokens and values.");
    deleteListWith(tokens, free);
    return NULL;
  }

  // build each of our tokens into the info
  BUFFER            *buf = newBuffer(1);
  LIST_ITERATOR *token_i = newListIterator(tokens);
  char            *token = NULL;
  int                  i = 0;
  PyObject          *var = NULL;
  ITERATE_LIST(token, token_i) {
    if(!strcasecmp(token, "ch")) {
      var = PyTuple_GetItem(vars, i);
      if(!PyChar_Check(var)) {
	PyErr_Format(PyExc_TypeError, "arg %d was not Char", i);
	break;
      }
      bprintf(buf, "ch.%d", PyChar_AsUid(var));
    }
    else if(!strcasecmp(token, "obj")) {
      var = PyTuple_GetItem(vars, i);
      if(!PyObj_Check(var)) {
	PyErr_Format(PyExc_TypeError, "arg %d was not Obj", i);
	break;
      }
      bprintf(buf, "obj.%d", PyObj_AsUid(var));
    }
    else if(!strcasecmp(token, "rm") || !strcasecmp(token, "room")) {
      var = PyTuple_GetItem(vars, i);
      if(!PyRoom_Check(var)) {
	PyErr_Format(PyExc_TypeError, "arg %d was not Room", i);
	break;
      }
      bprintf(buf, "rm.%d", PyRoom_AsUid(var));
    }
    else if(!strcasecmp(token, "ex") || !strcasecmp(token, "exit")) {
      var = PyTuple_GetItem(vars, i);
      if(!PyExit_Check(var)) {
	PyErr_Format(PyExc_TypeError, "arg %d was not Exit", i);
	break;
      }
      bprintf(buf, "ex.%d", PyExit_AsUid(var));
    }
    else if(!strcasecmp(token, "sk") || !strcasecmp(token, "sock")) {
      var = PyTuple_GetItem(vars, i);
      if(!PySocket_Check(var)) {
	PyErr_Format(PyExc_TypeError, "arg %d was not Mudsock", i);
	break;
      }
      bprintf(buf, "sk.%d", PySocket_AsUid(var));
    }
    else if(!strcasecmp(token, "str")) {
      var = PyTuple_GetItem(vars, i);
      if(!PyString_Check(var)) {
	PyErr_Format(PyExc_TypeError, "arg %d was not String", i);
	break;
      }
      bprintf(buf, "%c%s%c", HOOK_STR_MARKER, PyString_AsString(var),
	      HOOK_STR_MARKER);
    }
    else if(!strcasecmp(token, "int")) {
      var = PyTuple_GetItem(vars, i);
      if(!PyInt_Check(var)) {
	PyErr_Format(PyExc_TypeError, "arg %d was not Integer", i);
	break;
      }
      bprintf(buf, "%ld", PyInt_AsLong(var));
    }
    else if(!strcasecmp(token, "dbl")) {
      var = PyTuple_GetItem(vars, i);
      if(!PyFloat_Check(var)) {
	PyErr_Format(PyExc_TypeError, "arg %d was not Float", i);
	break;
      }
      bprintf(buf, "%lf", PyFloat_AsDouble(var));
    }

    // put a space in for our next variable if needed
    if(i < listSize(tokens) - 1)
      bprintf(buf, " ");
    i++;
  } deleteListIterator(token_i);
  deleteListWith(tokens, free);

  // did we manage to go through all of our tokens or not?
  if(i != PyTuple_Size(vars)) {
    deleteBuffer(buf);
    return NULL;
  }
  else {
    PyObject *retval = Py_BuildValue("s", bufferString(buf));
    deleteBuffer(buf);
    return retval;
  }
}


PyObject *PyHooks_ParseInfo(PyObject *self, PyObject *args) {
  // parse out our info
  char             *info = NULL;
  if(!PyArg_ParseTuple(args, "s", &info)) {
    PyErr_Format(PyExc_TypeError, "PyHook_ParseInfo must be supplied a string");
    return NULL;
  }

  // parse out all of our tokens
  LIST           *tokens = parse_hook_info_tokens(info);
  LIST_ITERATOR *token_i = newListIterator(tokens);
  char            *token = NULL;
  PyObject         *list = PyTuple_New(listSize(tokens));
  int                  i = 0;

  // id number we'll need for parsing some values
  int id = 0;

  // go through all of our tokens
  ITERATE_LIST(token, token_i) {
    if(startswith(token, "ch")) {
      sscanf(token, "ch.%d", &id);
      CHAR_DATA *ch = propertyTableGet(mob_table, id);
      PyTuple_SetItem(list, i, (ch ? charGetPyForm(ch) : Py_None));
    }
    else if(startswith(token, "obj")) {
      sscanf(token, "obj.%d", &id);
      OBJ_DATA *obj = propertyTableGet(obj_table, id);
      PyTuple_SetItem(list, i, (obj ? objGetPyForm(obj) : Py_None));
    }
    else if(startswith(token, "rm")) {
      sscanf(token, "rm.%d", &id);
      ROOM_DATA *rm = propertyTableGet(room_table, id);
      PyTuple_SetItem(list, i, (rm ? roomGetPyForm(rm) : Py_None));
    }
    else if(startswith(token, "room")) {
      sscanf(token, "room.%d", &id);
      ROOM_DATA *rm = propertyTableGet(room_table, id);
      PyTuple_SetItem(list, i, (rm ? roomGetPyForm(rm) : Py_None));
    }
    else if(startswith(token, "exit")) {
      sscanf(token, "exit.%d", &id);
      EXIT_DATA *ex = propertyTableGet(exit_table, id);
      PyTuple_SetItem(list, i, (ex ? newPyExit(ex) : Py_None));
    }
    else if(startswith(token, "ex")) {
      sscanf(token, "ex.%d", &id);
      EXIT_DATA *ex = propertyTableGet(exit_table, id);
      PyTuple_SetItem(list, i, (ex ? newPyExit(ex) : Py_None));
    }
    else if(startswith(token, "sk")) {
      sscanf(token, "sk.%d", &id);
      SOCKET_DATA *sock = propertyTableGet(sock_table, id);
      PyTuple_SetItem(list,i, (sock ? socketGetPyForm(sock) : Py_None));
    }
    else if(startswith(token, "sock")) {
      sscanf(token, "sock.%d", &id);
      SOCKET_DATA *sock = propertyTableGet(sock_table, id);
      PyTuple_SetItem(list,i, (sock ? socketGetPyForm(sock) : Py_None));
    }
    else if(*token == HOOK_STR_MARKER) {
      char *str = strdup(token + 1);
      str[strlen(str)-1] = '\0';
      PyTuple_SetItem(list,i, Py_BuildValue("s", str));
      free(str);
    }
    else if(isdigit(*token)) {
      // integer or double?
      if(next_letter_in(token, '.') > -1)
	PyTuple_SetItem(list, i, Py_BuildValue("d", atof(token)));
      else
	PyTuple_SetItem(list,i, Py_BuildValue("i", atoi(token)));
    }
    i++;
  } deleteListIterator(token_i);
  deleteListWith(tokens, free);

  return list;
}


PyObject *PyHooks_Run(PyObject *self, PyObject *args) {
  char *type = NULL;
  char *info = NULL;
  if(!PyArg_ParseTuple(args, "ss", &type, &info)) {
    PyErr_Format(PyExc_TypeError, "A string type and info must be supplied");
    return NULL;
  }

  // run the hook
  hookRun(type, info);
  return Py_BuildValue("i", 1);
}


PyObject *PyHooks_Add(PyObject *self, PyObject *args) {
  char     *type = NULL;
  PyObject *hook = NULL;
  if(!PyArg_ParseTuple(args, "sO", &type, &hook)) {
    PyErr_Format(PyExc_TypeError, "Must supply with a type and function");
    return NULL;
  }

  if(!PyFunction_Check(hook)) {
    PyErr_Format(PyExc_TypeError, "Only functions may be supplied as hooks.");
    return NULL;
  }

  Py_INCREF(hook);
  LIST *list = hashGet(pyhook_table, type);
  if(list == NULL) {
    list = newList();
    hashPut(pyhook_table, type, list);
  }
  listQueue(list, hook);  
  return Py_BuildValue("i", 1);
}


PyObject *PyHooks_Remove(PyObject *self, PyObject *args) {
  char     *type = NULL;
  PyObject *hook = NULL;
  if(!PyArg_ParseTuple(args, "sO", &type, &hook)) {
    PyErr_Format(PyExc_TypeError, "Must supply with a type and function");
    return NULL;
  }

  LIST *list = hashGet(pyhook_table, type);
  if(list != NULL && listRemove(list, hook)) {
    Py_DECREF(hook);
  }
  return Py_BuildValue("i", 1);
}


//
// monitors hook activity, and handles the ones on the Python end
void PyHooks_Monitor(const char *type, const char *info) {
  LIST *list = hashGet(pyhook_table, type);
  if(list != NULL) {
    char *info_dup = strdup(info);
    LIST_ITERATOR *list_i = newListIterator(list);
    PyObject *func = NULL;
    ITERATE_LIST(func, list_i) {
      PyObject *arglist = Py_BuildValue("(s)", info_dup);
      PyObject *retval  = PyEval_CallObject(func, arglist);
      // check for an error:
      if(retval == NULL)
	log_pyerr("Error running Python hook");

      // garbage collection
      Py_XDECREF(retval);
      Py_XDECREF(arglist);
    } deleteListIterator(list_i);
    free(info_dup);
  }
}



//*****************************************************************************
// Hooks module
//*****************************************************************************
PyMODINIT_FUNC
init_PyHooks(void) {
  PyHooks_addMethod("parse_info", PyHooks_ParseInfo, METH_VARARGS,
    "parse_info(info)\n\n"
    "Returns a tuple of parsed hook information.");
  PyHooks_addMethod("build_info", PyHooks_BuildInfo, METH_VARARGS,
    "build_info(format, args)\n\n"
    "Returns hook information from a string format and a tuple of values for\n"
    "the format. Format arguments must be space-separated. They include:\n"
    "ch, rm, obj, ex, sk, str, int, dbl.");
  PyHooks_addMethod("run", PyHooks_Run, METH_VARARGS,
    "run(hooktypes)\n\n"
    "Runs hooks registered to the given type.");
  PyHooks_addMethod("add", PyHooks_Add, METH_VARARGS,
    "add(type, function)\n\n"
    "Register a new hook function. Hook functions should take one argument:\n"
    "an information string that can be parsed with hooks.parse_info");
  PyHooks_addMethod("remove", PyHooks_Remove, METH_VARARGS,
    "remove(type, function)\n\n"
    "Unregister a hook function.");

  Py_InitModule3("hooks", makePyMethods(pyhooks_methods),
    "The python module for registering and running hooks.");

  // set up our hook monitor
  pyhook_table = newHashtable();
  hookAddMonitor(PyHooks_Monitor);
}

void PyHooks_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pyhooks_methods == NULL) pyhooks_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pyhooks_methods, def);
}
