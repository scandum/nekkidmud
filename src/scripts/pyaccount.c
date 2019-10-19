//*****************************************************************************
//
// pyaccount.c
//
// Contains a python account module, and an Account class that is a python 
// wrapper for NakedMud accounts.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../account.h"
#include "../character.h"
#include "../save.h"

#include "scripts.h"
#include "pyplugs.h"
#include "pyauxiliary.h"
#include "pyaccount.h"
#include "pychar.h"



//*****************************************************************************
// local structures and defines
//*****************************************************************************

// a list of the get/setters on the Exit class
LIST *pyaccount_getsetters = NULL;

// a list of the methods on the Exit class
LIST *pyaccount_methods = NULL;

typedef struct {
  PyObject_HEAD
  ACCOUNT_DATA *account;
} PyAccount;



//*****************************************************************************
// allocation, deallocation, initialization, and comparison
//*****************************************************************************
void PyAccount_dealloc(PyAccount *self) {
  if(account_exists(accountGetName(self->account)))
    unreference_account(self->account);
  self->ob_type->tp_free((PyObject*)self);
}

PyObject *PyAccount_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyAccount *self;
    self = (PyAccount *)type->tp_alloc(type, 0);
    self->account = NULL;
    return (PyObject *)self;
}

int PyAccount_init(PyAccount *self, PyObject *args, PyObject *kwds) {
  char *kwlist[] = {"name", NULL};
  char     *name = NULL;

  // get the uid
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &name)) {
    PyErr_Format(PyExc_TypeError, "Accounts may only be created using a name");
    return -1;
  }

  // make sure an account with this name exists
  if(!account_exists(name)) {
    PyErr_Format(PyExc_TypeError, "Account with name %s does not exist.", name);
    return -1;
  }

  self->account = get_account(name);
  return 0;
}

int PyAccount_compare(PyAccount *acc1, PyAccount *acc2) {
  return strcasecmp(accountGetName(acc1->account),
		    accountGetName(acc2->account));
}



//*****************************************************************************
// getters and setters for the Account class
//*****************************************************************************
PyObject *PyAccount_getname(PyAccount *self, void *closure) {
  ACCOUNT_DATA *acc = PyAccount_AsAccount((PyObject *)self);
  if(acc != NULL) return Py_BuildValue("s", accountGetName(acc));
  else            return NULL;
}



//*****************************************************************************
// methods for the Account class
//*****************************************************************************
PyObject *PyAccount_add_char(PyAccount *self, PyObject *args) {
  PyObject    *val = NULL;
  CHAR_DATA    *ch = NULL;
  const char *name = NULL;

  if(!PyArg_ParseTuple(args, "O", &val)) {
    PyErr_Format(PyExc_TypeError,
		 "add_char() must be supplied with a character or name.");
    return NULL;
  }

  // make sure we exist
  ACCOUNT_DATA *acc = PyAccount_AsAccount((PyObject *)self);
  if(acc == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to add character to a nonexistant account.");
    return NULL;
  }

  if(PyString_Check(val))
    name = PyString_AsString(val);
  else if(PyChar_Check(val)) {
    ch = PyChar_AsChar(val);
    if(ch != NULL)
      name = charGetName(ch);
    else {
      PyErr_Format(PyExc_StandardError,
		   "Tried to add nonexistant character %d to account.",
		   PyChar_AsUid(val));
      return NULL;
    }
  }

  accountPutChar(acc, name);
  listSortWith(accountGetChars(acc), strcasecmp);
  return Py_BuildValue("i", 1);
}

PyObject *PyAccount_characters(PyAccount *self, PyObject *args) {
  ACCOUNT_DATA *acc = PyAccount_AsAccount((PyObject *)self);
  if(acc == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to add character to a nonexistant account.");
    return NULL;
  }

  LIST_ITERATOR *name_i = newListIterator(accountGetChars(acc));
  PyObject        *list = PyList_New(0);
  const char      *name = NULL;

  ITERATE_LIST(name, name_i) {
    PyObject *val = PyString_FromString(name);
    PyList_Append(list, val);
    Py_DECREF(val);
  } deleteListIterator(name_i);
  PyObject *retval = Py_BuildValue("O", list);
  Py_DECREF(list);
  return retval;
}

//
// returns the specified piece of auxiliary data from the account
// if it is a piece of python auxiliary data.
PyObject *PyAccount_get_auxiliary(PyAccount *self, PyObject *args) {
  char *keyword = NULL;
  if(!PyArg_ParseTuple(args, "s", &keyword)) {
    PyErr_Format(PyExc_TypeError,
		 "getAuxiliary() must be supplied with the name that the "
		 "auxiliary data was installed under!");
    return NULL;
  }

  // make sure we exist
  ACCOUNT_DATA *acc = PyAccount_AsAccount((PyObject *)self);
  if(acc == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to get auxiliary data for a nonexistant account.");
    return NULL;
  }

  // make sure the auxiliary data exists
  if(!pyAuxiliaryDataExists(keyword)) {
    PyErr_Format(PyExc_StandardError,
		 "No auxiliary data named '%s' exists!", keyword);
    return NULL;
  }

  PyObject *data = accountGetAuxiliaryData(acc, keyword);
  if(data == NULL)
    data = Py_None;
  PyObject *retval = Py_BuildValue("O", data);
  //  Py_DECREF(data);
  return retval;
}



//*****************************************************************************
// structures to define our methods and classes
//*****************************************************************************
PyTypeObject PyAccount_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "account.Account",         /*tp_name*/
    sizeof(PyAccount),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyAccount_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    (cmpfunc)PyAccount_compare,/*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Python Account object",   /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */ 
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PyAccount_init,  /* tp_init */
    0,                         /* tp_alloc */
    PyAccount_new,             /* tp_new */
};

PyMethodDef account_module_methods[] = {
  {NULL, NULL, 0, NULL}  /* Sentinel */
};



//*****************************************************************************
// implementation of pyexit.h
//*****************************************************************************
void PyAccount_addGetSetter(const char *name, void *g, void *s,const char *doc){
  // make sure our list of get/setters is created
  if(pyaccount_getsetters == NULL) pyaccount_getsetters = newList();

  // make the GetSetter def
  PyGetSetDef *def = calloc(1, sizeof(PyGetSetDef));
  def->name        = strdup(name);
  def->get         = (getter)g;
  def->set         = (setter)s;
  def->doc         = (doc ? strdup(doc) : NULL);
  def->closure     = NULL;
  listPut(pyaccount_getsetters, def);
}

void PyAccount_addMethod(const char *name, void *f, int flags, const char *doc){
  // make sure our list of methods is created
  if(pyaccount_methods == NULL) pyaccount_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pyaccount_methods, def);
}

// initialize accounts for use. This must be called AFTER 
PyMODINIT_FUNC
init_PyAccount(void) {
    PyObject* module = NULL;

    // add all of the basic getsetters
    PyAccount_addGetSetter("name", PyAccount_getname, NULL, 
			   "The account's name. Immutable.");

    // add all of the basic methods
    PyAccount_addMethod("add_char", PyAccount_add_char, METH_VARARGS,
      "add_char(name_or_char)\n\n"
      "Adds a new character to an account's list of registered characters.");
    PyAccount_addMethod("characters", PyAccount_characters, METH_VARARGS,
      "characters()\n\n"
      "Returns a list of names for characters registered to the account.");
    PyAccount_addMethod("getAuxiliary", PyAccount_get_auxiliary, METH_VARARGS,
      "getAuxiliary(name)\n"
      "\n"
      "Returns account's auxiliary data of the specified name.");
    PyAccount_addMethod("aux", PyAccount_get_auxiliary, METH_VARARGS,
      "Alias for account.Account.getAuxiliary.");

    // add in all the getsetters and methods
    makePyType(&PyAccount_Type, pyaccount_getsetters, pyaccount_methods);
    deleteListWith(pyaccount_getsetters, free); pyaccount_getsetters = NULL;
    deleteListWith(pyaccount_methods,    free); pyaccount_methods    = NULL;

    // make sure the account class is ready to be made
    if (PyType_Ready(&PyAccount_Type) < 0)
        return;

    // initialize the module
    module = Py_InitModule3("account", account_module_methods,
			    "Contains the Python wrapper for accounts.");

    // make sure the module parsed OK
    if (module == NULL)
      return;

    // add the Account class to the account module
    PyTypeObject *type = &PyAccount_Type;
    PyModule_AddObject(module, "Account", (PyObject *)type);
    Py_INCREF(&PyAccount_Type);
}

ACCOUNT_DATA *PyAccount_AsAccount(PyObject *account) {
  return ((PyAccount *)account)->account;
}

int PyAccount_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PyAccount_Type);
}

PyObject *newPyAccount(ACCOUNT_DATA *account) {
  PyAccount *py_account = 
    (PyAccount *)PyAccount_new(&PyAccount_Type, NULL, NULL);
  py_account->account = account;
  if(account_exists(accountGetName(account)))
    reference_account(account);
  return (PyObject *)py_account;
}
