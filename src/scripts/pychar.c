//*****************************************************************************
//
// py_char.c
//
// A python extention to allow python scripts to treat MUD characters as an
// object within the script.
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../world.h"
#include "../room.h"
#include "../character.h"
#include "../body.h"
#include "../object.h"
#include "../races.h"
#include "../handler.h"
#include "../utils.h"
#include "../action.h"
#include "../socket.h"
#include "../prototype.h"
#include "../save.h"
#include "../inform.h"

#include "pyplugs.h"
#include "scripts.h"
#include "pychar.h"
#include "pyroom.h"
#include "pyobj.h"
#include "pyexit.h"
#include "pyaccount.h"
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
// a list of the get/setters on the Char class
LIST *pychar_getsetters = NULL;

// a list of the methods on the Char class
LIST *pychar_methods = NULL;

typedef struct {
  PyObject_HEAD
  int uid;
} PyChar;



//*****************************************************************************
// allocation, deallocation, initialization, and comparison
//*****************************************************************************
void PyChar_dealloc(PyChar *self) {
  self->ob_type->tp_free((PyObject*)self);
}

PyObject *PyChar_new(PyTypeObject *type, PyObject *args, PyObject *kwds){
    PyChar *self;
    self = (PyChar  *)type->tp_alloc(type, 0);
    self->uid = NOBODY;
    return (PyObject *)self;
}

int PyChar_init(PyChar *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[] = {"uid", NULL};
  int uid = NOBODY;

  // get the universal id
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &uid)) {
    PyErr_Format(PyExc_TypeError, 
                    "Characters may only be created by uid");
    return -1;
  }

  // make sure a character with the UID exists
  if(!propertyTableGet(mob_table, uid)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character with uid, %d, does not exist", uid);
    return -1;
  }

  self->uid = uid;
  return 0;
}


int PyChar_compare(PyChar *ch1, PyChar *ch2) {
  if(ch1->uid == ch2->uid)
    return 0;
  else if(ch1->uid < ch2->uid)
    return -1;
  else
    return 1;
}

long PyChar_Hash(PyChar *ch) {
  return ch->uid;
}



//*****************************************************************************
// getters and setters for the Char class
//*****************************************************************************
PyObject *PyChar_getname(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", charGetName(ch));
  else           return NULL;
}

PyObject *PyChar_getkeywords(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", charGetKeywords(ch));
  else           return NULL;
}

PyObject *PyChar_getmname(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", charGetMultiName(ch));
  else           return NULL;
}

PyObject *PyChar_getdesc(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", charGetDesc(ch));
  else           return NULL;
}

PyObject *PyChar_getlookbuf(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", bufferString(charGetLookBuffer(ch)));
  else           return NULL;
}  

PyObject *PyChar_getrdesc(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", charGetRdesc(ch));
  else           return NULL;
}

PyObject *PyChar_getmdesc(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", charGetMultiRdesc(ch));
  else           return NULL;
}

PyObject *PyChar_getrace(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", charGetRace(ch));
  else           return NULL;
}

PyObject *PyChar_getsex(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", sexGetName(charGetSex(ch)));
  else           return NULL;
}

PyObject *PyChar_getposition(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", posGetName(charGetPos(ch)));
  else           return NULL;
}

PyObject *PyChar_getroom(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL)
    return NULL;
  else if(charGetRoom(ch) != NULL)
    return Py_BuildValue("O", roomGetPyFormBorrowed(charGetRoom(ch)));
  else {
    return Py_BuildValue("");
  }
}

PyObject *PyChar_getlastroom(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL)
    return NULL;
  else if(charGetLastRoom(ch) != NULL)
    return Py_BuildValue("O", roomGetPyFormBorrowed(charGetLastRoom(ch)));
  else {
    return Py_BuildValue("");
  }
}

PyObject *PyChar_getisnpc(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("i", charIsNPC(ch));
  else           return NULL;
}

PyObject *PyChar_getispc(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("i", !charIsNPC(ch));
  else           return NULL;
}

PyObject *PyChar_gethisher(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", HISHER(ch));
  else           return NULL;
}

PyObject *PyChar_gethimher(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", HIMHER(ch));
  else           return NULL;
}

PyObject *PyChar_getheshe(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", HESHE(ch));
  else           return NULL;
}

PyObject *PyChar_geton(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL) 
    return NULL;
  else if(charGetFurniture(ch) == NULL)
    return Py_BuildValue("");
  else 
    return Py_BuildValue("O", objGetPyFormBorrowed(charGetFurniture(ch)));
}

PyObject *PyChar_getuid(PyChar *self, void *closure) {
  return Py_BuildValue("i", self->uid);
}

PyObject *PyChar_gethidden(PyObject *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar(self);
  if(ch != NULL)  return Py_BuildValue("i", charGetHidden(ch));
  else            return NULL;  
}

PyObject *PyChar_getweight(PyObject *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar(self);
  if(ch != NULL)  return Py_BuildValue("d", charGetWeight(ch));
  else            return NULL;  
}

PyObject *PyChar_getbirth(PyObject *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar(self);
  if(ch != NULL)  return Py_BuildValue("i", charGetBirth(ch));
  else            return NULL;
}

PyObject *PyChar_getage(PyObject *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar(self);
  if(ch != NULL)  return Py_BuildValue("d", difftime(current_time, charGetBirth(ch)));
  else            return NULL;  
}

PyObject *PyChar_getprototypes(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", charGetPrototypes(ch));
  else           return NULL;
}

PyObject *PyChar_getclass(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) return Py_BuildValue("s", charGetClass(ch));
  else           return NULL;
}

PyObject *PyChar_getinv(PyChar *self, PyObject *args) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL) 
    return NULL;

  LIST_ITERATOR *inv_i = newListIterator(charGetInventory(ch));
  PyObject *list = PyList_New(0);
  OBJ_DATA *obj;
  
  // for each obj in the inventory, add it to the Python list
  ITERATE_LIST(obj, inv_i)
    PyList_Append(list, objGetPyFormBorrowed(obj));
  deleteListIterator(inv_i);
  PyObject *retval = Py_BuildValue("O", list);
  Py_DECREF(list);
  return retval;
}

PyObject *PyChar_geteq(PyChar *self, PyObject *args) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL)
    return NULL;

  PyObject      *list = PyList_New(0);
  LIST      *equipped = bodyGetAllEq(charGetBody(ch));
  LIST_ITERATOR *eq_i = newListIterator(equipped);
  OBJ_DATA        *eq = NULL;
  ITERATE_LIST(eq, eq_i) {
    PyList_Append(list, objGetPyFormBorrowed(eq));
  } deleteListIterator(eq_i);
  PyObject *retval = Py_BuildValue("O", list);
  Py_DECREF(list);
  deleteList(equipped);
  return retval;
}

PyObject *PyChar_getbodyparts(PyChar *self, PyObject *args) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL) 
    return NULL;

  PyObject *list = PyList_New(0);
  int i, num_bodyparts;
  const char **bodyparts = bodyGetParts(charGetBody(ch), TRUE, &num_bodyparts);
  for(i = 0; i < num_bodyparts; i++) {
    PyObject *str = Py_BuildValue("s", bodyparts[i]);
    PyList_Append(list, str);
    Py_DECREF(str);
  }
  free(bodyparts);
  return list;
}

PyObject *PyChar_getusergroups(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) 
    return Py_BuildValue("s", bitvectorGetBits(charGetUserGroups(ch)));
  else           
    return NULL;
}

PyObject *PyChar_getsocket(PyChar *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL)
    return NULL;
  else {
    SOCKET_DATA *sock = charGetSocket(ch);
    if(sock == NULL)
      return Py_BuildValue("");
    return Py_BuildValue("O", socketGetPyFormBorrowed(sock));
  }
}



//
// Standard check to make sure the character exists when
// trying to set a value for it. If successful, assign the
// character to ch. Otherwise, return -1 (error)
#define PYCHAR_CHECK_CHAR_EXISTS(uid, ch)                                      \
  ch = propertyTableGet(mob_table, uid);                                       \
  if(ch == NULL) {                                                             \
    PyErr_Format(PyExc_TypeError,                                              \
		    "Tried to modify nonexistant character, %d", uid);         \
    return -1;                                                                 \
  }                                                                            


int PyChar_setname(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete character's name");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Character names must be strings");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetName(ch, PyString_AsString(value));
  return 0;
}

int PyChar_setkeywords(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete character's keywords");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Character keywords must be strings");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);

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
  charSetKeywords(ch, bufferString(new_kwds));

  // garbage collection
  deleteListWith(kwds, free);
  deleteBuffer(new_kwds);
  return 0;
}

int PyChar_setmname(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete character's multi-name");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Character multi-names must be strings");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetMultiName(ch, PyString_AsString(value));
  return 0;
}

int PyChar_setdesc(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete character's description");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Character descriptions must be strings");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetDesc(ch, PyString_AsString(value));
  return 0;
}

int PyChar_sethidden(PyObject *self, PyObject *value, void *closure) {
  CHAR_DATA *ch = NULL;
  PYCHAR_CHECK_CHAR_EXISTS(PyChar_AsUid(self), ch);

  if(value == NULL || value == Py_None)
    charSetHidden(ch, 0);
  else if(PyInt_Check(value))
    charSetHidden(ch, PyInt_AsLong(value));
  else {
    PyErr_Format(PyExc_TypeError,
		"Tried to change char %d's spot difficulty to an invalid type.",
		 charGetUID(ch));
    return -1;
  }

  return 0;
}

int PyChar_setweight(PyObject *self, PyObject *value, void *closure) {
  CHAR_DATA *ch = NULL;
  PYCHAR_CHECK_CHAR_EXISTS(PyChar_AsUid(self), ch);

  if(value == NULL || value == Py_None)
    charSetWeight(ch, 0.0);
  else if(PyFloat_Check(value))
    charSetWeight(ch, PyFloat_AsDouble(value));
  else if(PyInt_Check(value))
    charSetWeight(ch, PyInt_AsLong(value));
  else {
    PyErr_Format(PyExc_TypeError,
		"Tried to change char %d's weight to an invalid type.",
		 charGetUID(ch));
    return -1;
  }

  return 0;
}

int PyChar_setlookbuf(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete character's look buffer");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Look bufers must be strings");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  bufferClear(charGetLookBuffer(ch));
  bufferCat(charGetLookBuffer(ch), PyString_AsString(value));
  return 0;
}

int PyChar_setrdesc(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete character's rdesc");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Character rdescs must be strings");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetRdesc(ch, PyString_AsString(value));
  return 0;
}

int PyChar_setmdesc(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete character's multi-rdesc");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Character multi-rdescs must be strings");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetMultiRdesc(ch, PyString_AsString(value));
  return 0;
}

int PyChar_setrace(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's race");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Character races must be strings");
    return -1;
  }

  const char *race = PyString_AsString(value);
  if(!isRace(race))
    return -1;

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetRace(ch, race);
  charResetBody(ch);
  return 0;
}

int PyChar_seton(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's furniture.");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);

  if (value == Py_None) {
    char_from_furniture(ch);
    return 0;
  }
  else if(PyObj_Check(value)) {
    OBJ_DATA *obj = propertyTableGet(obj_table, PyObj_AsUid(value));
    if(obj == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "Tried to %s's furniture to a nonexistant object.",
		   charGetName(ch));
      return -1;
    }
    else if(objIsType(obj, "furniture")) {
      if(charGetFurniture(ch))
	char_from_furniture(ch);
      char_to_furniture(ch, obj);
      return 0;
    }
  }

  PyErr_Format(PyExc_TypeError, 
	       "A Character's furniture may only be set to None or a ."
	       "furniture object.");
  return -1;
}

int PyChar_setsex(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's sex");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character sexes must be strings");
    return -1;
  }

  int sex = sexGetNum(PyString_AsString(value));
  if(sex == SEX_NONE) {
    PyErr_Format(PyExc_TypeError, "%s is an invalid sex type",
		 PyString_AsString(value));
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetSex(ch, sex);
  return 0;
}

int PyChar_setposition(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's position");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character positions must be strings");
    return -1;
  }

  int pos = posGetNum(PyString_AsString(value));
  if(pos == POS_NONE) {
    PyErr_Format(PyExc_TypeError, "%s is an invalid position type", 
		 PyString_AsString(value));
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);
  charSetPos(ch, pos);
  // players can't be on furniture if they are standing or flying
  if(poscmp(charGetPos(ch), POS_STANDING) >= 0 && charGetFurniture(ch))
    char_from_furniture(ch);
  return 0;
}

int PyChar_setroom(PyChar *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete a character's room");
    return -1;
  }

  CHAR_DATA *ch;
  PYCHAR_CHECK_CHAR_EXISTS(self->uid, ch);

  ROOM_DATA *room = NULL;

  if(PyRoom_Check(value))
    room = PyRoom_AsRoom(value);
  else if(PyString_Check(value))
    room = worldGetRoom(gameworld, 
			get_fullkey_relative(PyString_AsString(value),
					     get_smart_locale(ch)));
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Character's room must be a string value or a "
		 "room object.");
    return -1;
  }

  if(room == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Attempting to move character to nonexistent room.");
    return -1;
  }

  // only move if we're not already here
  if(charGetRoom(ch) != room) {
    char_to_room(ch, room);

    // if we were on furniture, make sure we dismount it
    if(charGetFurniture(ch))
      char_from_furniture(ch);
  }

  return 0;
}



//*****************************************************************************
// methods for the Char class
//*****************************************************************************

//
// pages a length of text to the character
PyObject *PyChar_page(PyChar *self, PyObject *value) {
  char *mssg = NULL;
  if (!PyArg_ParseTuple(value, "s", &mssg)) {
    PyErr_Format(PyExc_TypeError, "Characters may only be paged strings");
    return NULL;
  }

  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) {
    if(charGetSocket(ch))
      page_string(charGetSocket(ch), mssg);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
                    "Tried to page message to nonexistant character, %d.", 
		    self->uid);
    return NULL;
  }
}

//
// sends text to the character
PyObject *PyChar_send_raw(PyChar *self, PyObject *value) {
  char *mssg = NULL;
  if (!PyArg_ParseTuple(value, "s", &mssg)) {
    PyErr_Format(PyExc_TypeError, 
                    "Characters may only be sent strings");
    return NULL;
  }

  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch) {
    text_to_char(ch, mssg);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
                    "Tried to send message to nonexistant character, %d.", 
		    self->uid);
    return NULL;
  }
}

//
// Send a message with Python statements potentially embedded in it. For 
//evaluating 
PyObject *PyChar_send(PyObject *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[ ] = { "mssg", "dict", "newline", NULL };
  CHAR_DATA  *me = NULL;
  char     *text = NULL;
  PyObject *dict = NULL;
  bool   newline = TRUE;

  if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|Ob", kwlist, 
				  &text, &dict, &newline)) {
    PyErr_Format(PyExc_TypeError, "Invalid arguments supplied to Char.send");
    return NULL;
  }

  // is dict None? set it to NULL for expand_to_char
  if(dict == Py_None)
    dict = NULL;

  // make sure the dictionary is a dictionary
  if(!(dict == NULL || PyDict_Check(dict))) {
    PyErr_Format(PyExc_TypeError, "Char.send expects second argument to be a dict object.");
    return NULL;
  }
  
  // make sure we exist
  if( (me = PyChar_AsChar(self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to send nonexistant character.");
    return NULL;
  }
  else if(charGetSocket(me) == NULL) {
    // no point
    return Py_BuildValue("");
  }

  if(dict != NULL)
    PyDict_SetItemString(dict, "me", self);
  expand_to_char(me, text, dict, get_script_locale(), newline);
  return Py_BuildValue("");
}

//
// Send a newline-tagged message to everyone around the character
PyObject *PyChar_sendaround(PyObject *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[ ] = { "mssg", "dict", "cansee_only", "newline", NULL };
  CHAR_DATA    *me = NULL;
  char       *text = NULL;
  PyObject   *dict = NULL;
  bool cansee_only = FALSE;
  bool     newline = TRUE;

  if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|Obb", kwlist, 
				  &text, &dict, &cansee_only, &newline)) {
    PyErr_Format(PyExc_TypeError, "Invalid arguments supplied to Char.sendaround");
    return NULL;
  }

  // is dict None? set it to NULL for expand_to_char
  if(dict == Py_None)
    dict = NULL;

  // make sure the dictionary is a dictionary
  if(!(dict == NULL || PyDict_Check(dict))) {
    PyErr_Format(PyExc_TypeError, "Char.sendaround expects second argument to be a dict object.");
    return NULL;
  }
  
  // make sure we exist
  if( (me = PyChar_AsChar(self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to sendaround nonexistent character.");
    return NULL;
  }
  // make sure we have a room
  else if(charGetRoom(me) == NULL) {
    // no point
    return Py_BuildValue("");
  }

  if(dict != NULL)
    PyDict_SetItemString(dict, "me", self);

  // go through all of the characters in our room and send out messages
  LIST_ITERATOR *char_i = newListIterator(roomGetCharacters(charGetRoom(me)));
  CHAR_DATA         *ch = NULL;
  ITERATE_LIST(ch, char_i) {
    // it's us, or a linkdead character. Ignore
    if(me == ch || charGetSocket(ch) == NULL)
      continue;
    // can't see and need to see
    else if(cansee_only && !can_see_char(ch, me))
      continue;
    PyDict_SetItemString(dict, "ch", charGetPyFormBorrowed(ch));
    expand_to_char(ch, text, dict, get_script_locale(), newline);
  } deleteListIterator(char_i);
  return Py_BuildValue("");
}

//
// make the character perform an action
PyObject *PyChar_act(PyChar *self, PyObject *value) {
  char     *act = NULL;
  bool alias_ok = FALSE; 
  if (!PyArg_ParseTuple(value, "s|b", &act, &alias_ok)) {
    PyErr_Format(PyExc_TypeError, "Characters actions must be strings.");
    return NULL;
  }

  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);

  // we don't exist
  if(ch == NULL) {
    PyErr_Format(PyExc_TypeError, 
                    "Nonexistant character, %d, tried to perform an action.", 
		    self->uid);
    return NULL;
  }
  // It's not safe to act if we don't have a room to act in yet
  else if(charGetRoom(ch) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Character, %d, tried to act without first having a room "
		 "to act in.", self->uid);
    return NULL;
  }
  else {
    // do not send the actual act - if we edit it, things go awry
    char *working_act = strdupsafe(act);
    do_cmd(ch, working_act, alias_ok);
    free(working_act);
    return Py_BuildValue("i", 1);
  }
}

//
// returns whether or not the character can see something
PyObject *PyChar_cansee(PyChar *self, PyObject *arg) {
  PyObject *py_tgt = NULL;

  if(!PyArg_ParseTuple(arg, "O", &py_tgt)) {
    PyErr_Format(PyExc_TypeError, "Must supply obj, mob, or exit for cansee");
    return NULL;
  }

  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);  
  if(ch == NULL) {
    PyErr_Format(PyExc_TypeError, "Nonexistent character, %d, tried cansee",
		 self->uid);
    return NULL;
  }
  else {
    OBJ_DATA    *obj = NULL;
    CHAR_DATA  *pers = NULL;
    EXIT_DATA    *ex = NULL;

    if(PyChar_Check(py_tgt))
      pers = PyChar_AsChar(py_tgt);
    else if(PyObj_Check(py_tgt))
      obj  = PyObj_AsObj(py_tgt);
    else if(PyExit_Check(py_tgt))
      ex   = PyExit_AsExit(py_tgt);
    else {
      PyErr_Format(PyExc_TypeError, "Must supply obj, mob, or exit to cansee");
      return NULL;
    }

    if(obj != NULL)
      return Py_BuildValue("b", can_see_obj(ch, obj));
    else if(pers != NULL)
      return Py_BuildValue("b", can_see_char(ch, pers));
    else if(ex != NULL)
      return Py_BuildValue("b", can_see_exit(ch, ex));
    else {
      PyErr_Format(PyExc_StandardError, "Target of cansee did not exist!");
      return NULL;
    }
  }
}

//
// returns whether or not the character can see something
PyObject *PyChar_see_as(PyChar *self, PyObject *arg) {
  PyObject *py_tgt = NULL;

  if(!PyArg_ParseTuple(arg, "O", &py_tgt)) {
    PyErr_Format(PyExc_TypeError, "Must supply obj or mob for see_as");
    return NULL;
  }

  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);  
  if(ch == NULL) {
    PyErr_Format(PyExc_TypeError, "Nonexistent character, %d, tried see_as",
		 self->uid);
    return NULL;
  }
  else {
    OBJ_DATA    *obj = NULL;
    CHAR_DATA  *pers = NULL;
    EXIT_DATA  *exit = NULL;

    if(PyChar_Check(py_tgt))
      pers = PyChar_AsChar(py_tgt);
    else if(PyObj_Check(py_tgt))
      obj  = PyObj_AsObj(py_tgt);
    else if(PyExit_Check(py_tgt))
      exit = PyExit_AsExit(py_tgt);
    else {
      PyErr_Format(PyExc_TypeError, "Must supply obj, mob, or exit to see_as");
      return NULL;
    }

    if(obj != NULL)
      return Py_BuildValue("s", see_obj_as(ch, obj));
    else if(pers != NULL)
      return Py_BuildValue("s", see_char_as(ch, pers));
    else if(exit != NULL)
      return Py_BuildValue("s", see_exit_as(ch, exit));
    else {
      PyErr_Format(PyExc_StandardError, "Target of cansee did not exist!");
      return NULL;
    }
  }
}

//
// Returns TRUE if the character has the given variable set
PyObject *PyChar_hasvar(PyChar *self, PyObject *arg) {
  char *var = NULL;
  if (!PyArg_ParseTuple(arg, "s", &var)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character variables must have string names.");
    return NULL;
  }

  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL)
    return Py_BuildValue("b", charHasVar(ch, var));

  PyErr_Format(PyExc_TypeError, 
	       "Tried to get a variable value for nonexistant character, %d",
	       self->uid);
  return NULL;
}


//
// Delete the variable set on the character with the specified name
PyObject *PyChar_deletevar(PyChar *self, PyObject *arg) {
  char *var = NULL;
  if (!PyArg_ParseTuple(arg, "s", &var)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character variables must have string names.");
    return NULL;
  }

  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) {
    charDeleteVar(ch, var);
    return Py_BuildValue("i", 1);
  }

  PyErr_Format(PyExc_TypeError, 
	       "Tried to get a variable value for nonexistant character, %d",
	       self->uid);
  return NULL;
}


//
// Get the value of a variable stored on the character
PyObject *PyChar_getvar(PyChar *self, PyObject *arg) {
  char *var = NULL;
  if (!PyArg_ParseTuple(arg, "s", &var)) {
    PyErr_Format(PyExc_TypeError, 
                    "Character variables must have string names.");
    return NULL;
  }

  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) {
    int vartype = charGetVarType(ch, var);
    if(vartype == DYN_VAR_INT)
      return Py_BuildValue("i", charGetInt(ch, var));
    else if(vartype == DYN_VAR_LONG)
      return Py_BuildValue("i", charGetLong(ch, var));
    else if(vartype == DYN_VAR_DOUBLE)
      return Py_BuildValue("d", charGetDouble(ch, var));
    else
      return Py_BuildValue("s", charGetString(ch, var));
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to get a variable value for nonexistant character, %d",
		 self->uid);
    return NULL;
  }
}


//
// Set the value of a variable assocciated with the character
PyObject *PyChar_setvar(PyChar *self, PyObject *args) {  
  char     *var = NULL;
  PyObject *val = NULL;

  if (!PyArg_ParseTuple(args, "sO", &var, &val)) {
    PyErr_Format(PyExc_TypeError, 
		 "Character setvar must be supplied with a var name and integer value.");
    return NULL;
  }

  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) {
    if(PyInt_Check(val))
      charSetInt(ch, var, (int)PyInt_AsLong(val));
    else if(PyFloat_Check(val))
      charSetDouble(ch, var, PyFloat_AsDouble(val));
    else if(PyString_Check(val))
      charSetString(ch, var, PyString_AsString(val));
    else {
      PyErr_Format(PyExc_TypeError,
		   "Tried to store a char_var of invalid type on char %d.",
		   self->uid);
      return NULL;
    }
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to set a variable value for nonexistant character, %d",
		 self->uid);
    return NULL;
  }
}

PyObject *PyChar_getbodypct(PyChar *self, PyObject *args) {
  char   *parts = NULL;
  CHAR_DATA *ch = NULL;

  if (!PyArg_ParseTuple(args, "s", &parts)) {
    PyErr_Format(PyExc_TypeError,"A comma-separated list of body parts needed");
    return NULL;
  }

  ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to query body info for nonexistant character!");
    return NULL;
  }

  return Py_BuildValue("d", bodyPartRatio(charGetBody(ch), parts));
}


//
// equips a character with an item
PyObject *PyChar_equip(PyChar *self, PyObject *args) {  
  OBJ_DATA      *obj = NULL;
  CHAR_DATA      *ch = NULL;
  PyObject     *pobj = NULL;
  char          *pos = NULL;
  const char *needed = NULL;
  bool        forced = FALSE;

  // incase the equip fails, keep item in the original place.. here's the vars
  CHAR_DATA *old_carrier = NULL;
  CHAR_DATA  *old_wearer = NULL;
  const char    *old_pos = NULL;
  ROOM_DATA    *old_room = NULL;
  OBJ_DATA     *old_cont = NULL;

  if (!PyArg_ParseTuple(args, "O|zb", &pobj, &pos, &forced)) {
    PyErr_Format(PyExc_TypeError, 
		 "Character equip must be supplied with an item to equip!");
    return NULL;
  }

  if(!PyObj_Check(pobj)) {
    PyErr_Format(PyExc_TypeError,
		 "Only objects may be equipped to characters!");
    return NULL;
  }

  ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to equip nonexistant character!");
    return NULL;
  }

  obj = PyObj_AsObj(pobj);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to equip character with existant object!");
    return NULL;
  }

  // remove the object from whatever it's in/on currently
  if((old_room = objGetRoom(obj)) != NULL)
    obj_from_room(obj);
  if((old_cont = objGetContainer(obj)) != NULL)
    obj_from_obj(obj);
  if((old_carrier = objGetCarrier(obj)) != NULL)
    obj_from_char(obj);
  if((old_wearer = objGetWearer(obj)) != NULL) {
    old_pos = bodyEquippedWhere(charGetBody(old_wearer), obj);
    try_unequip(old_wearer, obj);
  }

  if(objIsType(obj, "worn"))
    needed = wornGetPositions(obj);

  // try equipping the object. If we fail, put it back wherever it came from
  if((!forced && !objIsType(obj, "worn")) || !try_equip(ch,obj,pos,needed)) {
    if(old_room != NULL)
      obj_to_room(obj, old_room);
    else if(old_cont != NULL)
      obj_to_obj(obj, old_cont);
    else if(old_carrier != NULL)
      obj_to_char(obj, old_carrier);
    else if(old_wearer != NULL)
      try_equip(ch, obj, old_pos, NULL);
    if(pos == NULL)
      message(ch, NULL, obj, NULL, TRUE, TO_CHAR, "You are already equipped in all possible positions for $o.");
    else
      message(ch, NULL, obj, NULL, TRUE, TO_CHAR, "You could not equip $o there.");

    return Py_BuildValue("i", 0);
    //    PyErr_Format(PyExc_StandardError,
    //		 "Character is already equipped in all possible positions!");
    //    return NULL;
  }
  // success
  else 
    return Py_BuildValue("i", 1);
}


PyObject *PyChar_getequip(PyChar *self, PyObject *args) {  
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  OBJ_DATA *obj = NULL;
  char     *pos = NULL;
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError, "Nonexistant character");
    return NULL;
  }

  if(!PyArg_ParseTuple(args, "s", &pos)) {
    PyErr_Format(PyExc_TypeError, "A position name must be supplied.");
    return NULL;
  }
  
  obj = bodyGetEquipment(charGetBody(ch), pos);
  if(obj == NULL)
    return Py_BuildValue("");
  else
    return Py_BuildValue("O", objGetPyFormBorrowed(obj));
}

PyObject *PyChar_getslots(PyChar *self, PyObject *args) {  
  CHAR_DATA   *ch = PyChar_AsChar((PyObject *)self);
  PyObject *pyobj = NULL;
  OBJ_DATA   *obj = NULL;
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError, "Nonexistant character");
    return NULL;
  }

  if(!PyArg_ParseTuple(args, "O", &pyobj)) {
    PyErr_Format(PyExc_TypeError, "An item must be supplied.");
    return NULL;
  }
  if(!PyObj_Check(pyobj)) {
    PyErr_Format(PyExc_TypeError, "Missing object argument type.");
    return NULL;
  }
  obj = PyObj_AsObj(pyobj);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried locate positions of nonexistent object, %d.", PyObj_AsUid(pyobj));
    return NULL;
  }

  // return the slots
  return Py_BuildValue("s", bodyEquippedWhere(charGetBody(ch), obj));
}

PyObject *PyChar_getslottypes(PyChar *self, PyObject *args) {  
  CHAR_DATA   *ch = PyChar_AsChar((PyObject *)self);
  PyObject *pyobj = NULL;
  OBJ_DATA   *obj = NULL;
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError, "Nonexistant character");
    return NULL;
  }

  if(!PyArg_ParseTuple(args, "O", &pyobj)) {
    PyErr_Format(PyExc_TypeError, "An item must be supplied.");
    return NULL;
  }
  if(!PyObj_Check(pyobj)) {
    PyErr_Format(PyExc_TypeError, "Missing object argument type.");
    return NULL;
  }
  obj = PyObj_AsObj(pyobj);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried locate positions of nonexistent object, %d.", PyObj_AsUid(pyobj));
    return NULL;
  }

  PyObject *ret = PyList_New(0);
  LIST   *where = parse_keywords(bodyEquippedWhere(charGetBody(ch), obj));
  if(where > 0) {
    LIST_ITERATOR *where_i = newListIterator(where);
    const char        *pos = NULL;
    ITERATE_LIST(pos, where_i) {
      PyObject *str = Py_BuildValue("s", bodyposGetName(bodyGetPart(charGetBody(ch), pos)));
      PyList_Append(ret, str);
      Py_DECREF(str);
    } deleteListIterator(where_i);
  }
  deleteListWith(where, free);
  return ret;
}

PyObject *PyChar_attach(PyChar *self, PyObject *args) {  
  char *key = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "To attach a trigger, the trigger key must be suppplied.");
    return NULL;
  }

  // pull out the character and do the attaching
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach trigger to nonexistant char, %d.", self->uid);
    return NULL;
  }

  TRIGGER_DATA *trig = 
    worldGetType(gameworld, "trigger", 
		 get_fullkey_relative(key, get_script_locale()));
  if(trig != NULL) {
    triggerListAdd(charGetTriggers(ch), triggerGetKey(trig));
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to attach nonexistant trigger, %s, to character %s.",
		 key, charGetClass(ch));
    return NULL;
  }
}


PyObject *PyChar_detach(PyChar *self, PyObject *args) {  
  char *key = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "To detach a trigger, the key must be supplied.");
    return NULL;
  }

  // pull out the character and do the attaching
  CHAR_DATA    *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL) {
    const char *fkey = get_fullkey_relative(key, get_script_locale());
    triggerListRemove(charGetTriggers(ch), fkey);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_StandardError, 
		"Tried to detach trigger from nonexistant char, %d.",self->uid);
    return NULL;
  }
}


//
// handles the completion of an action queued up by Python
void PyAction_on_complete(CHAR_DATA *ch, PyObject *tuple, bitvector_t where,
			  const char *arg) {
  PyObject *pychar = NULL; // the python representation of our ch
  PyObject  *cfunc = NULL; // the function called on completion
  PyObject  *ifunc = NULL; // the function called on interruption
  PyObject   *data = NULL; // the data we need to send back

  // only run the action of our arguments parse properly
  if(PyArg_ParseTuple(tuple, "OOOO", &pychar, &data, &cfunc, &ifunc)) {
    if(cfunc != Py_None) {
      PyObject *ret = PyObject_CallFunction(cfunc, "OOs", pychar, data, arg);
      Py_XDECREF(ret);
    }
  }
  
  Py_DECREF(tuple);
}


//
// handles the interruption of an action queued up by Python
void PyAction_on_interrupt(CHAR_DATA *ch, PyObject *tuple, bitvector_t where,
			   const char *arg) {
  PyObject *pychar = NULL; // the python representation of our ch
  PyObject  *cfunc = NULL; // the function called on completion
  PyObject  *ifunc = NULL; // the function called on interruption
  PyObject   *data = NULL; // the data we need to send back

  // only run the action of our arguments parse properly
  if(PyArg_ParseTuple(tuple, "OOOO", &pychar, &data, &cfunc, &ifunc)) {
    if(ifunc != Py_None) {
      PyObject *ret = PyObject_CallFunction(ifunc, "OOs", pychar, data, arg);
      Py_XDECREF(ret);
    }
  }
  
  Py_DECREF(tuple);
}


//
// start a new action (and interrupt old ones)
PyObject *PyChar_start_action(PyChar *self, PyObject *args) {  
  CHAR_DATA          *ch = NULL;    // our normal character representation
  PyObject  *on_complete = Py_None; // func called when action is completed
  PyObject *on_interrupt = Py_None; // func called when action is interrupted
  PyObject         *data = Py_None; // the function's data value
  double           delay = 0;       // the delay of the action (seconds)
  char              *arg = NULL;    // the action's string argument

  // parse all of our values
  if(!PyArg_ParseTuple(args, "dO|OOz", &delay,  &on_complete, &on_interrupt,
		      &data, &arg)) {
    PyErr_Format(PyExc_TypeError,
		 "startAction supplied with invalid arguments!");
    return NULL;
  }

  // make sure we exist
  if((ch = PyChar_AsChar((PyObject *)self)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to start action for nonexistant character!");
    return NULL;
  }

  // now, queue up the action
  start_action(ch, (int)(delay SECONDS), 1, 
	       PyAction_on_complete, PyAction_on_interrupt, 
	       Py_BuildValue("OOOO", self, data, on_complete, on_interrupt),
	       arg);

  // success!
  return Py_BuildValue("i", 1);
}


//
// check to see if a character currently has an action in progress
PyObject *PyChar_is_acting(PyChar *self, void *closure) {  
  // make sure we exist
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);

  // make sure the character exists
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to query action status for a nonexistant character.");
    return NULL;
  }

  // return our value
  return Py_BuildValue("i", is_acting(ch, 1));
}


//
// interrupt any actions the character is currently performing
PyObject *PyChar_interrupt_action(PyChar *self, void *closure) {
  // make sure we exist
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);

  // make sure the character exists
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to interrupt actions for a nonexistant character.");
    return NULL;
  }

  interrupt_action(ch, 1);
  return Py_BuildValue("i", 1);
}


//
// returns the specified piece of auxiliary data from the character
// if it is a piece of python auxiliary data.
PyObject *PyChar_get_auxiliary(PyChar *self, PyObject *args) {
  char *keyword = NULL;
  if(!PyArg_ParseTuple(args, "s", &keyword)) {
    PyErr_Format(PyExc_TypeError,
		 "getAuxiliary() must be supplied with the name that the "
		 "auxiliary data was installed under!");
    return NULL;
  }

  // make sure we exist
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to get auxiliary data for a nonexistant character.");
    return NULL;
  }

  // make sure the auxiliary data exists
  if(!pyAuxiliaryDataExists(keyword)) {
    PyErr_Format(PyExc_StandardError,
		 "No auxiliary data named '%s' exists!", keyword);
    return NULL;
  }

  PyObject *data = charGetAuxiliaryData(ch, keyword);
  if(data == NULL)
    data = Py_None;
  PyObject *retval = Py_BuildValue("O", data);
  //  Py_DECREF(data);
  return retval;
}


//
// returns whether or not the character is an instance of the prototype
PyObject *PyChar_isinstance(PyChar *self, PyObject *args) {  
  char *type = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &type)) {
    PyErr_Format(PyExc_TypeError, "isinstance only accepts strings.");
    return NULL;
  }

  // pull out the object and check the type
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL)
    return Py_BuildValue("i", 
        charIsInstance(ch, get_fullkey_relative(type, get_script_locale())));
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to check instances of nonexistent char, %d.", self->uid);
    return NULL;
  }
}

PyObject *PyChar_hasPreferences(PyChar *self, PyObject *args) {
  char *prefs = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &prefs)) {
    PyErr_Format(PyExc_TypeError, "hasPrefs only accepts strings.");
    return NULL;
  }

  // pull out the object and check the type
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL)
    return Py_BuildValue("i", bitIsSet(charGetPrfs(ch), prefs));
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to check prefs of nonexistent char, %d.", self->uid);
    return NULL;
  }
}

//
// returns whether or not the character belongs to one of the specified groups
PyObject *PyChar_is_in_groups(PyChar *self, PyObject *args) {  
  char *groups = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &groups)) {
    PyErr_Format(PyExc_TypeError, "is_in_groups only accepts strings.");
    return NULL;
  }

  // pull out the object and check the type
  CHAR_DATA *ch = PyChar_AsChar((PyObject *)self);
  if(ch != NULL)
    return Py_BuildValue("i", bitIsSet(charGetUserGroups(ch), groups));
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to check user groups of nonexistent char, %d.", self->uid);
    return NULL;
  }
}

PyObject *PyChar_append_look(PyObject *self, PyObject *args) {
  char *desc = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &desc)) {
    PyErr_Format(PyExc_TypeError, "Only strings can be appended to the look buffer.");
    return NULL;
  }

  // pull out the object and check the type
  CHAR_DATA *ch = PyChar_AsChar(self);
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to append description to look buffer of nonexistent character, %d.", PyChar_AsUid(self));
    return NULL;
  }
  else {
    bufferCat(charGetLookBuffer(ch), desc);
    return Py_BuildValue("");
  }
}

PyObject *PyChar_clear_look(PyObject *self, void *closure) {
  // pull out the object and check the type
  CHAR_DATA *ch = PyChar_AsChar(self);
  if(ch == NULL) {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to clear look buffer of nonexistent character, %d.", PyChar_AsUid(self));
    return NULL;
  }
  else {
    bufferClear(charGetLookBuffer(ch));
    return Py_BuildValue("i", 1);
  }
}

PyObject *PyChar_store(PyObject *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar(self);
  if(ch == NULL) {
    PyErr_Format(PyExc_TypeError, "failed to store nonexistent character.");
    return NULL;
  }
  return newPyStorageSet(charStore(ch));
}

PyObject *PyChar_copy(PyObject *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar(self);
  if(ch == NULL) {
    PyErr_Format(PyExc_TypeError, "failed to copy nonexistent character.");
    return NULL;
  }
  CHAR_DATA *newch = charCopy(ch);

  // we have to put the object in the global tables and list, 
  // or else Python will not be able to access it
  char_to_game(newch);

  return charGetPyForm(newch);
}



//*****************************************************************************
// comparators, getattr, setattr, and all that other class stuff
//*****************************************************************************
PyTypeObject PyChar_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "char.Char",               /*tp_name*/
    sizeof(PyChar),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyChar_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    (cmpfunc)PyChar_compare,   /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    (hashfunc)PyChar_Hash,     /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Char/Mob objects",        /* tp_doc */
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
    (initproc)PyChar_init,     /* tp_init */
    0,                         /* tp_alloc */
    PyChar_new,                /* tp_new */
};



//*****************************************************************************
// methods in the char module
//*****************************************************************************
PyObject *PyChar_load_mob(PyObject *self, PyObject *args) {
  char      *mob_key = NULL;
  PyObject       *to = NULL;
  ROOM_DATA    *room = NULL;
  OBJ_DATA       *on = NULL;
  char      *posname = NULL;
  
  if (!PyArg_ParseTuple(args, "sO|s", &mob_key, &to, &posname)) {
    PyErr_Format(PyExc_TypeError, 
		 "Load char failed - it needs prototype and destination.");
    return NULL;
  }

  // see what we're trying to load to
  if(PyString_Check(to))
    room = worldGetRoom(gameworld, PyString_AsString(to));
  else if(PyRoom_Check(to))
    room = PyRoom_AsRoom(to);
  else if(PyObj_Check(to))
    on = propertyTableGet(obj_table, PyObj_AsUid(to));
  else {
    PyErr_Format(PyExc_TypeError, 
                    "Load char failed: invalid load-to type.");
    return NULL;
  }

  // see if we're loading onto something
  if(on != NULL)
    room = objGetRoom(on);

  if(room == NULL) {
    PyErr_Format(PyExc_TypeError, "Load char failed: room does not exist, or "
		 "furniture is not. in a room.");
    return NULL;
  }

  // check the mob
  PROTO_DATA *mob_proto = 
    worldGetType(gameworld, "mproto", 
		 get_fullkey_relative(mob_key, get_script_locale()));
  if(mob_proto == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Load char failed: no mproto for %s exists", 
		 get_fullkey_relative(mob_key, get_script_locale()));
    return NULL;
  }

  // copy the mob, and put it into the game
  CHAR_DATA *mob = protoMobRun(mob_proto);
  if(mob == NULL) {
    PyErr_Format(PyExc_TypeError,
		 "Load char failed: proto script terminated with an error.");
    return NULL;
  }

  char_to_room(mob, room);

  // now check if we need to put the char onto some furniture
  // default position is POS_SITTING
  if(on) {
    int pos = POS_SITTING;
    char_to_furniture(mob, on);
    if(posname)
      pos = posGetNum(posname);

    // if the position is none, or greater 
    // than sitting, default to sitting.
    if(pos == POS_NONE || poscmp(pos, POS_SITTING) > 0)
      pos = POS_SITTING;
    charSetPos(mob, pos);
  }
  else if(posname) {
    int pos = posGetNum(posname);
    // if it was an invalid name, set it to standing
    if(pos == POS_NONE)
      pos = POS_STANDING;
    charSetPos(mob, pos);
  }

  // create a python object for the new char, and return it
  return Py_BuildValue("O", charGetPyFormBorrowed(mob));
}

PyObject *PyChar_find_char_key(PyObject *self, PyObject *args) {
  CHAR_DATA     *ch = NULL;
  PyObject    *pych = NULL;
  LIST       *where = mobile_list;
  PyObject *pywhere = NULL;
  char         *key = NULL;
  bool     must_see = TRUE;
  bool     multiple = FALSE;
  ROOM_DATA   *room = NULL;

  // figure out our arguments
  if(!PyArg_ParseTuple(args, "Os|Obb",&pych,&key,&pywhere,&must_see,&multiple)){
    PyErr_Format(PyExc_TypeError, "Invalid arguments supplied to find_char");
    return NULL;
  }

  // make sure ch exists, if we supplied one
  if(pych == Py_None)
    ch = NULL;
  else if(PyChar_Check(pych)) {
    ch = PyChar_AsChar(pych);
    if(ch == NULL) {
      PyErr_Format(PyExc_StandardError, "character does not exist");
      return NULL;
    }
  }
  else {
    PyErr_Format(PyExc_TypeError, "first arg must be a Char, or None");
    return NULL;
  }

  // figoure out our room if we supply one
  if(pywhere != NULL) {
    if(PyRoom_Check(pywhere))
      room = PyRoom_AsRoom(pywhere);
    else if(PyString_Check(pywhere))
      room = worldGetRoom(gameworld, 
			  get_fullkey_relative(PyString_AsString(pywhere),
					       get_script_locale()));
    else if(pywhere == Py_None)
      room = NULL;
    else {
      PyErr_Format(PyExc_TypeError, "search scope must be a room or room key");
      return NULL;
    }
  }

  // if we've got a room, look in it
  if(room != NULL)
    where = roomGetCharacters(room);

  // do the searching for a single thing
  if(multiple == FALSE) {
    CHAR_DATA *found = find_char(ch, where, 1, NULL, 
				 get_fullkey_relative(key, get_script_locale()),
				 must_see);
    return Py_BuildValue("O", (found ? charGetPyFormBorrowed(found) : Py_None));
  }
  // search for multiple occurences
  else {
    LIST *found = find_all_chars(ch, where, NULL,
				 get_fullkey_relative(key, get_script_locale()),
				 must_see);
    PyObject         *list = PyList_New(0);
    LIST_ITERATOR *found_i = newListIterator(found);
    CHAR_DATA   *one_found = NULL;
    ITERATE_LIST(one_found, found_i) {
      PyList_Append(list, charGetPyFormBorrowed(one_found));
    } deleteListIterator(found_i);
    deleteList(found);
    PyObject *retval = Py_BuildValue("O", list);
    Py_DECREF(list);
    return retval;
  }
}

PyObject *PyChar_count_mobs(PyObject *self, PyObject *args) {
  LIST            *list = NULL;
  char             *tgt = NULL;
  PyObject          *in = NULL;
  ROOM_DATA       *room = NULL;
  OBJ_DATA   *furniture = NULL;
  const char *prototype = NULL;

  if (!PyArg_ParseTuple(args, "s|O", &tgt, &in)) {
    PyErr_Format(PyExc_TypeError, 
                    "count_mobs failed. No arguments supplied.");
    return NULL;
  }

  // figure out the full key of our prototype
  prototype = get_fullkey_relative(tgt, get_script_locale());

  // if we didn't supply something to look in, assume it means the world
  if(in == NULL)
    return Py_BuildValue("i", count_chars(NULL, mobile_list, NULL, prototype, 
					  FALSE));

  // see what we're looking in
  if(PyString_Check(in))
    room = worldGetRoom(gameworld, PyString_AsString(in));
  else if(PyRoom_Check(in))
    room = PyRoom_AsRoom(in);
  else if(PyObj_Check(in))
    furniture = propertyTableGet(obj_table, PyObj_AsUid(in));

  // now find the list we're dealing with
  if(room)      list = roomGetCharacters(room);
  if(furniture) list = objGetUsers(furniture);

  if(list == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "count_mobs failed. invalid argument supplied.");
    return NULL;
  }
  
  return Py_BuildValue("i", count_chars(NULL, list, NULL, prototype, FALSE));
}

PyObject *PyChar_all_chars(PyObject *self) {
  PyObject      *list = PyList_New(0);
  LIST_ITERATOR *ch_i = newListIterator(mobile_list);
  CHAR_DATA       *ch = NULL;
  ITERATE_LIST(ch, ch_i)
    PyList_Append(list, charGetPyFormBorrowed(ch));
  deleteListIterator(ch_i);
  return list;
}

PyObject *PyChar_read(PyObject *self, PyObject *args) {
  PyObject *pyset = NULL;
  if(!PyArg_ParseTuple(args, "O", &pyset)) {
    PyErr_Format(PyExc_TypeError, "failed to read character from storage set.");
    return NULL;
  }
  else if(!PyStorageSet_Check(pyset)) {
    PyErr_Format(PyExc_TypeError, "storage set must be supplied to read.");
    return NULL;
  }

  CHAR_DATA *ch = charRead(PyStorageSet_AsSet(pyset));
  char_to_game(ch);
  return Py_BuildValue("O", charGetPyFormBorrowed(ch));
}

PyObject *PyChar_is_abstract(PyObject *self, PyObject *args) {
  char     *mob_key = NULL;
  if (!PyArg_ParseTuple(args, "s", &mob_key)) {
    PyErr_Format(PyExc_TypeError, 
		 "is_abstract failed - it needs a mob key/locale.");
    return NULL;
  }

  PROTO_DATA *proto = worldGetType(gameworld, "mproto", 
				   get_fullkey_relative(mob_key, 
							get_script_locale()));
  if(proto == NULL || protoIsAbstract(proto))
    return Py_BuildValue("i", 1);
  return Py_BuildValue("i", 0);
}

PyMethodDef char_module_methods[] = {
  { "read",     PyChar_read, METH_VARARGS,
    "read(storage_set)\n"
    "\n"
    "Read and return a character from a storage set." },
  { "char_list", (PyCFunction)PyChar_all_chars, METH_NOARGS,
    "char_list()\n"
    "\n"
    "Return a list of every character in game." },
  { "load_mob", PyChar_load_mob, METH_VARARGS,
    "load_mob(proto, room, pos = 'standing')\n"
    "\n"
    "Generate a new mobile from the specified prototype. Add it to the\n"
    "given room. Return the created mobile." },
  { "count_mobs", PyChar_count_mobs, METH_VARARGS,
    "count_mobs(keyword, loc = None)\n"
    "\n"
    "count how many occurences of a mobile with the specified keyword, uid,\n"
    "or prototype exist at a location. If loc is None, search the entire mud.\n"
    "Loc can be a room, room prototype, or furniture object." },
  { "find_char_key", PyChar_find_char_key, METH_VARARGS,
    "Function has been deprecated. Entrypoint for generic_find()\n"
    "Use mud.parse_args instead."  },
  { "is_abstract",   PyChar_is_abstract, METH_VARARGS,
    "is_abstract(proto)\n"
    "\n"
    "Returns whether a specified mob prototype is abstract. Also return True\n"
    "if the prototype does not exist." },
  {NULL, NULL, 0, NULL}  /* Sentinel */
};



//*****************************************************************************
// implementation of pychar.h
//*****************************************************************************
void PyChar_addGetSetter(const char *name, void *g, void *s, const char *doc) {
  // make sure our list of get/setters is created
  if(pychar_getsetters == NULL) pychar_getsetters = newList();

  // make the GetSetter def
  PyGetSetDef *def = calloc(1, sizeof(PyGetSetDef));
  def->name        = strdup(name);
  def->get         = (getter)g;
  def->set         = (setter)s;
  def->doc         = (doc ? strdup(doc) : NULL);
  def->closure     = NULL;
  listPut(pychar_getsetters, def);
}

void PyChar_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pychar_methods == NULL) pychar_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pychar_methods, def);
}



PyMODINIT_FUNC init_PyChar(void) {
  PyObject* m;

  // add in our setters and getters for the char class
  PyChar_addGetSetter("inv", PyChar_getinv, NULL,
    "An immutable list of objects in the character's inventory.\n"
    "See obj.Obj.carrier for altering an item's carrier.");
  PyChar_addGetSetter("objs", PyChar_getinv, NULL,
    "An alias for inv to be consistent with how room and object contents are\n"
    "accessed.");
  PyChar_addGetSetter("eq",   PyChar_geteq,  NULL,
    "An immutable list of the character's worn equipment.\n"
    "See equip() and unequip() for altering a characters worn items.");
  PyChar_addGetSetter("bodyparts", PyChar_getbodyparts, NULL,
    "An immutable list naming all of the character's bodyparts.");
  PyChar_addGetSetter("name", PyChar_getname, PyChar_setname,
    "The characer's name, e.g., Grunald the Baker.");
  PyChar_addGetSetter("mname", PyChar_getmname, PyChar_setmname,
    "The character's name for describing packs, e.g.,\n"
    "a horde of 9001 mosquitos. The number should be replaced by %d, or not\n"
    "included.");
  PyChar_addGetSetter("desc", PyChar_getdesc, PyChar_setdesc,
    "A character's verbose description for e.g., when they are looked at.");
  PyChar_addGetSetter("look_buf", PyChar_getlookbuf, PyChar_setlookbuf,
    "When characters look at something, the thing's description is copied to\n"
    "the character's look buffer for processing before being sent.");
  PyChar_addGetSetter("rdesc", PyChar_getrdesc, PyChar_setrdesc,
    "The character's description when seen in a room, e.g., \n"
    "Bob is here, baking a cake.");
  PyChar_addGetSetter("mdesc", PyChar_getmdesc, PyChar_setmdesc,
    "The equivalent of mname, for room descriptions.");
  PyChar_addGetSetter("keywords", PyChar_getkeywords, PyChar_setkeywords,
    "A comma-separated list of the keywords for referencing the character.");
  PyChar_addGetSetter("sex", PyChar_getsex, PyChar_setsex,
    "The character's sex. Can be male, female, or neutral.");
  PyChar_addGetSetter("gender", PyChar_getsex, PyChar_setsex,
    "Alias for char.Char.sex");
  PyChar_addGetSetter("race", PyChar_getrace, PyChar_setrace,
    "The character's race.");
  PyChar_addGetSetter("pos", PyChar_getposition, PyChar_setposition,
    "Alias for char.Char.position.");
  PyChar_addGetSetter("position", PyChar_getposition, PyChar_setposition,
    "The character's current position (e.g., standing, sleeping, sitting).");
  PyChar_addGetSetter("room", PyChar_getroom, PyChar_setroom,
    "The current room a character is in. Can be set by room or room key.");
  PyChar_addGetSetter("last_room", PyChar_getlastroom, NULL,
    "The last room a character was in. Immutable. Value is None if character\n"
    "was not previously in a room.");
  PyChar_addGetSetter("on", PyChar_geton, PyChar_seton,
   "The furniture the character is sitting on/at. Value is None if character\n"
   "is not currently on furniture. Set value to None to remove a character\n"
   "from their furniture.");
  PyChar_addGetSetter("uid", PyChar_getuid, NULL,
    "The character's unique identification number. Immutable.");
  PyChar_addGetSetter("prototypes", PyChar_getprototypes, NULL,
   "A comma-separated list of prototypes the mobile inherits from. Immutable.");
  PyChar_addGetSetter("mob_class", PyChar_getclass, NULL,
    "The main prototype the mobile inherits from. Immutable.");
  PyChar_addGetSetter("is_npc", PyChar_getisnpc, NULL,
    "Value is True if character is an NPC, and False otherwise. Immutable.");
  PyChar_addGetSetter("is_pc", PyChar_getispc, NULL,
    "Value is negation of char.Char.is_npc");
  PyChar_addGetSetter("hisher", PyChar_gethisher, NULL,
    "Value is 'his', 'her', or 'its'. Immutable.");
  PyChar_addGetSetter("himher", PyChar_gethimher, NULL,
    "Value is 'him', 'her', or 'it'. Immutable.");
  PyChar_addGetSetter("heshe", PyChar_getheshe, NULL,
    "Value is 'he', 'she', or 'it'. Immutable.");
  PyChar_addGetSetter("user_groups", PyChar_getusergroups, NULL,
    "A comma-separated list of user groups the character belongs to.\n"
    "Use char.Char.isInGroup(group) to check for a specific group. Immutable.");
  PyChar_addGetSetter("socket", PyChar_getsocket, NULL,
    "The current socket this character is attached to. Value is None if \n"
    "socket does not exist. Immutable. Use mudsys.attach_char_socket to \n"
    "attach a character and socket to each other.");
  PyChar_addGetSetter("sock",   PyChar_getsocket, NULL,
    "Alias for char.Char.socket");
  PyChar_addGetSetter("hidden", PyChar_gethidden, PyChar_sethidden,
    "Integer value representing how hidden the character is. Default is 0.");
  PyChar_addGetSetter("weight", PyChar_getweight, PyChar_setweight,
    "Floating point value representing how heavy the character is.");
  PyChar_addGetSetter("age", PyChar_getage, NULL,
    "Value is the difference between the character's creation time and the\n"
    "current system time. Immutable.");
  PyChar_addGetSetter("birth", PyChar_getbirth, NULL,
    "Value is the character's creation time (system time). Immutable.");

  // add in all of our methods for the Char class
  PyChar_addMethod("attach", PyChar_attach, METH_VARARGS,
    "attach(trigger)\n"
    "\n"
    "Attach a trigger to the character by key name.");
  PyChar_addMethod("detach", PyChar_detach, METH_VARARGS,
    "detach(trigger)\n"
    "\n"
    "Detach a trigger from the character by key name.");
  PyChar_addMethod("send", PyChar_send, METH_KEYWORDS,
    "send(mssg, dict = None, newline = True)\n"
    "\n"
    "Sends message to the character. Messages can have scripts embedded in\n" 
    "them, using [ and ]. If so, a variable dictionary must be provided. By\n"
    "default, 'me' references the character being sent the message.");
  PyChar_addMethod("send_raw", PyChar_send_raw, METH_VARARGS,
    "send_raw(mssg)\n"
    "\n"
    "Sends message to the character with no newline appended.");
  PyChar_addMethod("sendaround", PyChar_sendaround, METH_KEYWORDS,
    "sendaround(mssg, dict = None, cansee_only = False, newline=True)\n"
    "\n"
    "Sends a message to everyone in the character's room. Messages can have\n"
    "scripts embedded in them using [ and ]. If so, a variable dictionary\n"
    "must be provided. By default, 'me' references the person sendaround is\n"
    "called on, and 'ch' references each character being sent a message.");
  PyChar_addMethod("act", PyChar_act, METH_VARARGS,
    "act(command)\n"
    "\n"
    "Simulate a character typing in a command.");
  PyChar_addMethod("getvar", PyChar_getvar, METH_VARARGS,
    "getvar(name)\n"
    "\n"
    "Return value of a special variable. Return 0 if no value has been set.");
  PyChar_addMethod("setvar", PyChar_setvar, METH_VARARGS,
    "setvar(name, val)\n"
    "\n"
    "Set value of a special variable for the character. Values must be\n"
    "strings or numbers. This function is intended to allow scripts and\n"
    "triggers to open-endedly add variables to characters.");
  PyChar_addMethod("hasvar", PyChar_hasvar, METH_VARARGS,
    "hasvar(name)\n"
    "\n"
    "Return True if a character has the given special variable. False otherwise.");
  PyChar_addMethod("deletevar", PyChar_deletevar, METH_VARARGS,
    "deletevar(name)\n"
    "\n"
    "Deletes a special variable from a character if they have one by the\n"
    "given name.");
  PyChar_addMethod("delvar", PyChar_deletevar, METH_VARARGS,
    "Alias for char.Char.deletevar(name)");
  PyChar_addMethod("equip", PyChar_equip, METH_VARARGS,
    "equip(obj, positions=None, forced=False)\n"
    "\n"
    "Attempts to equip an object to the character's body. Positions can be a\n"
    "comma-separated list of position names or position types. If positions\n"
    "is None and object is of type 'worn', attempt to equip the object to\n"
    "its default positions. Setting forced to True allows non-worn objects\n"
    "to be equipped, or worn objects to be equipped to their non-default\n"
    "positions. Returns success of attempt.");
  PyChar_addMethod("get_equip", PyChar_getequip, METH_VARARGS,
    "get_equip(bodypart)\n"
    "\n"
    "Returns object currently equipped to the character's bodypart, or None.");
  PyChar_addMethod("get_slots", PyChar_getslots, METH_VARARGS,
    "get_slots(obj)\n"
    "\n"
    "Returns a comma-separated list of bodypart names currently occupied by\n"
    "the object.");
  PyChar_addMethod("get_slot_types", PyChar_getslottypes, METH_VARARGS,
    "get_slot_types(obj)\n"
    "\n"
    "Returns a list of the bodypart types currently occupied by the object.\n"
    "Returns an empty list of the object is not equipped to this character.");
  PyChar_addMethod("get_bodypct", PyChar_getbodypct, METH_VARARGS,
    "get_bodypct(posnames)\n"
    "\n"
    "Returns the percent mass of the character's body taken up by the\n"
    "specified parts. Bodyparts must be a comma-separated list.");
  PyChar_addMethod("isActing", PyChar_is_acting, METH_NOARGS,
    "isActing()\n"
    "\n"
    "Returns True if the character is currently taking an action, and False\n"
    "otherwise.");
  PyChar_addMethod("startAction", PyChar_start_action, METH_VARARGS,
    "startAction(delay, on_complete, on_interrupt=None, data=None, arg='')\n"
    "\n"
    "Begins a new delayed action for the character. Delay is in seconds.\n"
    "on_complete is a function taking three arguments: the character, the\n"
    "data, and the argument. Argument must be a string, data can be anything.\n"
    "on_interrupt takes the same arguments as on_complete, but is instead\n"
    "called if the character's action is interrupted.");
  PyChar_addMethod("interrupt", PyChar_interrupt_action, METH_NOARGS,
    "interrupt()\n"
    "\n"
    "Cancel any action the character is currently taking.");
  PyChar_addMethod("getAuxiliary", PyChar_get_auxiliary, METH_VARARGS,
    "getAuxiliary(name)\n"
    "\n"
    "Returns character's auxiliary data of the specified name.");
  PyChar_addMethod("aux", PyChar_get_auxiliary, METH_VARARGS,
    "Alias for char.Char.getAuxiliary(name)");
  PyChar_addMethod("cansee", PyChar_cansee, METH_VARARGS,
    "cansee(thing)\n"
    "\n"
    "Returns whether a character can see the specified object, exit, or other\n"
    "character.");
  PyChar_addMethod("see_as", PyChar_see_as, METH_VARARGS,
    "see_as(thing)\n"
    "\n"
    "Returns the name by which a character sees a specified object, exit, or\n"
    "other character.");
  PyChar_addMethod("page", PyChar_page, METH_VARARGS,
    "page(text)\n"
    "\n"
    "Send text to the character in paginated form e.g., for helpfiles and\n."
    "other large blocks of text.");
  PyChar_addMethod("isinstance", PyChar_isinstance, METH_VARARGS,
    "isinstance(prototype)\n"
    "\n"
    "returns whether the character inherits from a specified mob prototype.");
  PyChar_addMethod("isInGroup", PyChar_is_in_groups, METH_VARARGS,
    "isInGroup(usergroup)\n"
    "\n"
    "Returns whether a character belongs to a specified user group.");
  PyChar_addMethod("hasPrefs", PyChar_hasPreferences, METH_VARARGS,
    "hasPrefs(char_prefs)\n"
    "\n"
    "Return whether character has any of the specified character preferences.\n"
    "Multiples can be specified as a comma-separated string.");
  PyChar_addMethod("append_look", PyChar_append_look, METH_VARARGS,
    "append_look(text)\n"
    "\n"
    "Adds text to the character's current look buffer.");
  PyChar_addMethod("clear_look",  PyChar_clear_look, METH_VARARGS,
    "clear_look()\n"
    "\n"
    "Clear the character's current look buffer.");
  PyChar_addMethod("store", PyChar_store, METH_NOARGS,
    "store()\n"
    "\n"
    "Return a storage set representing the character.");
  PyChar_addMethod("copy", PyChar_copy, METH_NOARGS,
    "copy()\n"
    "\n"
    "Returns a copy of the character.");
  PyChar_addMethod("do_trigs", py_gen_do_trigs, METH_KEYWORDS,
    "do_trigs(type, ch=None, obj=None, room=None, exit=None, cmd=None,\n"
    "         arg=None, opts=None)\n\n"
    "Run triggers of the specified type on the character. By default, the\n"
    "trigger owner is 'me'. Other variables can be specified. The opts\n"
    "variable can be a dictionary that maps optional variable names to their\n"
    "values.");

  // add in all the getsetters and methods
  makePyType(&PyChar_Type, pychar_getsetters, pychar_methods);
  deleteListWith(pychar_getsetters, free); pychar_getsetters = NULL;
  deleteListWith(pychar_methods,    free); pychar_methods    = NULL;

  // make sure the room class is ready to be made
  if (PyType_Ready(&PyChar_Type) < 0)
    return;

  // load the module
  m = Py_InitModule3("char", char_module_methods,
    "Contains the Python wrapper for characters, and utilities for searching,\n"
    "storing, and generating NPCs from mob prototypes.");
  
  // make sure it loaded OK
  if (m == NULL)
    return;

  // add the Char class to the module
  PyTypeObject *type = &PyChar_Type;
  Py_INCREF(&PyChar_Type);
  PyModule_AddObject(m, "Char", (PyObject *)type);
}


int PyChar_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PyChar_Type);
}

int PyChar_AsUid(PyObject *ch) {
  return ((PyChar *)ch)->uid;
}

CHAR_DATA *PyChar_AsChar(PyObject *ch) {
  return propertyTableGet(mob_table, PyChar_AsUid(ch));
}

PyObject *
newPyChar(CHAR_DATA *ch) {
  PyChar *py_ch = (PyChar *)PyChar_new(&PyChar_Type, NULL, NULL);
  py_ch->uid = charGetUID(ch);
  return (PyObject *)py_ch;
}
