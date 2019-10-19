//*****************************************************************************
//
// olc_extender.c
//
// A structure for helping in the extension of OLC functions. Allows for the
// registration of new commands, menu displays, parsers, and execution 
// functions.
//
//*****************************************************************************
#include "../mud.h"
#include "../socket.h"
#include "olc.h"
#include "olc_extender.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pyplugs.h"



//*****************************************************************************
// local variables, structures, and functions
//*****************************************************************************

// what kind of extension is this?
#define OLCEXT_C        0
#define OLCEXT_PY       1

// what is the minimum ID we use?
#define MIN_CHOICE_ID   10000

typedef struct olc_extender_data {
  // what type of extension is this?
  int type;

  // C functions
  void       (* menu)(SOCKET_DATA *sock, void *data);
  int (* choose_exec)(SOCKET_DATA *sock, void *data);
  bool (* parse_exec)(SOCKET_DATA *sock, void *data, const char *arg);
  void (* from_proto)(void *data);
  void   (* to_proto)(void *data, BUFFER *buf);

  // Python functions
  PyObject   *pymenu;
  PyObject *pychoose;
  PyObject  *pyparse;
  PyObject *pyfrompr;
  PyObject   *pytopr;
} OLC_EXT_DATA;

struct olc_extender {
  HASHTABLE *opt_hash;
  void *(* borrow_py)(void *data);
};

OLC_EXT_DATA *newOLCExt(void *menu, void *choose_exec, void *parse_exec,
			void *from_proto, void *to_proto) {
  OLC_EXT_DATA *ext = calloc(1, sizeof(OLC_EXT_DATA));
  ext->menu         = menu;
  ext->choose_exec  = choose_exec;
  ext->parse_exec   = parse_exec;
  ext->from_proto   = from_proto;
  ext->to_proto     = to_proto;
  ext->type = OLCEXT_C;
  return ext;
}

OLC_EXT_DATA *newPyOLCExt(PyObject *menu, PyObject *choose_exec, 
			  PyObject *parse_exec, PyObject *from_proto,
			  PyObject *to_proto) {
  OLC_EXT_DATA *ext = calloc(1, sizeof(OLC_EXT_DATA));
  ext->pymenu   = menu;        Py_XINCREF(menu);
  ext->pychoose = choose_exec; Py_XINCREF(choose_exec);
  ext->pyparse  = parse_exec;  Py_XINCREF(parse_exec);
  ext->pyfrompr = from_proto;  Py_XINCREF(from_proto);
  ext->pytopr   = to_proto;    Py_XINCREF(to_proto);
  ext->type = OLCEXT_PY;
  return ext;
}

void deleteOLCExt(OLC_EXT_DATA *ext) {
  Py_XDECREF(ext->pymenu);
  Py_XDECREF(ext->pychoose);
  Py_XDECREF(ext->pyparse);
  Py_XDECREF(ext->pyfrompr);
  Py_XDECREF(ext->pytopr);
  free(ext);
}



//*****************************************************************************
// implementation of olc_extender.h
//*****************************************************************************
OLC_EXTENDER *newExtender(void) {
  OLC_EXTENDER *data = calloc(1, sizeof(OLC_EXTENDER));
  data->opt_hash = newHashtable();
  return data;
}

void extenderSetPyFunc(OLC_EXTENDER *ext, void *borrow_py) {
  ext->borrow_py = borrow_py;
}

void extenderDoMenu(SOCKET_DATA *sock, OLC_EXTENDER *ext, void *data) {
  LIST           *keys = hashCollect(ext->opt_hash);
  char            *key = NULL;
  OLC_EXT_DATA  *edata = NULL;

  // display each menu item alphabetically
  listSortWith(keys, strcasecmp);
  LIST_ITERATOR *key_i = newListIterator(keys);
  ITERATE_LIST(key, key_i) {
    // display the menu option
    send_to_socket(sock, "{g%s) ", key);

    // then display the information
    edata = hashGet(ext->opt_hash, key);
    if(edata->type == OLCEXT_C)
      edata->menu(sock, data);
    else if(ext->borrow_py != NULL) {
      PyObject *ret = PyObject_CallFunction(edata->pymenu, "OO", 
					    socketGetPyFormBorrowed(sock), 
					    ext->borrow_py(data));
      if(ret == NULL)
	log_pyerr("Error running Python OLC exention menu function: %s", key);
      Py_XDECREF(ret);
    }
  } deleteListIterator(key_i);

  // free up our garbage
  deleteListWith(keys, free);
}

int extenderDoOptChoice(SOCKET_DATA *sock, OLC_EXTENDER *ext, void *data, 
			char opt) {
  char key[2] = { opt, '\0' };

  // does it exist?
  if(!hashIn(ext->opt_hash, key))
    return MENU_CHOICE_INVALID;

  // pull out the data
  OLC_EXT_DATA *edata = hashGet(ext->opt_hash, key);
  int          retval = MENU_CHOICE_INVALID;

  // is it C data or Python data?
  if(edata->type == OLCEXT_C)
    retval = edata->choose_exec(sock, data);
  else if(ext->borrow_py != NULL) {
    PyObject *ret = 
      PyObject_CallFunction(edata->pychoose, "OO", socketGetPyFormBorrowed(sock), ext->borrow_py(data));
    if(ret == NULL)
      log_pyerr("Error running Python OLC exention choice function: %s", key);
    else if(PyInt_Check(ret))
      retval = (int)PyInt_AsLong(ret);
    Py_XDECREF(ret);
  }

  // did we do a toggle, or enter a submenu?
  if(retval == MENU_NOCHOICE || retval == MENU_CHOICE_INVALID)
    return retval;
  return MIN_CHOICE_ID + opt;
}

bool extenderDoParse(SOCKET_DATA *sock, OLC_EXTENDER *ext, void *data, 
		     int choice, const char *arg) {
  char key[2] = { (char)(choice - MIN_CHOICE_ID), '\0' };

  // pull out the data
  OLC_EXT_DATA *edata = hashGet(ext->opt_hash, key);
  int          retval = FALSE;

  // is it C data or Python data?
  if(edata->type == OLCEXT_C)
    retval = edata->parse_exec(sock, data, arg);
  else if(ext->borrow_py != NULL) {
    PyObject *pyret = 
      PyObject_CallFunction(edata->pyparse, "Os", ext->borrow_py(data), arg);
    if(pyret == NULL)
      log_pyerr("Error running Python OLC exention parse function: %s", key);
    else if(PyObject_IsTrue(pyret))
      retval = TRUE;
    Py_XDECREF(pyret);
  }

  // was it valid?
  return retval;
}

void extenderToProto(OLC_EXTENDER *ext, void *data, BUFFER *buf) {
  LIST           *keys = hashCollect(ext->opt_hash);
  char            *key = NULL;
  OLC_EXT_DATA  *edata = NULL;

  // display each menu item alphabetically
  listSortWith(keys, strcasecmp);
  LIST_ITERATOR *key_i = newListIterator(keys);
  ITERATE_LIST(key, key_i) {
    edata = hashGet(ext->opt_hash, key);

    // is it C data?
    if(edata->type == OLCEXT_C) {
      if(edata->to_proto != NULL)
	edata->to_proto(data, buf);
    }
    // is it Python data?
    else if(ext->borrow_py != NULL) {
      if(edata->pytopr != NULL && edata->pytopr != Py_None) {
	PyObject *ret = PyObject_CallFunction(edata->pytopr, "O",
					      ext->borrow_py(data));
	if(ret == NULL)
	  log_pyerr("Error running Python OLC to_proto function: %s", key);
	else
	  bufferCat(buf, PyString_AsString(ret));
	Py_XDECREF(ret);
      }
    }
  } deleteListIterator(key_i);

  // free up our garbage
  deleteListWith(keys, free);
}

void extenderFromProto(OLC_EXTENDER *ext, void *data) {
  LIST           *keys = hashCollect(ext->opt_hash);
  char            *key = NULL;
  OLC_EXT_DATA  *edata = NULL;

  // display each menu item alphabetically
  listSortWith(keys, strcasecmp);
  LIST_ITERATOR *key_i = newListIterator(keys);
  ITERATE_LIST(key, key_i) {
    edata = hashGet(ext->opt_hash, key);

    // is it C data?
    if(edata->type == OLCEXT_C) {
      if(edata->from_proto != NULL)
	edata->from_proto(data);
    }
    // is it Python data?
    else if(ext->borrow_py != NULL) {
      if(edata->pyfrompr != NULL && edata->pyfrompr != Py_None) {
	PyObject *ret = PyObject_CallFunction(edata->pyfrompr, "O",
					      ext->borrow_py(data));
	if(ret == NULL)
	  log_pyerr("Error running Python OLC from_proto function: %s", key);
	Py_XDECREF(ret);
      }
    }
  } deleteListIterator(key_i);

  // free up our garbage
  deleteListWith(keys, free);
}

void gen_register_opt(OLC_EXTENDER *ext, char opt, OLC_EXT_DATA *data) {
  char key[2] = { opt, '\0' };

  // do we already have this registered? If so, clear it
  if(hashIn(ext->opt_hash, key))
    deleteOLCExt(hashRemove(ext->opt_hash, key));
  hashPut(ext->opt_hash, key, data);
}

void extenderRegisterOpt(OLC_EXTENDER *ext, char opt, 
			 void *menu, void *choice, void *parse, 
			 void *from_proto, void *to_proto) {
  gen_register_opt(ext, opt, newOLCExt(menu,choice,parse,from_proto,to_proto));
}

//
// The same, but takes Python functions instead of C function
void extenderRegisterPyOpt(OLC_EXTENDER *ext, char opt, 
			   void *menu, void *choice, void *parse,
			   void *from_proto, void *to_proto) {
  gen_register_opt(ext,opt,newPyOLCExt(menu,choice,parse,from_proto,to_proto));
}
