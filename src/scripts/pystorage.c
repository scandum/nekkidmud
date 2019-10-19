//*****************************************************************************
//
// pystorage.c
//
// Provides a wrapper around NakedMud's storage structure for use by scripts
// and python modules.
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../storage.h"

#include "pystorage.h"



//*****************************************************************************
// local data structures
//*****************************************************************************
typedef struct {
  PyObject_HEAD
  STORAGE_SET *set;
} PyStorageSet;

typedef struct {
  PyObject_HEAD
  STORAGE_SET_LIST *list;
} PyStorageList;



//*****************************************************************************
// local function prototypes
//*****************************************************************************
PyObject *newPyStorageList(STORAGE_SET_LIST *list);
int     PyStorageSet_Check(PyObject *value);
int    PyStorageList_Check(PyObject *value);



//*****************************************************************************
// py storage lists
//*****************************************************************************

//
// deallocate the storage list
void PyStorageList_dealloc(PyStorageList *self) {
  // do NOT free this!! People will need to close the encapsulating set
  //  if(self->list) storage_list_delete(self->list);
  self->ob_type->tp_free((PyObject*)self);
}

//
// create a new python storage list
PyObject *PyStorageList_new(PyTypeObject *type, PyObject *args, PyObject *kwds){
    PyStorageList *self = (PyStorageList *)type->tp_alloc(type, 0);
    self->list = NULL;
    return (PyObject *)self;
}

//
// initialize a new storage list
int PyStorageList_init(PyStorageList *self, PyObject *args, PyObject *kwds) {
  // are we building this from a Python list?
  PyObject *pylist = NULL;
  if(!PyArg_ParseTuple(args, "|O", &pylist)) {
    PyErr_Format(PyExc_TypeError, "unexpected arguments passed to StorageList.");
    return -1;
  }
  
  // if we received something, make sure it's a list
  if(pylist != NULL && !PyList_Check(pylist)) {
    PyErr_Format(PyExc_TypeError, "StorageList expected a list argument.");
    return -1;
  }

  // create the storage list
  self->list = new_storage_list();

  // try adding contents to the storage list
  if(pylist != NULL) {
    int i;
    // first, make sure they're all storage sets
    for(i = 0; i < PyList_Size(pylist); i++) {
      PyObject *set = PyList_GetItem(pylist, i);
      if(!PyStorageSet_Check(set)) {
	PyErr_Format(PyExc_TypeError, "StorageList list content to be a StorageSet.");
	return -1;
      }
    }

    // all good, append them
    for(i = 0; i < PyList_Size(pylist); i++) {
      PyObject *set = PyList_GetItem(pylist, i);
      storage_list_put(self->list, PyStorageSet_AsSet(set));
    }
  }

  return 0;
}

//
// return a python list of the sets in our storage list
PyObject *PyStorageList_sets(PyObject *self, PyObject *args) {
  STORAGE_SET_LIST *set_list = ((PyStorageList *)self)->list;
  PyObject             *list = PyList_New(0);
  STORAGE_SET           *set = NULL;

  // add in all of the elements
  while( (set = storage_list_next(set_list)) != NULL) {
    PyObject *pyset = newPyStorageSet(set);
    PyList_Append(list, pyset);
    Py_DECREF(pyset);
  }

  // return the new list
  return list;
}

//
// add a new set to the storage list
PyObject *PyStorageList_add(PyObject *self, PyObject *args) {
  PyStorageSet *set = NULL;
  if(!PyArg_ParseTuple(args, "O", &set)) {
    PyErr_Format(PyExc_TypeError,
		 "Only storage sets can be added to storage lists");
    return NULL;
  }

  // make sure it is indeed a storage set
  if(!PyStorageSet_Check((PyObject *)set)) {
    PyErr_Format(PyExc_TypeError,
		 "Only storage sets can be added to storage lists");
    return NULL;
  }

  // add it to the list and return
  storage_list_put(((PyStorageList *)self)->list, set->set);
  return Py_BuildValue("i", 1);
}



//*****************************************************************************
// The type object for PyStorageLists and method list
//*****************************************************************************
PyMethodDef PyStorageList_class_methods[] = {
  {"sets", PyStorageList_sets, METH_VARARGS,
   "sets()\n\n"
   "Returns a python list of all the sets in the storage list." },
  {"add",  PyStorageList_add,  METH_VARARGS,
   "add(storage_set)\n\n"
   "Append a new storage set to the storage list." },
  {NULL, NULL, 0, NULL}  /* Sentinel */
};

PyTypeObject PyStorageList_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                /*ob_size*/
    "storage.StorageList",            /*tp_name*/
    sizeof(PyStorageList),            /*tp_basicsize*/
    0,                                /*tp_itemsize*/
    (destructor)PyStorageList_dealloc,/*tp_dealloc*/
    0,                                /*tp_print*/
    0,                                /*tp_getattr*/
    0,                                /*tp_setattr*/
    0,                                /*tp_compare*/
    0,                                /*tp_repr*/
    0,                                /*tp_as_number*/
    0,                                /*tp_as_sequence*/
    0,                                /*tp_as_mapping*/
    0,                                /*tp_hash */
    0,                                /*tp_call*/
    0,                                /*tp_str*/
    0,                                /*tp_getattro*/
    0,                                /*tp_setattro*/
    0,                                /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "__init__(self, list=None)\n\n"
    "Create a new storage list. A Python list of storage sets may be\n"
    "supplied, from which to build the storage list.",
    0,		                      /* tp_traverse */
    0,		                      /* tp_clear */
    0,		                      /* tp_richcompare */
    0,		                      /* tp_weaklistoffset */
    0,		                      /* tp_iter */
    0,		                      /* tp_iternext */
    PyStorageList_class_methods,      /* tp_methods */
    0,                                /* tp_members */
    0,                                /* tp_getset */
    0,                                /* tp_base */
    0,                                /* tp_dict */
    0,                                /* tp_descr_get */
    0,                                /* tp_descr_set */
    0,                                /* tp_dictoffset */
    (initproc)PyStorageList_init,     /* tp_init */
    0,                                /* tp_alloc */
    PyStorageList_new,                /* tp_new */
};




//*****************************************************************************
// py storage sets
//*****************************************************************************

//
// deallocate the storage set
void PyStorageSet_dealloc(PyStorageSet *self) {
  // do NOT close or free this!! People will need to use the close() function
  //  if(self->set) storage_close(self->set);
  self->ob_type->tp_free((PyObject*)self);
}

//
// create a new python storage set
PyObject *PyStorageSet_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyStorageSet *self = (PyStorageSet *)type->tp_alloc(type, 0);
    self->set = NULL;
    return (PyObject *)self;
}

//
// initialize a new storage set
int PyStorageSet_init(PyStorageSet *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[] = {"file", NULL};
  char            *file = NULL;

  // get the universal id
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &file)) {
    PyErr_Format(PyExc_TypeError,
		 "Storage Set initializers may only take filename args.");
    return -1;
  }

  // if we have a filename, load up the storage set there
  if(file != NULL) {
    self->set = storage_read(file);
    // the file doesn't exist... just make a blank storage set
    if(self->set == NULL)
      self->set = new_storage_set();
  }
  // no argument... make a new storage set
  else
    self->set = new_storage_set();

  // no errors
  return 0;
}


//
// parses out the key of some argument list provided to a read function
char *PyStorageSet_readParseKey(PyObject *args) {
  char *key = NULL;
  if(!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "String keys must be provided for storage read methods");
    return NULL;
  }
  return key;
}


//
// read a string from the storage set
PyObject *PyStorageSet_readString(PyObject *self, PyObject *args) { 
  char *key = PyStorageSet_readParseKey(args);
  if(key != NULL) 
    return Py_BuildValue("s", read_string(((PyStorageSet *)self)->set, key));
  else            
    return NULL;
}

//
// read an integer value from the storage set
PyObject *PyStorageSet_readInt   (PyObject *self, PyObject *args) { 
  char *key = PyStorageSet_readParseKey(args);
  if(key != NULL) 
    return Py_BuildValue("i", read_int(((PyStorageSet *)self)->set, key));
  else
    return NULL;
}

//
// read a double value from the storage set
PyObject *PyStorageSet_readDouble(PyObject *self, PyObject *args) {
  char *key = PyStorageSet_readParseKey(args);
  if(key != NULL) 
    return Py_BuildValue("d", read_double(((PyStorageSet *)self)->set, key));
  else
    return NULL;
}

//
// read a boolean value from the storage set
PyObject *PyStorageSet_readBool  (PyObject *self, PyObject *args) { 
  char *key = PyStorageSet_readParseKey(args);
  if(key != NULL) {
    bool val = read_bool(((PyStorageSet *)self)->set, key);
    return Py_BuildValue("O", (val ? Py_True : Py_False));
  }
  else
    return NULL;
}

//
// read a storage list from the storage set
PyObject *PyStorageSet_readList  (PyObject *self, PyObject *args) { 
  char *key = PyStorageSet_readParseKey(args);
  if(key == NULL)
    return NULL;
  else
      return newPyStorageList(read_list(((PyStorageSet*)self)->set, key));
}

//
// read a storage set from within the set
PyObject *PyStorageSet_readSet   (PyObject *self, PyObject *args) { 
  char *key = PyStorageSet_readParseKey(args);
  if(key == NULL)
    return NULL;
  else
    return newPyStorageSet(read_set(((PyStorageSet *)self)->set, key));
}


//
// here's a yucky macro for handling to store end of thigns
#define PYSTORE_PARSE(args, key, val, fmt)			\
  if(!PyArg_ParseTuple(args, fmt, &key, &val)) {		\
    PyErr_Format(PyExc_TypeError,				\
		 "Invalid types supplied to storage method");	\
    return NULL;						\
  }


//
// store a string in the set
PyObject *PyStorageSet_storeString(PyObject *self, PyObject *args) { 
  char *key = NULL;
  char *val = NULL;
  PYSTORE_PARSE(args, key, val, "ss");
  store_string(((PyStorageSet *)self)->set, key, val);
  return Py_BuildValue("i", 1);
}

//
// store an integer in the set
PyObject *PyStorageSet_storeInt   (PyObject *self, PyObject *args) { 
  char *key = NULL;
  int   val = 0;
  PYSTORE_PARSE(args, key, val, "si");
  store_int(((PyStorageSet *)self)->set, key, val);
  return Py_BuildValue("i", 1);
}

//
// store a double in the set
PyObject *PyStorageSet_storeDouble(PyObject *self, PyObject *args) { 
  char  *key = NULL;
  double val = 0;
  PYSTORE_PARSE(args, key, val, "sd");
  store_double(((PyStorageSet *)self)->set, key, val);
  return Py_BuildValue("i", 1);
}

//
// store a boolean in the set
PyObject *PyStorageSet_storeBool  (PyObject *self, PyObject *args) { 
  return PyStorageSet_storeInt(self, args);
}

//
// store a list in the set
PyObject *PyStorageSet_storeList  (PyObject *self, PyObject *args) { 
  PyStorageList *val = NULL;
  char          *key = NULL;
  PYSTORE_PARSE(args, key, val, "sO");
  store_list(((PyStorageSet *)self)->set, key, val->list);
  return Py_BuildValue("i", 1);
}

//
// store a set in the set
PyObject *PyStorageSet_storeSet   (PyObject *self, PyObject *args) { 
  PyStorageSet *val = NULL;
  char         *key = NULL;
  PYSTORE_PARSE(args, key, val, "sO");
  store_set(((PyStorageSet *)self)->set, key, val->set);
  return Py_BuildValue("i", 1);
}

//
// write the set to file
PyObject *PyStorageSet_write      (PyObject *self, PyObject *args) { 
  char *fname = NULL;
  if(!PyArg_ParseTuple(args, "s", &fname)) {
    PyErr_Format(PyExc_TypeError, 
		 "Filenames must be in string form");
    return NULL;
  }
  storage_write(((PyStorageSet *)self)->set, fname);
  return Py_BuildValue("i", 1);
}

//
// close a storage set, and clean up its memory
PyObject *PyStorageSet_close      (PyObject *self, PyObject *args) { 
  storage_close(((PyStorageSet *)self)->set);
  ((PyStorageSet *)self)->set = NULL;
  return Py_BuildValue("i", 1);
}

//
// return whether or not the set contains a given key
PyObject *PyStorageSet_contains   (PyObject *self, PyObject *args) { 
  char *key = NULL;
  if(!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, 
		"You can only check if storage sets contain string key values");
    return NULL;
  }
  return Py_BuildValue("i", storage_contains(((PyStorageSet *)self)->set, key));
}



//*****************************************************************************
// The type object for PyStorageSets and method list
//*****************************************************************************
PyMethodDef PyStorageSet_class_methods[] = {
  // read functions
  { "readString", PyStorageSet_readString, METH_VARARGS,
    "readString(name)\n\n"
    "Read a string value from the storage set. Return empty string if the\n"
    "storage set does not contain an entry by the given name." },
  { "readInt",    PyStorageSet_readInt,    METH_VARARGS,
    "Same as readString, for integers. Returns 0 if entry does not exist." },
  { "readDouble", PyStorageSet_readDouble, METH_VARARGS,
    "Same as readString, for floating points. Returns 0 if entry does not exist.." },
  { "readBool",   PyStorageSet_readBool,   METH_VARARGS,
    "Same as readString, for booleans. Returns False if entry does not exist."},
  { "readList",   PyStorageSet_readList,   METH_VARARGS,
    "Same as readString, for storage lists. Returns an empty storage list\n"
    "if entry does not exist." },
  { "readSet",    PyStorageSet_readSet,   METH_VARARGS,
    "Same as readString, for storage sets. Returns an empty set if entry\n"
    "does not exist." },

  // write store functions
  { "storeString", PyStorageSet_storeString, METH_VARARGS,
    "storeString(name, val)\n\n"
    "Store a string value in the storage set." },
  { "storeInt",    PyStorageSet_storeInt,    METH_VARARGS,
    "Same as storeString, for integers." },
  { "storeDouble", PyStorageSet_storeDouble, METH_VARARGS,
    "Same as storeString, for floating point values." },
  { "storeBool",   PyStorageSet_storeBool,   METH_VARARGS,
    "Same as storeString, for boolean values." },
  { "storeList",   PyStorageSet_storeList,   METH_VARARGS,
    "Same as storeString, for storage lists." },
  { "storeSet",    PyStorageSet_storeSet,    METH_VARARGS,
    "Same as storeString, for storage sets." },

  // other functions
  { "write",       PyStorageSet_write,       METH_VARARGS,
    "write(filename)\n\n"
    "Write the contents of a storage set to the specified file name." },
  { "close",       PyStorageSet_close,       METH_NOARGS,
    "close()\n\n"
    "Recursively close a storage set and all of its children sets and lists.\n"
    "MUST be called when the storage set has finished being used. Garbage\n"
    "collection will not delete the set." },
  { "contains",    PyStorageSet_contains,    METH_VARARGS,
    "contains(name)\n\n"
    "Returns True or False if the set contains an entry by the given name." },
  { "__contains__",    PyStorageSet_contains,    METH_VARARGS,
    "__contains__(name) <==> name in self" },

  {NULL, NULL, 0, NULL}  /* Sentinel */
};


PyTypeObject PyStorageSet_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                /*ob_size*/
    "storage.StorageSet",             /*tp_name*/
    sizeof(PyStorageSet),             /*tp_basicsize*/
    0,                                /*tp_itemsize*/
    (destructor)PyStorageSet_dealloc, /*tp_dealloc*/
    0,                                /*tp_print*/
    0,                                /*tp_getattr*/
    0,                                /*tp_setattr*/
    0,                                /*tp_compare*/
    0,                                /*tp_repr*/
    0,                                /*tp_as_number*/
    0,                                /*tp_as_sequence*/
    0,                                /*tp_as_mapping*/
    0,                                /*tp_hash */
    0,                                /*tp_call*/
    0,                                /*tp_str*/
    0,                                /*tp_getattro*/
    0,                                /*tp_setattro*/
    0,                                /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "__init__(self, filename=None)\n\n"
    "Create a new storage set. If a file name is supplied, read a storage set\n"
    "in from the specified file.",
    0,		                      /* tp_traverse */
    0,		                      /* tp_clear */
    0,		                      /* tp_richcompare */
    0,		                      /* tp_weaklistoffset */
    0,		                      /* tp_iter */
    0,		                      /* tp_iternext */
    PyStorageSet_class_methods,       /* tp_methods */
    0,                                /* tp_members */
    0,                                /* tp_getset */
    0,                                /* tp_base */
    0,                                /* tp_dict */
    0,                                /* tp_descr_get */
    0,                                /* tp_descr_set */
    0,                                /* tp_dictoffset */
    (initproc)PyStorageSet_init,      /* tp_init */
    0,                                /* tp_alloc */
    PyStorageSet_new,                 /* tp_new */
};



//
// all of the methods assocciated with the storage module
PyMethodDef PyStorage_module_methods[] = {
  {NULL, NULL, 0, NULL}  /* Sentinel */
};



//*****************************************************************************
// implementation of pystorage.h
//*****************************************************************************
PyMODINIT_FUNC init_PyStorage(void) {
  PyObject *module = Py_InitModule3("storage", PyStorage_module_methods,
    "The storage module allows users to save information to disk as key:value\n"
    "pairs without having to interact with files directly. This module gives\n"
    "users access to two classes: StorageSets and StorageLists. Sets match\n"
    "keys to values. Lists hold multiple sets.");

  // something went wrong... abort!
  if(module == NULL)
    return;

  // make sure the storage class is ready to be made
  if (!(PyType_Ready(&PyStorageSet_Type) < 0)) {
    // add our two classes
    PyTypeObject *type = &PyStorageSet_Type;
    Py_INCREF(&PyStorageSet_Type);
    PyModule_AddObject(module, "StorageSet", (PyObject *)type);
  }

  // make sure the list calss is ready to be made
  if(!(PyType_Ready(&PyStorageList_Type) < 0)) {
    PyTypeObject *type = &PyStorageList_Type;
    Py_INCREF(&PyStorageList_Type);
    PyModule_AddObject(module, "StorageList", (PyObject *)type);
  }
}


//
// create a new python representation of a storage set
PyObject *newPyStorageSet(STORAGE_SET *set) {
  PyStorageSet *pyset = 
    (PyStorageSet *)PyStorageSet_new(&PyStorageSet_Type, NULL, NULL);
  pyset->set = set;
  return (PyObject *)pyset;
}


//
// create a new list representation of a storage set
PyObject *newPyStorageList(STORAGE_SET_LIST *list) {
  PyStorageList *pylist = 
    (PyStorageList *)PyStorageList_new(&PyStorageList_Type, NULL, NULL);
  pylist->list = list;
  return (PyObject *)pylist;
}


//
// checks to see if the python object is a storage set
int PyStorageSet_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PyStorageSet_Type);
}


//
// checks to see if the python object is a storage list
int PyStorageList_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PyStorageList_Type);
}

//
// return the storage set that is contained within it.
STORAGE_SET *PyStorageSet_AsSet(PyObject *set) {
  return ((PyStorageSet *)set)->set;
}
