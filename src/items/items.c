//*****************************************************************************
//
// items.c
//
// Objects can serve many different functions; they might be containers, they
// might be weapons, they might be pieces of clothing... the list goes on and
// on. The classification of what type of item the object is is what handles
// these sorts of distinctions. This is a general framework for adding new
// item types. Contains functions for storing type data on objects. Item types
// are expected to be able to handle their own subtyping (if neccessary). 
// Objects can be multi-typed (e.g. a desk with a drawer can be both a piece
// of furniture and a container).
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../object.h"
#include "../auxiliary.h"
#include "../storage.h"

#include "items.h"
#include "iedit.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pymudsys.h"
#include "../scripts/pystorage.h"
#include "../scripts/pyobj.h"
#include "../scripts/pyplugs.h"



//*****************************************************************************
// local functions, datastructures, and defines
//*****************************************************************************
// a table of all our item types, and their assocciated new/delete/etc.. funcs
HASHTABLE *type_table = NULL;

#define ITYPE_C   0
#define ITYPE_PY  1

//
// this is what will go into our type_data table
typedef struct item_func_data {
  // what type of item function data do we have?
  int              type;

  // C functions
  void          *(* new)(void);
  void        (* delete)(void *data);
  void         *(* copy)(void *data);
  void        (* copyto)(void *from, void *to);
  STORAGE_SET *(* store)(void *data);
  void         *(* read)(STORAGE_SET *set);

  // Python functions
  PyObject *pyfuncs;

} ITEM_FUNC_DATA;

void *ifunc_new(ITEM_FUNC_DATA *funcs) {
  if(funcs->type == ITYPE_PY) {
    PyObject *ret = PyInstance_New(funcs->pyfuncs, NULL, NULL);
    if(ret == NULL)
      log_pyerr("Error creating new Python item type");
    return ret;
  }
  return funcs->new();
}

void ifunc_delete(ITEM_FUNC_DATA *funcs, void *data) {
  if(funcs->type == ITYPE_PY) {
    Py_XDECREF((PyObject *)data);
  }
  else 
    funcs->delete(data);
}

void *ifunc_copy(ITEM_FUNC_DATA *funcs, void *data) {
  if(funcs->type == ITYPE_PY) {
    PyObject *ret = PyObject_CallMethod(data, "copy", NULL);
    if(ret == NULL)
      log_pyerr("Error copying Python item type");
    return ret;
  }
  return funcs->copy(data);
}

void ifunc_copyto(ITEM_FUNC_DATA *funcs, void *from, void *to) {
  if(funcs->type == ITYPE_PY) {
    PyObject *ret = PyObject_CallMethod(from, "copyTo", "O", to);
    if(ret == NULL)
      log_pyerr("Error calling copyTo Python item type method");
    Py_XDECREF(ret);
  }
  else
    funcs->copyto(from, to);
}

STORAGE_SET *ifunc_store(ITEM_FUNC_DATA *funcs, void *data) {
  if(funcs->type == ITYPE_PY) {
    PyObject  *pyset = PyObject_CallMethod(data, "store", NULL);
    STORAGE_SET *set = NULL;
    if(pyset != NULL)
      set = PyStorageSet_AsSet(pyset);
    else {
      set = new_storage_set();
      log_pyerr("Error storing Python item type");
    }

    Py_XDECREF(pyset);
    return set;
  }
  return funcs->store(data);
}

void *ifunc_read(ITEM_FUNC_DATA *funcs, STORAGE_SET *set) {
  if(funcs->type == ITYPE_PY) {
    PyObject *pyset = newPyStorageSet(set);
    PyObject  *args = Py_BuildValue("(O)", pyset);
    PyObject   *ret = PyInstance_New(funcs->pyfuncs, args, NULL);
    if(ret == NULL)
      log_pyerr("Error reading Python item type");
    Py_XDECREF(args);
    Py_XDECREF(pyset);
    return ret;
  }
  return funcs->read(set);
}

//
// create a new structure for holding item type functions
ITEM_FUNC_DATA *newItemFuncData(void *new, void *delete, void *copyTo,
				void *copy, void *store, void *read) {
  ITEM_FUNC_DATA *data = calloc(1, sizeof(ITEM_FUNC_DATA));
  data->new    = new;
  data->delete = delete;
  data->copy   = copy;
  data->copyto = copyTo;
  data->store  = store;
  data->read   = read;
  data->type   = ITYPE_C;
  return data;
}

//
// create a new structure for holding Python item type functions
ITEM_FUNC_DATA *newPyItemFuncData(PyObject *pyfuncs) {
  ITEM_FUNC_DATA *data = calloc(1, sizeof(ITEM_FUNC_DATA));
  data->pyfuncs = pyfuncs;
  data->type    = ITYPE_PY;
  Py_XINCREF(pyfuncs);
  return data;
}

//
// deletes one structure for holding item type functions
void deleteItemFuncData(ITEM_FUNC_DATA *data) {
  Py_XDECREF(data->pyfuncs);
  free(data);
}

//
// goes through an object's item table and deletes everything in it. Does
// not delete the table itself
void clearItemTable(HASHTABLE *table) {
  // if our hashtable isn't empty, we have to make sure 
  // we delete all of the data within it
  if(hashSize(table) > 0) {
    HASH_ITERATOR *hash_i = newHashIterator(table);
    const char       *key = NULL;
    void             *val = NULL;
    // delete the contents of our hashtable
    ITERATE_HASH(key, val, hash_i) {
      ITEM_FUNC_DATA *funcs = hashGet(type_table, key);
      ifunc_delete(funcs, hashRemove(table, key));
    }
    deleteHashIterator(hash_i);
  }
}

//
// copies the contents of one item table over to another. clears the destination
// table first.
void copyItemTableTo(HASHTABLE *from, HASHTABLE *to) {
  clearItemTable(to);
  // if the table we're copying has contents, go throug hand copy them all
  if(hashSize(from) > 0) {
    HASH_ITERATOR *hash_i = newHashIterator(from);
    const char       *key = NULL;
    void             *val = NULL;
    // copy all the contents
    ITERATE_HASH(key, val, hash_i) {
      ITEM_FUNC_DATA *funcs = hashGet(type_table, key);
      hashPut(to, key, ifunc_copy(funcs, val));
    }
    deleteHashIterator(hash_i);
  }
}

//
// Creates a new item table and copies the contents of the old one 
// over to the new one 
HASHTABLE *copyItemTable(HASHTABLE *table) {
  HASHTABLE *newtable = newHashtable();
  copyItemTableTo(table, newtable);
  return newtable;
}



//*****************************************************************************
// auxiliary data
//*****************************************************************************
typedef struct item_data {
  HASHTABLE *item_table;
} ITEM_DATA;

ITEM_DATA *newItemData() {
  ITEM_DATA *data  = malloc(sizeof(ITEM_DATA));
  data->item_table = newHashtable();
  return data;
}

void deleteItemData(ITEM_DATA *data) {
  clearItemTable(data->item_table);
  deleteHashtable(data->item_table);
  free(data);
}

void itemDataCopyTo(ITEM_DATA *from, ITEM_DATA *to) {
  copyItemTableTo(from->item_table, to->item_table);
}

ITEM_DATA *itemDataCopy(ITEM_DATA *data) {
  ITEM_DATA *newdata = newItemData();
  itemDataCopyTo(data, newdata);
  return newdata;
}

STORAGE_SET *itemDataStore(ITEM_DATA *data) {
  STORAGE_SET       *set = new_storage_set();
  // if we have a type or more, go through them all and store 'em
  if(hashSize(data->item_table) > 0) {
    STORAGE_SET_LIST *list = new_storage_list();
    HASH_ITERATOR  *hash_i = newHashIterator(data->item_table);
    ITEM_FUNC_DATA  *funcs = NULL;
    const char        *key = NULL;
    void              *val = NULL;

    // store all of our item type data
    ITERATE_HASH(key, val, hash_i) {
      STORAGE_SET *one_set = new_storage_set();
      funcs = hashGet(type_table, key);
      store_string(one_set, "type", key);
      store_set(one_set, "data", ifunc_store(funcs, val));
      storage_list_put(list, one_set);
    }
    deleteHashIterator(hash_i);

    store_list(set, "types", list);
  }
  return set;
}


ITEM_DATA *itemDataRead(STORAGE_SET *set) {
  ITEM_DATA         *data = newItemData();
  STORAGE_SET_LIST *types = read_list(set, "types");
  ITEM_FUNC_DATA   *funcs = NULL;
  STORAGE_SET  *one_entry = NULL;

  // go through each entry and parse the item type
  while( (one_entry = storage_list_next(types)) != NULL) {
    const char      *type = read_string(one_entry, "type");
    // make sure the type is valid
    if((funcs = hashGet(type_table, type)) == NULL) 
      continue;
    STORAGE_SET *type_set = read_set(one_entry, "data");
    hashPut(data->item_table, type, ifunc_read(funcs, type_set));
  }
  return data;
}



//*****************************************************************************
// Python implementation
//*****************************************************************************
void item_add_pytype(const char *type, PyObject *pyfuncs) {
  ITEM_FUNC_DATA *funcs = NULL;
  // first, make sure no item type by this name already
  // exists. If one does, delete it so it can be replaced
  if((funcs = hashRemove(type_table, type)) != NULL)
    deleteItemFuncData(funcs);
  funcs = newPyItemFuncData(pyfuncs);
  hashPut(type_table, type, funcs);
}

PyObject *PyMudSys_ItemAddType(PyObject *self, PyObject *args) {
  char        *type = NULL;
  PyObject *pyclass = NULL;

  // parse out our socket and functions
  if(!PyArg_ParseTuple(args, "sO",&type, &pyclass)) {
    PyErr_Format(PyExc_TypeError,"Invalid arguments supplied to " 
		 "item_add_type");
    return NULL;
  }

  // make sure it has a string that says what item type it is
  if(!PyObject_HasAttrString(pyclass, "__item_type__") ||
     !PyString_Check(PyObject_GetAttrString(pyclass, "__item_type__"))) {
    PyErr_Format(PyExc_TypeError,"Python item types must have an attribute, "
		 "__item_type__, that describes what type of item data it is.");
    return NULL;
  }

  // check to make sure it has all of the relevant methods
  //***********
  // FINISH ME
  //***********

  // create the new type
  item_add_pytype(type, pyclass);
  return Py_BuildValue("i", 1);
}

PyObject *PyObj_GetTypeData(PyObject *self, PyObject *args) {
  OBJ_DATA         *obj = PyObj_AsObj(self);
  ITEM_FUNC_DATA *fdata = NULL;
  char            *type = NULL;

  if(!PyArg_ParseTuple(args, "s", &type)) {
    PyErr_Format(PyExc_TypeError, "get_type_data must be supplied a string.");
    return NULL;
  }

  // make sure the object exists
  if(obj == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to get type data for non-existent object.");
    return NULL;
  }

  // make sure it's Python data
  fdata = hashGet(type_table, type);
  if(fdata == NULL || fdata->type == ITYPE_C)
    return Py_BuildValue("");
  
  // make sure this item actually has the relevant type data
  PyObject *tdata = objGetTypeData(obj, type);
  return Py_BuildValue("O", (tdata == NULL ? Py_None : tdata));
}

PyObject *PyObj_istype(PyObject *self, PyObject *args) {  
  char *type = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &type)) {
    PyErr_Format(PyExc_TypeError, "istype only accepts strings.");
    return NULL;
  }

  // pull out the object and check the type
  OBJ_DATA    *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL)
    return Py_BuildValue("i", objIsType(obj, type));
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to check type of nonexistent object, %d.", 
		 PyObj_AsUid(self));
    return NULL;
  }
}

PyObject *PyObj_get_types(PyObject *self) {  
  // pull out the object and check the type
  OBJ_DATA    *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL)
    return Py_BuildValue("s", objGetTypes(obj));
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to list types of nonexistent object, %d.", 
		 PyObj_AsUid(self));
    return NULL;
  }
}

PyObject *PyObj_settype(PyObject *self, PyObject *args) {  
  char *type = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &type)) {
    PyErr_Format(PyExc_TypeError, "settype only accepts strings.");
    return NULL;
  }

  // pull out the object and check the type
  OBJ_DATA    *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) {
    objSetType(obj, type);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to set type of nonexistent object, %d.", 
		 PyObj_AsUid(self));
    return NULL;
  }
}



//*****************************************************************************
// implementation of items.h
//*****************************************************************************
void init_items(void) {
  type_table = newHashtable();
  auxiliariesInstall("type_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_OBJ, newItemData, 
				       deleteItemData, itemDataCopyTo,
				       itemDataCopy, itemDataStore, 
				       itemDataRead));

  // initialize Python mudsys methods
  PyMudSys_addMethod("item_add_type", PyMudSys_ItemAddType, METH_VARARGS,
		     "item_add_type(name, type_data)\n"
		     "\n"
		     "Register a new item type and its data.");

  // initialize Python pyobj methods
  PyObj_addMethod("get_type_data", PyObj_GetTypeData, METH_VARARGS,
    "get_type_data(item_type)\n"
    "\n"
    "Returns Python item type data if it exists, or None.");
  PyObj_addMethod("istype", PyObj_istype, METH_VARARGS,
    "istype(item_type)\n"
    "\n"
    "Returns True or False if the object is of the specified item type.");
  PyObj_addMethod("settype", PyObj_settype, METH_VARARGS,
    "settype(item_type)\n"
    "\n"
    "Make an object be the specified item type.");
  PyObj_addMethod("get_types", PyObj_get_types, METH_NOARGS,
    "get_types()\n"
    "\n"
    "Returns a comma-separated list of item types this object has.");
  // do we need a deltype as well?
  //***********
  // FINISH ME
  //***********

  // initialize item olc
  init_item_olc();

  // now, initialize our basic items types
  extern void init_container(); init_container();
  extern void init_portal();    init_portal();
  extern void init_furniture(); init_furniture();
  extern void init_worn();      init_worn();
}

void item_add_type(const char *type, 
		   void *new,    void *delete,
		   void *copyTo, void *copy, 
		   void *store,  void *read) {
  ITEM_FUNC_DATA *funcs = NULL;
  // first, make sure no item type by this name already
  // exists. If one does, delete it so it can be replaced
  if((funcs = hashRemove(type_table, type)) != NULL)
    deleteItemFuncData(funcs);
  funcs = newItemFuncData(new, delete, copyTo, copy, store, read);
  hashPut(type_table, type, funcs);
}

LIST *itemTypeList(void) {
  LIST *list = newList();
  HASH_ITERATOR *hash_i = newHashIterator(type_table);
  const char       *key = NULL;
  void             *val = NULL;
  ITERATE_HASH(key, val, hash_i)
    listPutWith(list, strdup(key), strcasecmp);
  deleteHashIterator(hash_i);
  return list;
}

void *objGetTypeData(OBJ_DATA *obj, const char *type) {
  ITEM_DATA *data = objGetAuxiliaryData(obj, "type_data");
  return hashGet(data->item_table, type);
}

void objSetType(OBJ_DATA *obj, const char *type) {
  ITEM_FUNC_DATA *funcs = hashGet(type_table, type);
  ITEM_DATA       *data = objGetAuxiliaryData(obj, "type_data");
  void   *old_item_data = hashGet(data->item_table, type);
  // if the type exists and we're not already of the type, set it on us
  if(funcs != NULL && old_item_data == NULL) 
    hashPut(data->item_table, type, ifunc_new(funcs));
}

void objDeleteType(OBJ_DATA *obj, const char *type) {
  ITEM_FUNC_DATA *funcs = hashGet(type_table, type);
  ITEM_DATA *data       = objGetAuxiliaryData(obj, "type_data");
  void *old_item_data   = hashRemove(data->item_table, type);
  if(old_item_data) ifunc_delete(funcs, old_item_data);
}

bool objIsType(OBJ_DATA *obj, const char *type) {
  ITEM_DATA *data = objGetAuxiliaryData(obj, "type_data");
  return hashIn(data->item_table, type);
}

const char *objGetTypes(OBJ_DATA *obj) {
  static char buf[MAX_BUFFER];
  ITEM_DATA *data = objGetAuxiliaryData(obj, "type_data");
  if(hashSize(data->item_table) == 0)
    sprintf(buf, "none");
  else {
    *buf = '\0';
    HASH_ITERATOR *hash_i = newHashIterator(data->item_table);
    const char       *key = NULL;
    void             *val = NULL;
    int             count = 0;

    ITERATE_HASH(key, val, hash_i) {
      sprintf(buf, "%s%s%s", buf, (count > 0 ? ", " : ""), key);
      count++;
    }
    deleteHashIterator(hash_i);
  }
  return buf;
}
