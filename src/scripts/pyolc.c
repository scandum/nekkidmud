//*****************************************************************************
//
// pyolc.c
//
// Provides wrapper functions for Python to interface with OLC2.
//
//*****************************************************************************
#include "../mud.h"

#ifdef MODULE_OLC2
#include "scripts.h"
#include "pyplugs.h"
#include "pysocket.h"
#include "pyolc.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../olc2/olc.h"
#include "../olc2/olc_extender.h"
#include "../items/iedit.h"



//*****************************************************************************
// local variables and functions
//*****************************************************************************
// a list of methods to add to the module
LIST  *pyolc_methods = NULL;

// a dictionary for the OLC functions of our various item types
PyObject *pyitem_olc = NULL;



//*****************************************************************************
// raw pyolc methods, used by both items and the olc wrapper functions
//*****************************************************************************
void pyolc_do_menu(PyObject *menu, SOCKET_DATA *sock, PyObject *data) {
  PyObject    *pysock = socketGetPyFormBorrowed(sock);
  PyObject       *ret = PyObject_CallFunction(menu, "OO", pysock, data);
  if(ret == NULL)
    log_pyerr("pyolc_do_menu failed");
  Py_XDECREF(ret);
}

int pyolc_do_chooser(PyObject *chooser, SOCKET_DATA *sock, PyObject *data,
		     const char *option) {
  PyObject    *pysock = socketGetPyFormBorrowed(sock);
  PyObject     *pyret = PyObject_CallFunction(chooser, "OOs", pysock, data,
					      option);
  int             ret = MENU_CHOICE_INVALID;
  if(pyret == NULL)
    log_pyerr("pyolc_do_chooser failed: %s", option);
  else if(PyInt_Check(pyret))
    ret = (int)PyInt_AsLong(pyret);
  Py_XDECREF(pyret);
  return ret;
}

bool pyolc_do_parser(PyObject *parser, SOCKET_DATA *sock, PyObject *data,
		     int choice, const char *arg) {
  PyObject    *pysock = socketGetPyFormBorrowed(sock);
  PyObject     *pyret = PyObject_CallFunction(parser, "OOis", pysock, data,
					      choice, arg);
  bool            ret = FALSE;
  if(pyret == NULL)
    log_pyerr("pyolc_do_parser failed: %d, %s", choice, arg);
  else if(PyObject_IsTrue(pyret))
    ret = TRUE;
  Py_XDECREF(pyret);
  return ret;
}

void pyolc_do_from_proto(PyObject *from_proto, PyObject *data) {
  if(from_proto != NULL) {
    PyObject *ret = PyObject_CallFunction(from_proto, "O", data);
    if(ret == NULL)
      log_pyerr("Error in pyolc_do_from_proto");
    Py_XDECREF(ret);
  }
}

void pyolc_do_to_proto(PyObject *to_proto, PyObject *data, BUFFER *buf) {
  if(to_proto != NULL) {
      PyObject *ret = PyObject_CallFunction(to_proto, "O", data);
      if(ret == NULL)
	log_pyerr("Error in pyolc_do_to_proto");
      else if(PyString_Check(ret))
	bufferCat(buf, PyString_AsString(ret));
      Py_XDECREF(ret);
  }
}

void pyolc_do_copyto(PyObject *from, PyObject *to) {
  PyObject *ret = PyObject_CallMethod(from, "copyTo", "O", to);
  if(ret == NULL)
    log_pyerr("Error in pyolc_do_copyto");
  Py_XDECREF(ret);
}

void pyolc_do_save(PyObject *saver, PyObject *data) {
  if(saver != NULL && saver != Py_None) {
    PyObject *ret = PyObject_CallFunction(saver, "O", data);
    if(ret == NULL)
      log_pyerr("Error in pyolc_do_save");
    Py_XDECREF(ret);    
  }
}

void pyolc_do_delete(PyObject *pyolc) {
  Py_XDECREF(pyolc);
}



//*****************************************************************************
// Python OLC wrapper
//*****************************************************************************
void pyolc_menu(SOCKET_DATA *sock, PyObject *olc) {
  PyObject *working_copy = PyDict_GetItemString(olc, "working_copy");
  PyObject         *menu = PyDict_GetItemString(olc, "menu");
  pyolc_do_menu(menu, sock, working_copy);
}

int pyolc_chooser(SOCKET_DATA *sock, PyObject *olc, const char *option) {
  PyObject *working_copy = PyDict_GetItemString(olc, "working_copy");
  PyObject      *chooser = PyDict_GetItemString(olc, "chooser");
  return pyolc_do_chooser(chooser, sock, working_copy, option);
}

bool pyolc_parser(SOCKET_DATA *sock,PyObject *olc,int option,const char *arg) {
  PyObject *working_copy = PyDict_GetItemString(olc, "working_copy");
  PyObject       *parser = PyDict_GetItemString(olc, "parser");
  return pyolc_do_parser(parser, sock, working_copy, option, arg);
}

void pyolc_saver(PyObject *olc) {
  PyObject *working_copy = PyDict_GetItemString(olc, "working_copy");
  PyObject    *orig_copy = PyDict_GetItemString(olc, "orig_copy");
  PyObject        *saver = PyDict_GetItemString(olc, "saver");

  // do we need to copy from our working copy to our original?
  if(orig_copy != working_copy)
    pyolc_do_copyto(working_copy, orig_copy);

  // do we need to save this info to disk?
  if(saver != Py_None)
    pyolc_do_save(saver, orig_copy);
}

void pyolc_deleter(PyObject *olc) {
  Py_DECREF(olc);
}

PyObject *pyolc_do_olc(PyObject *self, PyObject *args) {
  PyObject      *pymenu = NULL;
  PyObject   *pychooser = NULL;
  PyObject    *pyparser = NULL;
  PyObject     *pysaver = NULL;
  PyObject      *pydata = NULL;
  PyObject      *pysock = NULL;
  SOCKET_DATA     *sock = NULL;
  bool         autosave = FALSE;

  // parse out our socket and functions
  if(!PyArg_ParseTuple(args, "OOOOOO|b", 
		       &pysock, &pymenu, &pychooser, &pyparser,
		       &pysaver, &pydata, &autosave)) {
    PyErr_Format(PyExc_TypeError,"Invalid number arguments supplied to do_olc");
    return NULL;
  }

  // make sure our socket is in fact a socket
  if(PySocket_Check(pysock))
    sock = PySocket_AsSocket(pysock);
  else {
    PyErr_Format(PyExc_TypeError, "First argument must be a socket.");
    return NULL;
  }

  // figure out what our C functions will be
  void    *menu = pyolc_menu;
  void *chooser = pyolc_chooser;
  void  *parser = pyolc_parser;
  void   *saver = pyolc_saver;
  void *deleter = pyolc_deleter;
  bool can_copy = (PyObject_HasAttrString(pydata, "copy") && 
		   PyObject_HasAttrString(pydata, "copyTo"));

  // figure out our remaining C OLC functions, if any?
  if(pyparser == Py_None)
    parser = NULL;

  // we do save if we have a save function, or we have a copy and a copyTo func
  if(autosave == TRUE || (pysaver == Py_None && !can_copy))
    saver = NULL;

  // build up our OLC Wrapper
  PyObject *wrapper = PyDict_New();
  PyDict_SetItemString(wrapper, "menu",      pymenu);
  PyDict_SetItemString(wrapper, "chooser",   pychooser);
  PyDict_SetItemString(wrapper, "parser",    pyparser);
  PyDict_SetItemString(wrapper, "saver",     pysaver);

  // set our original copy
  PyDict_SetItemString(wrapper, "orig_copy", pydata);

  // do we want to work on the original copy directly, or no?
  if(autosave == TRUE || !can_copy)
    PyDict_SetItemString(wrapper, "working_copy", pydata);
  else {
    PyObject *working_copy = PyObject_CallMethod(pydata, "copy", NULL);
    if(working_copy != NULL)
      PyDict_SetItemString(wrapper, "working_copy", working_copy);
    // work on the original copy instead
    else {
      log_pyerr("Error calling copy in pyolc_do_olc");
      PyDict_SetItemString(wrapper, "working_copy", pydata);
    }
    Py_XDECREF(working_copy);
  }

  // add the OLC functions
  do_olc(sock, menu, chooser, parser, NULL, NULL, deleter, saver, wrapper);
  return Py_BuildValue("O", Py_None);
}



//*****************************************************************************
// Item OLC
//*****************************************************************************
void pyitem_olc_menu(SOCKET_DATA *sock, PyObject *data) {
  char *str = PyString_AsString(PyObject_GetAttrString(data, "__item_type__"));
  PyObject *olc_funcs = PyDict_GetItemString(pyitem_olc, str);
  PyObject      *menu = PyDict_GetItemString(olc_funcs, "menu");
  pyolc_do_menu(menu, sock, data);
}

int pyitem_olc_chooser(SOCKET_DATA *sock, PyObject *data, const char *option) {
  char *str = PyString_AsString(PyObject_GetAttrString(data, "__item_type__"));
  PyObject *olc_funcs = PyDict_GetItemString(pyitem_olc, str);
  PyObject   *chooser = PyDict_GetItemString(olc_funcs, "chooser");
  return pyolc_do_chooser(chooser, sock, data, option);
}

bool pyitem_olc_parser(SOCKET_DATA *sock, PyObject *data,
		       int choice, const char *arg) {
  char *str = PyString_AsString(PyObject_GetAttrString(data, "__item_type__"));
  PyObject *olc_funcs = PyDict_GetItemString(pyitem_olc, str);
  PyObject    *parser = PyDict_GetItemString(olc_funcs, "parser");
  return pyolc_do_parser(parser, sock, data, choice, arg);
}

void pyitem_olc_from_proto(PyObject *data) {
  char *str = PyString_AsString(PyObject_GetAttrString(data, "__item_type__"));
  PyObject  *olc_funcs = PyDict_GetItemString(pyitem_olc, str);
  PyObject *from_proto = PyDict_GetItemString(olc_funcs, "from_proto");
  pyolc_do_from_proto(from_proto, data);
}

void pyitem_olc_to_proto(PyObject *data, BUFFER *buf) {
  char *str = PyString_AsString(PyObject_GetAttrString(data, "__item_type__"));
  PyObject  *olc_funcs = PyDict_GetItemString(pyitem_olc, str);
  PyObject   *to_proto = PyDict_GetItemString(olc_funcs, "to_proto");
  pyolc_do_to_proto(to_proto, data, buf);
}

PyObject *pyolc_item_add_olc(PyObject *self, PyObject *args) {
  PyObject       *pymenu = NULL;
  PyObject    *pychooser = NULL;
  PyObject     *pyparser = NULL;
  PyObject *pyfrom_proto = NULL;
  PyObject   *pyto_proto = NULL;
  char             *type = NULL;

  // parse out our socket and functions
  if(!PyArg_ParseTuple(args, "sOOOOO", &type, &pymenu, &pychooser,&pyparser,
		       &pyfrom_proto, &pyto_proto)) {
    PyErr_Format(PyExc_TypeError,"Invalid number arguments supplied to do_olc");
    return NULL;
  }

  // what are all the functions we need to pass to item_add_olc?
  void       *menu = pyitem_olc_menu;
  void    *chooser = pyitem_olc_chooser;
  void     *parser = NULL;
  void *from_proto = NULL;
  void   *to_proto = NULL;

  // figure out our remaining C OLC functions, if any?
  if(pyparser != Py_None)
    parser = pyitem_olc_parser;
  if(pyfrom_proto != Py_None)
    from_proto = pyitem_olc_from_proto;
  if(pyto_proto != Py_None)
    to_proto = pyitem_olc_to_proto;

  // A new dictionary, and add all of our Python OLC functions
  PyObject *item_olc = PyDict_New();
  PyDict_SetItemString(item_olc, "menu",       pymenu);
  PyDict_SetItemString(item_olc, "chooser",    pychooser);
  PyDict_SetItemString(item_olc, "parser",     pyparser);
  PyDict_SetItemString(item_olc, "from_proto", pyfrom_proto);
  PyDict_SetItemString(item_olc, "to_proto",   pyto_proto);
  PyDict_SetItemString(pyitem_olc, type, item_olc);

  // add the OLC functions
  item_add_olc(type, menu, chooser, parser, from_proto, to_proto);
  return Py_BuildValue("O", Py_None);
}


PyObject *pyolc_extend(PyObject *self, PyObject *args) {
  PyObject       *pymenu = NULL;
  PyObject    *pychooser = NULL;
  PyObject     *pyparser = NULL;
  PyObject *pyfrom_proto = NULL;
  PyObject   *pyto_proto = NULL;
  char             *type = NULL;
  char              *opt = NULL;

  // parse out our socket and functions
  if(!PyArg_ParseTuple(args, "ssOO|OOO", &type, &opt, &pymenu, &pychooser,&pyparser,
		       &pyfrom_proto, &pyto_proto)) {
    PyErr_Format(PyExc_TypeError,"Invalid number arguments supplied to do_olc");
    return NULL;
  }

  // What OLC are we trying to extend?
  OLC_EXTENDER *ext = NULL;
  if(!strcmp(type, "medit"))
    ext = medit_extend;
  else if(!strcmp(type, "oedit"))
    ext = oedit_extend;
  else if(!strcmp(type, "redit"))
    ext = redit_extend;
  else {
    PyErr_Format(PyExc_TypeError,"Extend options: medit, oedit, and redit.");
    return NULL;
  }

  extenderRegisterPyOpt(ext, *opt, pymenu, pychooser, pyparser, pyfrom_proto,
			pyto_proto);
  return Py_BuildValue("i", 1);
}



//*****************************************************************************
// PyOLC module
//*****************************************************************************
void PyOLC_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pyolc_methods == NULL) pyolc_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pyolc_methods, def);
}

PyMODINIT_FUNC
init_PyOLC(void) {
  // add all of our methods
  PyOLC_addMethod("do_olc", pyolc_do_olc, METH_VARARGS,
    "do_olc(sock, menu_func, chooser_func, parse_func, saver_func, data, autosave=False)\n\n"
    "Entry point to the olc system. See olc2/olc.h for documentation.");
  PyOLC_addMethod("item_add_olc", pyolc_item_add_olc, METH_VARARGS, 
    "item_add_olc(itemtype, menu_func, chooser_func, parse_func,\n"
    "             fromproto_func, toproto_func)\n\n"
    "Register a new olc handler for an item type. See items/iedit.h for\n"
    "documentation.");
  PyOLC_addMethod("extend", pyolc_extend, METH_VARARGS,
    "extend(olc_type, optname, menu_func, chooser_func, parse_func = None,\n"
    "       fromproto_func = None, toproto_func = None)\n\n"
    "Register a new olc menu extender. Types are medit, redit, and oedit. See\n"
    "olc2/olc_extender.h for documentation.");

  // create the module
  PyObject *m = Py_InitModule3("olc", makePyMethods(pyolc_methods),
    "This is the Python wrapper for the online creation module. Allows users\n"
    "to set up new menus and handlers for editing data, such as characters,\n"
    "while online.");

  // set some important OLC values as well
  PyObject_SetAttrString(m, "MENU_NOCHOICE", Py_BuildValue("i", MENU_NOCHOICE));
  PyObject_SetAttrString(m, "MENU_CHOICE_INVALID",
			 Py_BuildValue("i", MENU_CHOICE_INVALID));
  PyObject_SetAttrString(m, "MENU_CHOICE_OK",
			 Py_BuildValue("i", MENU_CHOICE_OK));

  pyitem_olc = PyDict_New();
}
#endif // MODULE_OLC2
