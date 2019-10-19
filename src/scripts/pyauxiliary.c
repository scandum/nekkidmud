//*****************************************************************************
//
// pyauxiliary.c
//
// This is a wrapper around the auxiliary data system so python can make use
// of it. Allows python modules to install new auxiliary data on characters,
// rooms, objects, etc...
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../utils.h"
#include "../storage.h"
#include "../auxiliary.h"

#include "pystorage.h"
#include "pyplugs.h"



//*****************************************************************************
// local variables and datastructures
//*****************************************************************************

// the minimum size of our aux table
#define MIN_AUX_TABLE_SIZE       50

// holds a map from auxiliary data names to their prototype
HASHTABLE *aux_table = NULL;



//*****************************************************************************
// the wrapper layer between python and C auxiliary data
//*****************************************************************************
PyObject *newPyAuxiliaryData(const char *keyword) {
  // look up the prototype
  PyObject *proto = hashGet(aux_table, keyword);
  
  // make sure we've got an OK proto
  if(proto == NULL)
    return NULL;
  else {
    // create a new instance of the proto
    PyObject *instance = PyInstance_New(proto, NULL, NULL);
    if(instance == NULL)
      log_pyerr("Error with Python auxiliary new");
    return instance;
  }
}

void deletePyAuxiliaryData(PyObject *data) {
  Py_XDECREF(data);
}

void pyAuxiliaryDataCopyTo(PyObject *from, PyObject *to) {
  PyObject *retval = PyObject_CallMethod(from, "copyTo", "O", to);
  if(retval == NULL)
    log_pyerr("Error with Python auxiliary copyto");
  Py_XDECREF(retval);
}

PyObject *pyAuxiliaryDataCopy(PyObject *data) {
  PyObject *retval = PyObject_CallMethod(data, "copy", NULL);
  if(retval == NULL)
    log_pyerr("Error with Python auxiliary copy");
  return retval;
}

STORAGE_SET *pyAuxiliaryDataStore(PyObject *data) {
  PyObject  *pyset = PyObject_CallMethod(data, "store", NULL);
  if(pyset == NULL)
    log_pyerr("Error with Python auxiliary store");
  STORAGE_SET *set = NULL;

  // make sure the set exists
  if(pyset == NULL || pyset == Py_None)
    set = new_storage_set();
  else
    set = PyStorageSet_AsSet(pyset);
  Py_XDECREF(pyset);
  return set;
}

PyObject *pyAuxiliaryDataRead(const char *keyword, STORAGE_SET *set) {
  // look up the prototype
  PyObject *proto = hashGet(aux_table, keyword);
  
  // make sure we've got an OK proto
  if(proto == NULL)
    return NULL;
  else {
    // build our arguments and keywords
    PyObject  *pystore = newPyStorageSet(set);
    PyObject     *args = Py_BuildValue("(O)", newPyStorageSet(set));
    PyObject *instance = PyInstance_New(proto, args, NULL);
    if(instance == NULL)
      log_pyerr("Error with Python auxiliary read");
    Py_DECREF(args);
    Py_DECREF(pystore);
    return instance;
  }
}



//*****************************************************************************
//  implementation of the pyauxiliary method 
//*****************************************************************************

//
// install new python auxiliary data. Auxiliary data must be a python CLASS
// OBJECT that has 4 functions:
//   __init__   this function initializes the auxiliary data. It takes an
//              optional argument that is a storage set which should be used
//              to initialize the values of the auxiliary data if it is supplied
//   copy       returns a copy of the auxiliary data
//   copyTo     the values of the auxiliary data to another auxiliary data
//              supplied as an argument to the function
//   store      returns a storage set representation of the auxiliary data
PyObject *PyAuxiliary_install(PyObject *self, PyObject *args) {
  PyObject   *proto = NULL; // the class proto
  char        *name = NULL; // the name of the auxiliary data
  char *installs_on = NULL; // a list of datatypes it installs onto
  bitvector_t  type = 0;    // bitvector representation of we we install on

  // try parsing the install
  if(!PyArg_ParseTuple(args, "sOs", &name, &proto, &installs_on)) {
    PyErr_Format(PyExc_TypeError,
		 "Installed auxiliaries require a name, a proto class, and "
		 "a comma-separated list of datatypes to install onto");
    return NULL;
  }

  // parse the data types that this auxiliary data installs to
  if(is_keyword(installs_on, "character", FALSE) ||
     is_keyword(installs_on, "char",      FALSE))
    SET_BIT(type, AUXILIARY_TYPE_CHAR);
  if(is_keyword(installs_on, "room",      FALSE))
    SET_BIT(type, AUXILIARY_TYPE_ROOM);
  if(is_keyword(installs_on, "object",    FALSE) ||
     is_keyword(installs_on, "obj",       FALSE))
    SET_BIT(type, AUXILIARY_TYPE_OBJ);
  if(is_keyword(installs_on, "account",   FALSE) ||
     is_keyword(installs_on, "acct",      FALSE))
    SET_BIT(type, AUXILIARY_TYPE_ACCOUNT);
  if(is_keyword(installs_on, "socket",    FALSE) ||
     is_keyword(installs_on, "sock",      FALSE))
    SET_BIT(type, AUXILIARY_TYPE_SOCKET);
  // more types to come soon!!
  //***********
  // FINISH ME
  //***********

  // add proto to our table
  Py_INCREF(proto);
  hashPut(aux_table, name, proto);
  AUXILIARY_FUNCS *funcs = newAuxiliaryFuncs(type, 
	newPyAuxiliaryData, deletePyAuxiliaryData,
	pyAuxiliaryDataCopyTo, pyAuxiliaryDataCopy,
	pyAuxiliaryDataStore, pyAuxiliaryDataRead);
  auxiliaryFuncSetIsPy(funcs, TRUE);
					     
  auxiliariesInstall(name, funcs);
  return Py_BuildValue("i", 1);
}

PyMethodDef pyauxiliary_module_methods[] = {
  { "install", PyAuxiliary_install, METH_VARARGS,
    "install(name, AuxClass, installs_on)\n\n"
    "Register new auxiliary data to the given name. Auxiliary data can be\n"
    "installed on: character, object, room, account, socket. Auxiliary data\n"
    "must be a class object of the following form:\n\n"
    "  class ClassName:\n"
    "    def __init__(self, storage_set = None)\n"
    "      ...\n\n"
    "    def copy(self)\n"
    "      ...\n\n"
    "    def copyTo(self, to)\n"
    "      ...\n\n"
    "    def store(self)\n"    
    "      ...\n\n"
    "The Store method returns a storage set representation of the data. If\n"
    "the auxiliary data is not persistent, an empty storage set can be\n"
    "returned. The class's init function must be able to handle reading in\n"
    "data from a storage set, or creating a fresh instance if set = None.\n" },
  {NULL, NULL, 0, NULL}  /* Sentinel */
};


//*****************************************************************************
// implementation of pyauxiliary.h
//*****************************************************************************

// initialize python auxiliary data for use
PyMODINIT_FUNC init_PyAuxiliary(void) {
  aux_table = newHashtableSize(MIN_AUX_TABLE_SIZE);
  Py_InitModule3("auxiliary", pyauxiliary_module_methods,
		 "The module for installing auxiliary data");
}

// returns TRUE if auxiliary data of the given name has been installed by python
bool pyAuxiliaryDataExists(const char *name) {
  return hashIn(aux_table, name);
}
