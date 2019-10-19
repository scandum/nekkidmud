//*****************************************************************************
//
// pyobj.c
//
// A python extention to allow python scripts to treat MUD objects as a Python
// object within the script.
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../utils.h"
#include "../world.h"
#include "../room.h"
#include "../character.h"
#include "../object.h"
#include "../races.h"
#include "../handler.h"
#include "../extra_descs.h"
#include "../prototype.h"

#include "pyplugs.h"
#include "scripts.h"
#include "pychar.h"
#include "pyroom.h"
#include "pyobj.h"
#include "pyauxiliary.h"
#include "pystorage.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../dyn_vars/dyn_vars.h"
#include "../items/items.h"
#include "../items/worn.h"



//*****************************************************************************
// local structures and defines
//*****************************************************************************
// a list of the get/setters on the Obj class
LIST *pyobj_getsetters = NULL;

// a list of the methods on the Obj class
LIST *pyobj_methods = NULL;

typedef struct {
  PyObject_HEAD
  int uid;
} PyObj;



//*****************************************************************************
// allocation, deallocation, initialization, and comparison
//*****************************************************************************
void PyObj_dealloc(PyObj *self) {
  self->ob_type->tp_free((PyObject*)self);
}

PyObject *PyObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyObj *self;

    self = (PyObj  *)type->tp_alloc(type, 0);
    self->uid = NOTHING;
    return (PyObject *)self;
}

int PyObj_init(PyObj *self, PyObject *args, PyObject *kwds) {
  char *kwlist[] = {"uid", NULL};
  int uid = NOTHING;

  // get the universal id
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &uid)) {
    PyErr_Format(PyExc_TypeError, 
                    "Objects may only be created by uid");
    return -1;
  }

  // make sure a object with the UID exists
  if(!propertyTableGet(obj_table, uid)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object with uid, %d, does not exist", uid);
    return -1;
  }

  self->uid = uid;
  return 0;
}

int PyObj_compare(PyObj *obj1, PyObj *obj2) {
  if(obj1->uid == obj2->uid)
    return 0;
  else if(obj1->uid < obj2->uid)
    return -1;
  else
    return 1;
}



//*****************************************************************************
// getters and setters for the Obj class
//*****************************************************************************
PyObject *PyObj_getname(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) return Py_BuildValue("s", objGetName(obj));
  else            return NULL;
}

PyObject *PyObj_getmname(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) return Py_BuildValue("s", objGetMultiName(obj));
  else            return NULL;
}

PyObject *PyObj_getbits(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) return Py_BuildValue("s", bitvectorGetBits(objGetBits(obj)));
  else            return NULL;
}

PyObject *PyObj_getkeywords(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) return Py_BuildValue("s", objGetKeywords(obj));
  else            return NULL;
}

PyObject *PyObj_getdesc(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) return Py_BuildValue("s", objGetDesc(obj));
  else            return NULL;
}

PyObject *PyObj_getrdesc(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) return Py_BuildValue("s", objGetRdesc(obj));
  else            return NULL;
}

PyObject *PyObj_getmdesc(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) return Py_BuildValue("s", objGetMultiRdesc(obj));
  else            return NULL;
}

PyObject *PyObj_getuid(PyObj *self, void *closure) {
  return Py_BuildValue("i", self->uid);
}

PyObject *PyObj_getprototypes(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) return Py_BuildValue("s", objGetPrototypes(obj));
  else           return NULL;
}

PyObject *PyObj_getweight(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) return Py_BuildValue("d", objGetWeight(obj));
  else            return NULL;
}


PyObject *PyObj_get_weight_raw(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) return Py_BuildValue("d", objGetWeightRaw(obj));
  else            return NULL;
}

PyObject *PyObj_gethidden(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj != NULL) return Py_BuildValue("i", objGetHidden(obj));
  else            return NULL;  
}

PyObject *PyObj_getbirth(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj != NULL) return Py_BuildValue("i", objGetBirth(obj));
  else            return NULL;  
}

PyObject *PyObj_getage(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj != NULL) return Py_BuildValue("d", difftime(current_time, objGetBirth(obj)));
  else            return NULL;  
}

PyObject *PyObj_getcontents(PyObj *self, PyObject *args) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj == NULL) 
    return NULL;

  LIST_ITERATOR *cont_i = newListIterator(objGetContents(obj));
  PyObject *list = PyList_New(0);
  OBJ_DATA *cont;
  
  // for each obj in the contentory, add it to the Python list
  ITERATE_LIST(cont, cont_i)
    PyList_Append(list, objGetPyFormBorrowed(cont));
  deleteListIterator(cont_i);
  PyObject *retval = Py_BuildValue("O", list);
  Py_DECREF(list);
  return retval;
}

PyObject *PyObj_getchars(PyObj *self, PyObject *args) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj == NULL) 
    return NULL;

  LIST_ITERATOR *char_i = newListIterator(objGetUsers(obj));
  PyObject *list = PyList_New(0);
  CHAR_DATA *ch;
  
  // for each obj in the contentory, add it to the Python list
  ITERATE_LIST(ch, char_i)
    PyList_Append(list, charGetPyFormBorrowed(ch));
  deleteListIterator(char_i);
  PyObject *retval = Py_BuildValue("O", list);
  Py_DECREF(list);
  return retval;
}

PyObject *PyObj_getcarrier(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj == NULL)
    return NULL;
  if(objGetCarrier(obj) == NULL)
    return Py_BuildValue("O", Py_None);
  return Py_BuildValue("O", charGetPyFormBorrowed(objGetCarrier(obj)));
}

PyObject *PyObj_getwearer(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj == NULL)
    return NULL;
  if(objGetWearer(obj) == NULL)
    return Py_BuildValue("O", Py_None);
  return Py_BuildValue("O", charGetPyFormBorrowed(objGetWearer(obj)));
}

PyObject *PyObj_getroom(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj == NULL)
    return NULL;
  if(objGetRoom(obj) == NULL)
    return Py_BuildValue("O", Py_None);
  return Py_BuildValue("O", roomGetPyFormBorrowed(objGetRoom(obj)));
}

PyObject *PyObj_getcontainer(PyObj *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj == NULL)
    return NULL;
  if(objGetContainer(obj) == NULL)
    return Py_BuildValue("O", Py_None);
  return Py_BuildValue("O", objGetPyFormBorrowed(objGetContainer(obj)));
}

//
// Standard check to make sure the object exists when
// trying to set a value for it. If successful, assign the
// object to ch. Otherwise, return -1 (error)
#define PYOBJ_CHECK_OBJ_EXISTS(uid, obj)                                       \
  obj = propertyTableGet(obj_table, uid);                                      \
  if(obj == NULL) {                                                            \
    PyErr_Format(PyExc_TypeError,                                              \
		    "Tried to modify nonexistant object, %d", uid);            \
    return -1;                                                                 \
  }                                                                            

int PyObj_setname(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's name");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object names must be strings");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  objSetName(obj, PyString_AsString(value));
  return 0;
}

int PyObj_setmname(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's multi-name");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object multi-names must be strings");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  objSetMultiName(obj, PyString_AsString(value));
  return 0;
}

int PyObj_setbits(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's bits");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object bits must be strings");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  bitClear(objGetBits(obj));
  bitSet(objGetBits(obj), PyString_AsString(value));
  return 0;
}

int PyObj_setkeywords(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's keywords");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object keywords must be strings");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);

  // clean up empty keywords, and rebuild it
  LIST           *kwds = parse_keywords(PyString_AsString(value));
  BUFFER     *new_kwds = newBuffer(1);
  LIST_ITERATOR *kwd_i = newListIterator(kwds);
  char            *kwd = NULL;

  ITERATE_LIST(kwd, kwd_i) {
    // if we already have content, add a comma
    if(bufferLength(new_kwds) > 0)
      bufferCat(new_kwds, ", ");
    bufferCat(new_kwds, kwd);
  } deleteListIterator(kwd_i);

  // set our keywords
  objSetKeywords(obj, bufferString(new_kwds));

  // garbage collection
  deleteListWith(kwds, free);
  deleteBuffer(new_kwds);
  return 0;
}

int PyObj_setdesc(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's description");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object descriptions must be strings");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  objSetDesc(obj, PyString_AsString(value));
  return 0;
}

int PyObj_setrdesc(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's rdesc");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object rdescs must be strings");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  objSetRdesc(obj, PyString_AsString(value));
  return 0;
}

int PyObj_setmdesc(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's multi-rdesc");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Object multi-rdescs must be strings");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  objSetMultiRdesc(obj, PyString_AsString(value));
  return 0;
}

int PyObj_setweight(PyObj *self, PyObject *value, void *closure) {
  double weight = 0;
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's weight");
    return -1;
  }

  if(PyFloat_Check(value))
    weight = PyFloat_AsDouble(value);
  else if(PyInt_Check(value))
    weight = PyInt_AsLong(value);
  else {
    PyErr_Format(PyExc_TypeError, "Object weight must be a numeric value.");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  objSetWeightRaw(obj, weight);
  return 0;
}

int PyObj_sethidden(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = NULL;
  PYOBJ_CHECK_OBJ_EXISTS(PyObj_AsUid(self), obj);

  if(value == NULL || value == Py_None)
    objSetHidden(obj, 0);
  else if(PyInt_Check(value))
    objSetHidden(obj, PyInt_AsLong(value));
  else {
    PyErr_Format(PyExc_TypeError,
		"Tried to change obj %d's spot difficulty to an invalid type.",
		 objGetUID(obj));
    return -1;
  }

  return 0;
}

int PyObj_setcarrier(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's carrier");
    return -1;
  }

  if (!PyChar_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Carrier must be a character!");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  CHAR_DATA *carrier = PyChar_AsChar(value);
  // remove us from whatever we're currently in
  if(objGetRoom(obj))
    obj_from_room(obj);
  if(objGetCarrier(obj))
    obj_from_char(obj);
  if(objGetContainer(obj))
    obj_from_obj(obj);
  if(objGetWearer(obj)) {
    // weird... we couldn't unequip the item from the current wearer
    if(!try_unequip(objGetWearer(obj), obj)) {
      PyErr_Format(PyExc_StandardError, "Could not unequip previous wearer.");
      return -1;
    }
  }

  // give the obj to the character
  obj_to_char(obj, carrier);
  return 0;
}

int PyObj_setroom(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's room");
    return -1;
  }

  ROOM_DATA *room = NULL;

  if(PyRoom_Check(value))
    room = PyRoom_AsRoom(value);
  else if(PyString_Check(value))
    room = worldGetRoom(gameworld, 
			get_fullkey_relative(PyString_AsString(value),
					     get_script_locale()));
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Object's room must be a string value or a "
		 "room object.");
    return -1;
  }

  OBJ_DATA *obj;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  // remove us from whatever we're currently in
  if(objGetRoom(obj))
    obj_from_room(obj);
  if(objGetCarrier(obj))
    obj_from_char(obj);
  if(objGetContainer(obj))
    obj_from_obj(obj);
  if(objGetWearer(obj)) {
    // weird... we couldn't unequip the item from the current wearer
    if(!try_unequip(objGetWearer(obj), obj)) {
      PyErr_Format(PyExc_StandardError, "Could not unequip wearer.");
      return -1;
    }
  }

  // give the obj to the character
  obj_to_room(obj, room);
  return 0;
}

int PyObj_setcontainer(PyObj *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete object's container");
    return -1;
  }

  if (!PyObj_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Container must be an object!");
    return -1;
  }

  OBJ_DATA *obj, *cont;
  PYOBJ_CHECK_OBJ_EXISTS(self->uid, obj);
  PYOBJ_CHECK_OBJ_EXISTS(((PyObj *)value)->uid, cont);
  // remove us from whatever we're currently in
  if(objGetRoom(obj))
    obj_from_room(obj);
  if(objGetCarrier(obj))
    obj_from_char(obj);
  if(objGetContainer(obj))
    obj_from_obj(obj);
  if(objGetWearer(obj)) {
    // weird... we couldn't unequip the item from the current wearer
    if(!try_unequip(objGetWearer(obj), obj)) {
      PyErr_Format(PyExc_StandardError, "Could not unequip wearer.");
      return -1;
    }
  }

  // give the obj to the character
  obj_to_obj(obj, cont);
  return 0;
}



//*****************************************************************************
// methods for the obj class
//*****************************************************************************
PyObject *PyObj_attach(PyObj *self, PyObject *args) {  
  char *key = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "To attach a trigger, the key must be suppplied.");
    return NULL;
  }

  // pull out the character and do the attaching
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach trigger to nonexistant obj, %d.", self->uid);
    return NULL;
  }

  TRIGGER_DATA *trig =
    worldGetType(gameworld, "trigger", 
		 get_fullkey_relative(key, get_script_locale()));
  if(trig != NULL) {
    triggerListAdd(objGetTriggers(obj), triggerGetKey(trig));
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to attach nonexistant script, %s, to object %s.",
		 key, objGetClass(obj));
    return NULL;
  }
}

PyObject *PyObj_detach(PyObj *self, PyObject *args) {  
  char *key = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "To detach a trigger, its key must be suppplied.");
    return NULL;
  }

  // pull out the character and do the attaching
  OBJ_DATA    *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) {
    const char *fkey = get_fullkey_relative(key, get_script_locale());
    triggerListRemove(objGetTriggers(obj), fkey);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to detach script from nonexistant obj, %d.", self->uid);
    return NULL;
  }
}

PyObject *PyObj_isinstance(PyObj *self, PyObject *args) {  
  char *type = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &type)) {
    PyErr_Format(PyExc_TypeError, "isinstance only accepts strings.");
    return NULL;
  }

  // pull out the object and check the type
  OBJ_DATA    *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) {
    PyObject *retval = NULL;
    char     *locale = NULL;
    if(get_script_locale())
      locale = strdup(get_script_locale());
    else
      locale = strdup(get_key_locale(objGetClass(obj)));

    retval = 
      Py_BuildValue("i", objIsInstance(obj, get_fullkey_relative(type,locale)));
    free(locale);
    return retval;
  }
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to check instances of nonexistent object, %d.", self->uid);
    return NULL;
  }
}

//
// create a new extra description for the object
PyObject *PyObj_edesc(PyObj *self, PyObject *value) {
  char *keywords = NULL;
  char     *desc = NULL;

  if (!PyArg_ParseTuple(value, "ss", &keywords, &desc)) {
    PyErr_Format(PyExc_TypeError, "Extra descs must be strings.");
    return NULL;
  }

  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) {
    EDESC_DATA *edesc = newEdesc(keywords, desc);
    edescSetPut(objGetEdescs(obj), edesc);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError,
		 "Tried to set edesc for nonexistent obj, %d.", self->uid);
    return NULL;
  }
}

//
// returns the specified piece of auxiliary data from the object
// if it is a piece of python auxiliary data.
PyObject *PyObj_get_auxiliary(PyObj *self, PyObject *args) {
  char *keyword = NULL;
  if(!PyArg_ParseTuple(args, "s", &keyword)) {
    PyErr_Format(PyExc_TypeError,
		 "getAuxiliary() must be supplied with the name that the "
		 "auxiliary data was installed under!");
    return NULL;
  }

  // make sure we exist
  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to get auxiliary data for a nonexistant object.");
    return NULL;
  }

  // make sure the auxiliary data exists
  if(!pyAuxiliaryDataExists(keyword)) {
    PyErr_Format(PyExc_StandardError,
		 "No auxiliary data named '%s' exists!", keyword);
    return NULL;
  }

  PyObject *data = objGetAuxiliaryData(obj, keyword);
  if(data == NULL)
    data = Py_None;
  PyObject *retval = Py_BuildValue("O", data);
  //  Py_DECREF(data);
  return retval;
}


//
// Returns TRUE if the obj has the given variable set
PyObject *PyObj_hasvar(PyObj *self, PyObject *arg) {
  char *var = NULL;
  if (!PyArg_ParseTuple(arg, "s", &var)) {
    PyErr_Format(PyExc_TypeError, 
                    "Obj variables must have string names.");
    return NULL;
  }

  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL)
    return Py_BuildValue("b", objHasVar(obj, var));

  PyErr_Format(PyExc_TypeError, 
	       "Tried to get a variable value for nonexistant obj, %d",
	       self->uid);
  return NULL;
}


//
// Delete the variable set on the obj with the specified name
PyObject *PyObj_deletevar(PyObj *self, PyObject *arg) {
  char *var = NULL;
  if (!PyArg_ParseTuple(arg, "s", &var)) {
    PyErr_Format(PyExc_TypeError, 
                    "Obj variables must have string names.");
    return NULL;
  }

  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) {
    objDeleteVar(obj, var);
    return Py_BuildValue("i", 1);
  }

  PyErr_Format(PyExc_TypeError, 
	       "Tried to get a variable value for nonexistant obj, %d",
	       self->uid);
  return NULL;
}


//
// Get the value of a variable stored on the obj
PyObject *PyObj_getvar(PyObj *self, PyObject *arg) {
  char *var = NULL;
  if (!PyArg_ParseTuple(arg, "s", &var)) {
    PyErr_Format(PyExc_TypeError, 
                    "Obj variables must have string names.");
    return NULL;
  }

  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) {
    int vartype = objGetVarType(obj, var);
    if(vartype == DYN_VAR_INT)
      return Py_BuildValue("i", objGetInt(obj, var));
    else if(vartype == DYN_VAR_LONG)
      return Py_BuildValue("i", objGetLong(obj, var));
    else if(vartype == DYN_VAR_DOUBLE)
      return Py_BuildValue("d", objGetDouble(obj, var));
    else
      return Py_BuildValue("s", objGetString(obj, var));
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to get a variable value for nonexistant obj, %d",
		 self->uid);
    return NULL;
  }
}


//
// Set the value of a variable assocciated with the character
PyObject *PyObj_setvar(PyObj *self, PyObject *args) {  
  char     *var = NULL;
  PyObject *val = NULL;

  if (!PyArg_ParseTuple(args, "sO", &var, &val)) {
    PyErr_Format(PyExc_TypeError, 
		 "Obj setvar must be supplied with a var name and integer value.");
    return NULL;
  }

  OBJ_DATA *obj = PyObj_AsObj((PyObject *)self);
  if(obj != NULL) {
    if(PyInt_Check(val))
      objSetInt(obj, var, (int)PyInt_AsLong(val));
    else if(PyFloat_Check(val))
      objSetDouble(obj, var, PyFloat_AsDouble(val));
    else if(PyString_Check(val))
      objSetString(obj, var, PyString_AsString(val));
    else {
      PyErr_Format(PyExc_TypeError,
		   "Tried to store a obj_var of invalid type on obj %d.",
		   self->uid);
      return NULL;
    }
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to set a variable value for nonexistant obj, %d",
		 self->uid);
    return NULL;
  }
}

PyObject *PyObj_fromall(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to fromall() nonexistant obj, %d",
		 PyObj_AsUid(self));
    return NULL;
  }

  // remove us from whatever we're currently in
  if(objGetRoom(obj))
    obj_from_room(obj);
  if(objGetCarrier(obj))
    obj_from_char(obj);
  if(objGetContainer(obj))
    obj_from_obj(obj);
  if(objGetWearer(obj)) {
    // weird... we couldn't unequip the item from the current wearer
    if(!try_unequip(objGetWearer(obj), obj)) {
      PyErr_Format(PyExc_StandardError, "Could not unequip previous wearer.");
      return NULL;
    }
  }

  return Py_BuildValue("");
}

PyObject *PyObj_store(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_TypeError, "failed to store nonexistent object.");
    return NULL;
  }
  PyObject *set = newPyStorageSet(objStore(obj));
  PyObject *ret = Py_BuildValue("O", set);
  Py_DECREF(set);
  return ret;
}

PyObject *PyObj_copy(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_TypeError, "failed to copy nonexistent object.");
    return NULL;
  }
  OBJ_DATA *newobj = objCopy(obj);

  // we have to put the object in the global tables and list, 
  // or else Python will not be able to access it
  obj_to_game(newobj);

  return objGetPyForm(newobj);
}



//*****************************************************************************
// structures to define our methods and classes
//*****************************************************************************
PyTypeObject PyObj_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "obj.Obj",                 /*tp_name*/
    sizeof(PyObj),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyObj_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    (cmpfunc)PyObj_compare,    /*tp_compare*/
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
    "Obj/Obj objects",         /* tp_doc */
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
    (initproc)PyObj_init,      /* tp_init */
    0,                         /* tp_alloc */
    PyObj_new,                 /* tp_new */
};



//*****************************************************************************
// the obj module
//*****************************************************************************
PyObject *PyObj_load_obj(PyObject *self, PyObject *args) {
  char          *key = NULL;
  PyObject       *in = Py_None;
  ROOM_DATA    *room = NULL; // are we loading to a room?
  OBJ_DATA     *cont = NULL; // are we loading to a container?
  CHAR_DATA      *ch = NULL; // are we loading to a character?
  char     *equip_to = NULL; // are we trying to equip the character?

  if (!PyArg_ParseTuple(args, "s|Os", &key, &in, &equip_to)) {
    PyErr_Format(PyExc_TypeError, 
		 "Load obj failed - it needs a key and destination.");
    return NULL;
  }

  // figure out what we're trying to load this thing into
  if(PyString_Check(in))
    room = worldGetRoom(gameworld, PyString_AsString(in));
  else if(PyRoom_Check(in))
    room = PyRoom_AsRoom(in);
  else if(PyObj_Check(in))
    cont = propertyTableGet(obj_table, PyObj_AsUid(in));
  else if(PyChar_Check(in))
    ch = propertyTableGet(mob_table, PyChar_AsUid(in));

  // make sure a destination exists
  if(room == NULL && cont == NULL && ch == NULL && in != Py_None) {
    PyErr_Format(PyExc_TypeError,
		 "Load obj failed: destination does not exist.");
    return NULL;
  }

  // check the obj
  PROTO_DATA *obj_proto = 
    worldGetType(gameworld, "oproto", 
		 get_fullkey_relative(key, get_script_locale()));
  if(obj_proto == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Load obj failed: oproto %s does not exist.", 
		 get_fullkey_relative(key, get_script_locale()));
    return NULL;
  }

  // copy the object
  OBJ_DATA *obj = protoObjRun(obj_proto);
  if(obj == NULL) {
    //    PyErr_Format(PyExc_TypeError,
    //		 "Load obj failed: proto script terminated with an error.");
    return NULL;
  }

  // figure out where we're trying to load the object to
  if(room != NULL)
    obj_to_room(obj, room);
  else if(cont != NULL)
    obj_to_obj(obj, cont);
  else if(ch != NULL) {
    // we're just trying to send it to our inventory
    if(equip_to == NULL)
      obj_to_char(obj, ch);
    // trying to equip to our default slots
    else if(!*equip_to && objIsType(obj, "worn")) {
      if(!try_equip(ch, obj, NULL, wornGetPositions(obj)))
	obj_to_char(obj, ch);
    }
    // trying to equip to specific slots
    else if(*equip_to) {
      if(!try_equip(ch, obj, equip_to, NULL))
	obj_to_char(obj, ch);
    }
    // can't equip it in any other case -- send it to the inventory
    else
      obj_to_char(obj, ch);
  }

  // create a python object for the new obj, and return it
  return Py_BuildValue("O", objGetPyFormBorrowed(obj));
}


PyObject *PyObj_count_objs(PyObject *self, PyObject *args) {
  LIST            *list = NULL;
  char             *tgt = NULL;
  PyObject          *in = NULL;
  ROOM_DATA       *room = NULL;
  OBJ_DATA        *cont = NULL;
  CHAR_DATA         *ch = NULL;
  const char *prototype = NULL;

  if (!PyArg_ParseTuple(args, "s|O", &tgt, &in)) {
    PyErr_Format(PyExc_TypeError, "count_objs failed. No arguments supplied.");
    return NULL;
  }

  // get the full key for our prototype
  prototype = get_fullkey_relative(tgt, get_script_locale());

  // if we didn't supply something to look in, assume it means the world
  if(in == NULL)
    return Py_BuildValue("i", count_objs(NULL, object_list, NULL, prototype, 
					 FALSE));

  // see what we're looking in
  if(PyString_Check(in))
    room = worldGetRoom(gameworld, PyString_AsString(in));
  else if(PyRoom_Check(in))
    room = PyRoom_AsRoom(in);
  else if(PyObj_Check(in))
    cont = propertyTableGet(obj_table, PyObj_AsUid(in));
  else if(PyChar_Check(in))
    ch   = propertyTableGet(mob_table, PyChar_AsUid(in));

  // now find the list we're dealing with
  if(room) list = roomGetContents(room);
  if(cont) list = objGetContents(cont);
  if(ch)   list = charGetInventory(ch);

  if(list == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "count_objs failed. invalid argument supplied.");
    return NULL;
  }
  
  return Py_BuildValue("i", count_objs(NULL, list, NULL, prototype, FALSE));
}


PyObject *PyObj_find_obj(PyObject *self, PyObject *args) {
  LIST *list           = object_list;
  PyObject *in         = NULL;
  ROOM_DATA *room      = NULL;
  OBJ_DATA  *cont      = NULL;
  CHAR_DATA *ch        = NULL;
  CHAR_DATA *looker_ch = NULL;
  PyObject *looker     = NULL;
  char *tgt            = NULL;

  if (!PyArg_ParseTuple(args, "s|OO", &tgt, &in, &looker)) {
    PyErr_Format(PyExc_TypeError, 
                    "find_obj failed. No arguments supplied.");
    return NULL;
  }

  // check for scope of search
  if(in) {
    if(PyString_Check(in))
      room = worldGetRoom(gameworld, PyString_AsString(in));
    else if(PyRoom_Check(in))
      room = PyRoom_AsRoom(in);
    else if(PyObj_Check(in))
      cont = propertyTableGet(obj_table, PyObj_AsUid(in));
    else if(PyChar_Check(in))
      ch   = propertyTableGet(mob_table, PyChar_AsUid(in));
  }

  // check to see who's looking
  if(looker && PyChar_Check(looker))
    looker_ch = propertyTableGet(mob_table, PyChar_AsUid(looker));

  // now find the list we're dealing with
  if(room) list = roomGetContents(room);
  if(cont) list = objGetContents(cont);
  if(ch)   list = charGetInventory(ch);

  // now, do the search
  int count = 1;
  char name[SMALL_BUFFER] = "";
  get_count(tgt, name, &count);

  // we're just looking for a single item
  if(count != COUNT_ALL) {
    OBJ_DATA *obj    = find_obj(looker_ch, list, count, name, NULL, 
				(looker_ch ? TRUE : FALSE));
    PyObject *py_obj = Py_None;
    if(obj) py_obj = objGetPyFormBorrowed(obj);
    return Py_BuildValue("O", py_obj);
  }
  // otherwise, return everything that meets our critereon
  else {
    //***********
    // FINISH ME
    //***********
    return Py_BuildValue("O", Py_None);
  }
}

PyObject *PyObj_find_obj_key(PyObject *self, PyObject *args) {
  LIST *list           = object_list;
  PyObject *in         = NULL;
  ROOM_DATA *room      = NULL;
  OBJ_DATA  *cont      = NULL;
  CHAR_DATA *ch        = NULL;
  CHAR_DATA *looker_ch = NULL;
  PyObject *looker     = NULL;
  char *tgt            = NULL;
  const char *key      = NULL;

  if (!PyArg_ParseTuple(args, "s|OO", &tgt, &in, &looker)) {
    PyErr_Format(PyExc_TypeError, 
                    "find_obj failed. No arguments supplied.");
    return NULL;
  }

  // figure out our key
  key = get_fullkey_relative(tgt, get_script_locale());

  // check for scope of search
  if(in) {
    if(PyString_Check(in))
      room = worldGetRoom(gameworld, PyString_AsString(in));
    else if(PyRoom_Check(in))
      room = PyRoom_AsRoom(in);
    else if(PyObj_Check(in))
      cont = propertyTableGet(obj_table, PyObj_AsUid(in));
    else if(PyChar_Check(in))
      ch   = propertyTableGet(mob_table, PyChar_AsUid(in));
  }

  // check to see who's looking
  if(looker && PyChar_Check(looker))
    looker_ch = propertyTableGet(mob_table, PyChar_AsUid(looker));

  // now, do the search
  int count = 1;
  char name[SMALL_BUFFER] = "";
  get_count(tgt, name, &count);

  // we're just looking for a single item
  if(count != COUNT_ALL) {
    OBJ_DATA *obj    = find_obj(looker_ch, list, count, NULL, key,
				(looker_ch ? TRUE : FALSE));
    PyObject *py_obj = Py_None;
    if(obj) py_obj = objGetPyFormBorrowed(obj);
    return Py_BuildValue("O", py_obj);
  }
  // otherwise, return everything that meets our critereon
  else {
    //***********
    // FINISH ME
    //***********
    return Py_BuildValue("O", Py_None);
  }
}

PyObject *PyObj_all_objs(PyObject *self) {
  PyObject      *list = PyList_New(0);
  LIST_ITERATOR *obj_i = newListIterator(object_list);
  OBJ_DATA        *obj = NULL;
  ITERATE_LIST(obj, obj_i) {
    PyList_Append(list, objGetPyFormBorrowed(obj));
  } deleteListIterator(obj_i);
  return list;
}

PyObject *PyObj_read(PyObject *self, PyObject *args) {
  PyObject *pyset = NULL;
  if(!PyArg_ParseTuple(args, "O", &pyset)) {
    PyErr_Format(PyExc_TypeError, "failed to read object from storage set.");
    return NULL;
  }
  else if(!PyStorageSet_Check(pyset)) {
    PyErr_Format(PyExc_TypeError, "storage set must be supplied to read.");
    return NULL;
  }

  OBJ_DATA *obj = objRead(PyStorageSet_AsSet(pyset));
  obj_to_game(obj);
  return Py_BuildValue("O", objGetPyFormBorrowed(obj));
}

PyMethodDef obj_module_methods[] = {
  { "read",     PyObj_read, METH_VARARGS,
    "read(storage_set)\n"
    "\n"
    "Read and return an object from a storage set." },
  { "obj_list", (PyCFunction)PyObj_all_objs, METH_NOARGS,
    "obj_list()\n"
    "\n"
    "Return a list containing every object in the game." },
  { "load_obj", PyObj_load_obj, METH_VARARGS,
    "load_obj(prototype, where=None, equip_to='')\n"
    "\n"
    "Generate a new object from the specified prototype. Add it to where.\n"
    "Where can be a room, character, or container. If where is a character,\n"
    "add the object to the character's inventory unless a comma-separated\n"
    "list of bodypart name of positions is specified. Return the created object." },
  { "count_objs", PyObj_count_objs, METH_VARARGS,
    "count_objs(keyword, loc = None)\n"
    "\n"
    "count how many occurences of an object with the specified keyword, uid,\n"
    "or prototype exist at a location. If loc is None, search the entire mud.\n"
    "Loc can be a room, character, or container object." },
  { "find_obj", PyObj_find_obj, METH_VARARGS,
    "function has been deprecated. Entrypoint for generic_find().\n"
    "Use mud.parse_args instead." },
  { "find_obj_key", PyObj_find_obj_key, METH_VARARGS,
    "function has been deprecated. Entrypoint for generic_find().\n"
    "Use mud.parse_args instead." },
  {NULL, NULL, 0, NULL}  /* Sentinel */
};



//*****************************************************************************
// implementation of pyobj.h
//*****************************************************************************
void PyObj_addGetSetter(const char *name, void *g, void *s, const char *doc) {
  // make sure our list of get/setters is created
  if(pyobj_getsetters == NULL) pyobj_getsetters = newList();

  // make the GetSetter def
  PyGetSetDef *def = calloc(1, sizeof(PyGetSetDef));
  def->name        = strdup(name);
  def->get         = (getter)g;
  def->set         = (setter)s;
  def->doc         = (doc ? strdup(doc) : NULL);
  def->closure     = NULL;
  listPut(pyobj_getsetters, def);
}

void PyObj_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pyobj_methods == NULL) pyobj_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pyobj_methods, def);
}

PyMODINIT_FUNC
init_PyObj(void) {
    PyObject* m;

    // getters and setters
    PyObj_addGetSetter("contents", PyObj_getcontents, NULL,
      "A list of other objects contained within this one. Immutable.\n"
      "See obj.Obj.container for changing an object's container.");
    PyObj_addGetSetter("objs", PyObj_getcontents, NULL,
      "Alias for obj.Obj.contents");
    PyObj_addGetSetter("chars", PyObj_getchars, NULL,
      "A list of characters currently sitting/riding this object. Immutable.\n"
      "See char.Char.furniture for changing a character's furniture.");
    PyObj_addGetSetter("name", PyObj_getname, PyObj_setname,
      "The object's name e.g., a longsword");
    PyObj_addGetSetter("mname", PyObj_getmname, PyObj_setmname,
      "The object's name for describing packs, e.g.,\n"
      "a stack of 12 linen towels. The number should be replaced by %d, or\n"
      "not included.");
    PyObj_addGetSetter("desc", PyObj_getdesc, PyObj_setdesc,
      "An object's verbose description e.g., for when it is looked at.");
    PyObj_addGetSetter("rdesc", PyObj_getrdesc, PyObj_setrdesc,
      "The object's description when seen in a room, e.g.,\n"
      "a longsword is here, gleaming in the sun.");
    PyObj_addGetSetter("mdesc", PyObj_getmdesc, PyObj_setmdesc,
      "the equivalent of mname, for room descriptions.");
    PyObj_addGetSetter("keywords", PyObj_getkeywords, PyObj_setkeywords,
      "A comma-separated list of keywords for referencing the object.");
    PyObj_addGetSetter("weight", PyObj_getweight, PyObj_setweight,
      "The object's weight (plus contents). When setting a new value, \n"
      "sets raw weight (minus contents).");
    PyObj_addGetSetter("weight_raw", PyObj_get_weight_raw, PyObj_setweight,
      "The object's weight (minus contents)");
    PyObj_addGetSetter("uid", PyObj_getuid, NULL,
      "The object's unique identification number. Immutable.");
    PyObj_addGetSetter("prototypes", PyObj_getprototypes, NULL,
      "A comma-separated list of prototypes this object inherits from. Immutable");
    PyObj_addGetSetter("bits", PyObj_getbits, PyObj_setbits,
      "A comma-separated list of bits currently toggled for this object.");
    PyObj_addGetSetter("carrier", PyObj_getcarrier, PyObj_setcarrier,
      "The character whose inventory this object is currently in, or None.");
    PyObj_addGetSetter("wearer", PyObj_getwearer, NULL,
      "The character who is currently wearing this object, or None.");
    PyObj_addGetSetter("room", PyObj_getroom, PyObj_setroom,
      "The room this object is current in, or None.");
    PyObj_addGetSetter("container", PyObj_getcontainer, PyObj_setcontainer,
      "The container this object is currently in, or None.");
    PyObj_addGetSetter("hidden", PyObj_gethidden, PyObj_sethidden,
      "Integer value representing how hard this object is to see.");
    PyObj_addGetSetter("age", PyObj_getage, NULL,
      "Value is the difference between the object's creation time and the\n"
      "current system time. Immutable.");
    PyObj_addGetSetter("birth", PyObj_getbirth, NULL,
      "Value is the object's creation time (system time). Immutable.");

    // methods
    PyObj_addMethod("attach", PyObj_attach, METH_VARARGS,
      "attach(trigger)\n"
      "\n"
      "Attach a trigger to the object by key name.");
    PyObj_addMethod("detach", PyObj_detach, METH_VARARGS,
      "detach(trigger)\n"
      "\n"
      "Detach a trigger from the object by key name.");
    PyObj_addMethod("isinstance", PyObj_isinstance, METH_VARARGS,
      "isinstance(prototype)\n"
      "\n"
      "returns whether the object inherits from a specified obj prototype.");
    PyObj_addMethod("edesc", PyObj_edesc, METH_VARARGS,
      "edesc(keywords, desc)\n"
      "\n"
      "Create an extra description for the object, accessible via a comma-\n"
      "separated list of keywords.");
    PyObj_addMethod("getAuxiliary", PyObj_get_auxiliary, METH_VARARGS,
      "getAuxiliary(name)\n"
      "\n"
      "Returns object's auxiliary data of the specified name.");
    PyObj_addMethod("aux", PyObj_get_auxiliary, METH_VARARGS,
      "Alias for obj.Obj.getAuxiliary(name)");
    PyObj_addMethod("getvar", PyObj_getvar, METH_VARARGS,
      "getvar(name)\n"
      "\n"
      "Return value of a special variable. Return 0 if no value has been set.");
    PyObj_addMethod("setvar", PyObj_setvar, METH_VARARGS,
      "setvar(name, val)\n"
      "\n"
      "Set value of a special variable for the object. Values must be strings\n"
      "or numbers. This function is intended to allow scripts and triggers to"
      "open-endedly add variables to objects.");
    PyObj_addMethod("hasvar", PyObj_hasvar, METH_VARARGS,
      "hasvar(name)\n"
      "\n"
      "Return True if object has the given special variable. False otherwise.");
    PyObj_addMethod("deletevar", PyObj_deletevar, METH_VARARGS,
      "deletevar(name)\n"
      "\n"
      "Deletes a special variable from an object if they have one by the\n"
      "given name.");
    PyObj_addMethod("delvar", PyObj_deletevar, METH_VARARGS,
      "Alias for obj.Obj.deletevar(name)");
    PyObj_addMethod("fromall", PyObj_fromall, METH_NOARGS,
      "fromall()\n"
      "\n"
      "Remove object from whichever room, character, or container it is in.");
    PyObj_addMethod("store", PyObj_store, METH_NOARGS,
      "store()\n"
      "\n"
      "Return a storage set representing the object.");
    PyObj_addMethod("copy", PyObj_copy, METH_NOARGS,
      "copy()\n"
      "\n"
      "Returns a copy of the object.");
    PyObj_addMethod("do_trigs", py_gen_do_trigs, METH_KEYWORDS,
      "do_trigs(type, ch=None, obj=None, room=None, exit=None, cmd=None,\n"
      "         arg=None, opts=None)\n\n"
      "Run triggers of the specified type on the object. By default, the\n"
      "trigger owner is 'me'. Other variables can be specified. The opts\n"
      "variable can be a dictionary that maps optional variable names to their\n"
      "values.");

    makePyType(&PyObj_Type, pyobj_getsetters, pyobj_methods);
    deleteListWith(pyobj_getsetters, free); pyobj_getsetters = NULL;
    deleteListWith(pyobj_methods,    free); pyobj_methods    = NULL;

    // make sure the obj class is ready to be made
    if (PyType_Ready(&PyObj_Type) < 0)
        return;

    // make the obj module
    m = Py_InitModule3("obj", obj_module_methods,
      "Contains the Python wrapper for game objects. Also contains utilities\n"
      "for listing, storing, and generating objects from prototypes.");

    // make sure the obj module parsed OK
    if (m == NULL)
      return;

    // add the obj class to the obj module
    PyTypeObject *type = &PyObj_Type;
    Py_INCREF(&PyObj_Type);
    PyModule_AddObject(m, "Obj", (PyObject *)type);
}


int PyObj_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PyObj_Type);
}

int PyObj_AsUid(PyObject *obj) {
  return ((PyObj *)obj)->uid;
}

OBJ_DATA *PyObj_AsObj(PyObject *obj) {
  return propertyTableGet(obj_table, PyObj_AsUid(obj));
}

PyObject *
newPyObj(OBJ_DATA *obj) {
  PyObj *py_obj = (PyObj *)PyObj_new(&PyObj_Type, NULL, NULL);
  py_obj->uid = objGetUID(obj);
  return (PyObject *)py_obj;
}
