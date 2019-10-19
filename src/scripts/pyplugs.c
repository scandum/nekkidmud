//*****************************************************************************
//
// pyplugs.c
//
// Pyplugs are just various things that occur at the intersection between 
// Python and C, in NakedMud. They include the loading of python modules,
// a few commands for interacting with python modules, and miscellaneory (sp?)
// python stuff that doesn't neccessarily fall under "scripting".
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>
#include <compile.h>
#include <dirent.h> 
#include <node.h>

#include "../mud.h"
#include "../utils.h"
#include "../character.h"
#include "pyplugs.h"



//*****************************************************************************
// local functions, structures, and defines
//*****************************************************************************

// the directory where we keep python modules that we have extended ourself with
#define PYMOD_LIB       "../lib/pymodules"


//
// Similar to Py_CompileString (in the Python API), but a file is compiled
// instead. fname is the file that will be compiled. The code object will be
// stored in a file named fname + c (e.g. foo.py becomes foo.pyc)
PyObject *Py_CompileFile(char *fname) {
  FILE *fl = fopen(fname, "r");
  if(fl == NULL) return NULL;
  char cfname[strlen(fname) + 2];
  sprintf(cfname, "%sc", fname);

  struct _node *node = PyParser_SimpleParseFile(fl, fname, Py_file_input);
  if(node == NULL) return NULL;
  PyCodeObject *code = PyNode_Compile(node, cfname);
  PyNode_Free(node);
  fclose(fl);
  return (PyObject *)code;
}


//
// reads in one python module, and adds it to the game. Fname is the name of
// the file we are reading the module from, and mname is the name of the module
// we will be storing it as. This function will reload the module if it has
// already been loaded
bool PyModule_Reload(char *fname, char *mname) {
  PyObject *code = Py_CompileFile(fname);
  if(code == NULL) {
    log_pyerr("Error in module file: %s", fname);
    return FALSE;
  }
  // no errors occured... load the module into our package
  else {
    // we need to run an unload function if the module has been loaded before
    PyObject *old_mod = PyImport_ImportModule(mname);
    if(old_mod != NULL) {
      PyObject *dict = PyModule_GetDict(old_mod);
      
      // if the module has an __unload__ function, call it
      if(PyDict_GetItemString(dict, "__unload__") != NULL) {
	PyObject *tbList = PyObject_CallMethod(old_mod, "__unload__", "");

	// encountered an error with the unload function
	if(tbList == NULL)
	  log_pyerr("Encountered error in %s.__unload__()", mname);
	Py_XDECREF(tbList);
      }
      Py_XDECREF(old_mod);
    }

    PyObject *module = PyImport_ExecCodeModule(mname, code);
    if(module == NULL) {
      log_pyerr("Error in module file: %s", fname);
      return FALSE;
    }
    else
      Py_DECREF(module);
    Py_DECREF(code);
    return TRUE;
  }
}

//
// takes the name of a python module, and loads that module into the game
COMMAND(cmd_pyload) {
  if(!*arg)
    send_to_char(ch, "Which module or package would you like to load?\r\n");
  else {
    send_to_char(ch, "{rpyload is unsafe and has been deprecated.{n\r\n");
    char fname[SMALL_BUFFER];
    sprintf(fname, "%s/%s.py", PYMOD_LIB, arg);
    // make sure the file exists
    if(!file_exists(fname))
      send_to_char(ch, "That module does not exist!\r\n");
    else if(PyModule_Reload(fname, arg))
      send_to_char(ch, "Module successfully loaded.\r\n");
  }
}

//
// NakedMud allows you to extend the codebase in Python. These extensions must
// take the form of Python modules, and must be stored in the PYMOD_LIB
// directory. These modules might add new commands to the mud, or provide new
// functions that scripts or other extension modules can use. The game datatypes
// (i.e. chars, rooms, objects, etc...) cannot be extended by Python, but just
// about any other feature of the mud can be.
void init_py_modules() {
  // build a list of all the files in this directory
  char mname[SMALL_BUFFER]; // module name
  char fname[SMALL_BUFFER]; // the name of the file
  DIR *dir = opendir(PYMOD_LIB);
  struct dirent *entry;

  // add our PYMOD_LIB directory to the sys path, 
  // so the modules can access each other
  PyObject *sys  = PyImport_ImportModule("sys");
  PyObject *path = (sys ? PyDict_GetItemString(PyModule_GetDict(sys), "path") :
		    NULL);
  if(path != NULL)
    PyList_Append(path, Py_BuildValue("s", PYMOD_LIB));
  else
    log_string("ERROR: Unable to add %s to python sys.path", PYMOD_LIB);

  // go through each of our python modules, and add them to the pymod package
  for(entry = readdir(dir); entry; entry = readdir(dir)) {
    // two cases: it's a package, or it's a module file. Packages will
    // be directories, and modules will be .py files. Check for both cases,
    // and then ignore all of the rest:
    int    nlen = strlen(entry->d_name);
    sprintf(fname, "%s/%s", PYMOD_LIB, entry->d_name);

    // skip hidden files, ourself, and our parent
    if(*entry->d_name == '.')
      continue;
    // python file == module
    else if(nlen >= 4 && !strcasecmp(".py", entry->d_name + nlen-3)) {
      sprintf(mname, "%s", entry->d_name);
      mname[strlen(mname)-3] = '\0';
    }
    // directory == package
    else if(dir_exists(fname))
      sprintf(mname, "%s", entry->d_name);
    // nothing we can use
    else
      continue;

    // Load the module if it hasn't been loaded yet
    PyObject *mod = PyImport_ImportModule(mname);
    if(mod != NULL)
      log_string("loading python module, %s", mname);
    // oops... something went wrong. Let's get the traceback
    else {
      // we now abandon bootup if we fail to load a Python module, because it
      // can have potentially dangerous affects on the loading of other modules
      // Thanks to Thirsteh for pointing this out.
      log_pyerr("Error loading module, %s:", mname);
      log_string("Bootup aborted. MUD shutting down.");
      closedir(dir);
      exit(1);
    }
  }
  closedir(dir);
}



//*****************************************************************************
// implementation of pyplugs.h
//*****************************************************************************
void init_pyplugs(void) {
  init_py_modules();
  add_cmd("pyload", NULL, cmd_pyload, "admin", FALSE);
}



//
// Python makes it overly complicated (IMO) to get traceback information when
// running in C. I spent some time hunting around, and came across a lovely
// site that provided me with this function for getting a traceback of an error.
// The site that provided this piece of code to me is located here:
//   http://stompstompstomp.com/weblog/technical/2004-03-29
char* getPythonTraceback(void) {
    // Python equivilant:
    // import traceback, sys
    // return "".join(traceback.format_exception(sys.exc_type, 
    //    sys.exc_value, sys.exc_traceback))

    PyObject *type, *value, *traceback;
    PyObject *tracebackModule;
    char *chrRetval;

    PyErr_Fetch(&type, &value, &traceback);

    tracebackModule = PyImport_ImportModule("traceback");
    if (tracebackModule != NULL) {
        PyObject *tbList, *emptyString, *strRetval;

        tbList = PyObject_CallMethod(
            tracebackModule, 
            "format_exception", 
            "OOO",
            type,
            value == NULL ? Py_None : value,
            traceback == NULL ? Py_None : traceback);

        emptyString = PyString_FromString("");
        strRetval = PyObject_CallMethod(emptyString, "join", "O", tbList);

        chrRetval = strdup((strRetval ? PyString_AsString(strRetval) :
			    "unknown error"));

        Py_XDECREF(tbList);
        Py_XDECREF(emptyString);
        Py_XDECREF(strRetval);
        Py_XDECREF(tracebackModule);
    }
    else
      chrRetval = strdup("Unable to import traceback module.");

    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);

    return chrRetval;
}

void log_pyerr(const char *format, ...) {
  // get the traceback, and print our message
  char *tb = getPythonTraceback();
  if(tb != NULL) {
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, MAX_BUFFER, format, args);
    va_end(args);

    log_string("%s\r\n\r\n%s\r\n", buf, tb);
    free(tb);
  }
}


PyGetSetDef *makePyGetSetters(LIST *getsetters) {
  PyGetSetDef *getsets = calloc(listSize(getsetters)+1,sizeof(PyGetSetDef));
  LIST_ITERATOR  *gs_i = newListIterator(getsetters);
  PyGetSetDef      *gs = NULL;
  int i                = 0;
  ITERATE_LIST(gs, gs_i) {
    getsets[i] = *gs;
    i++;
  } deleteListIterator(gs_i);
  return getsets;
}

PyMethodDef *makePyMethods(LIST *methods) {
  PyMethodDef  *meth = calloc(listSize(methods)+1, sizeof(PyMethodDef));
  LIST_ITERATOR *m_i = newListIterator(methods);
  PyMethodDef     *m = NULL;
  int i              = 0;
  ITERATE_LIST(m, m_i) {
    meth[i] = *m;
    i++;
  } deleteListIterator(m_i);
  return meth;
}

void makePyType(PyTypeObject *type, LIST *getsetters, LIST *methods) {
  // build up the array of getsetters for this object
  if(getsetters != NULL)
    type->tp_getset = makePyGetSetters(getsetters);
  
  // build up the array of methods for this object
  if(methods != NULL)
    type->tp_methods = makePyMethods(methods);
}
