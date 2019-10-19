//*****************************************************************************
//
// pyroom.c
//
// Contains the implementation of a module that contains all of the methods
// for dealing with rooms. Also contains a Python Class implementation of
// NakedMud rooms.
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../utils.h"
#include "../world.h"
#include "../room.h"
#include "../exit.h"
#include "../extra_descs.h"
#include "../character.h"
#include "../handler.h"
#include "../prototype.h"
#include "../commands.h"
#include "../hooks.h"

#include "pyplugs.h"
#include "scripts.h"
#include "pychar.h"
#include "pyobj.h"
#include "pyexit.h"
#include "pyroom.h"
#include "pymudsys.h"
#include "pyauxiliary.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../dyn_vars/dyn_vars.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../dyn_vars/dyn_vars.h"



//*****************************************************************************
// local structures and defines
//*****************************************************************************

// a list of the get/setters on the Room class
LIST *pyroom_getsetters = NULL;

// a list of the methods on the Room class
LIST *pyroom_methods = NULL;

typedef struct {
  PyObject_HEAD
  int uid;
} PyRoom;



//*****************************************************************************
// allocation, deallocation, initialization, and comparison
//*****************************************************************************
void PyRoom_dealloc(PyRoom *self) {
  self->ob_type->tp_free((PyObject*)self);
}

PyObject *PyRoom_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyRoom *self;
    self = (PyRoom *)type->tp_alloc(type, 0);
    self->uid = NOWHERE;
    return (PyObject *)self;
}

int PyRoom_init(PyRoom *self, PyObject *args, PyObject *kwds) {
  char *kwlist[] = {"uid", NULL};
  int        uid = NOTHING;
  PyObject  *who = NULL;

  // get the vnum
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &who)) {
    PyErr_Format(PyExc_TypeError, "a room UID or string ID must be supplied");
    return -1;
  }

  // are we doing it as a uid?
  if(PyInt_Check(who)) {
    uid = (int)PyInt_AsLong(who);
    // make sure a room with the uid exists
    if(!propertyTableGet(room_table, uid)) {
      PyErr_Format(PyExc_TypeError, 
		   "Room with uid, %d, does not exist", uid);
      return -1;
    }

    self->uid = uid;
    return 0;
  }
  else if(PyString_Check(who)) {
    ROOM_DATA *room = NULL;
    if( (room = worldGetRoom(gameworld, get_fullkey_relative(PyString_AsString(who), get_script_locale()))) != NULL) {
      self->uid = roomGetUID(room);
      return 0;
    }
    // create a fresh room with the given key, and add it to the game
    else {
      room = newRoom();
      roomSetClass(room, get_fullkey_relative(PyString_AsString(who),
					      get_script_locale()));
      worldPutRoom(gameworld, roomGetClass(room), room);
      room_to_game(room);
      self->uid = roomGetUID(room);
      return 0;
    }
  }
  else {
    PyErr_Format(PyExc_TypeError, "a room UID or string ID must be supplied");
    return -1;
  }
}

int PyRoom_compare(PyRoom *room1, PyRoom *room2) {
  if(room1->uid == room2->uid)
    return 0;
  else if(room1->uid < room2->uid)
    return -1;
  else
    return 1;
}

long PyRoom_Hash(PyRoom *room) {
  return room->uid;
}



//*****************************************************************************
// methods in the room module
//*****************************************************************************
PyObject *PyRoom_get_room(PyObject *self, PyObject *args) {
  char     *room_key = NULL;
  ROOM_DATA    *room = NULL;
  
  if (!PyArg_ParseTuple(args, "s", &room_key)) {
    PyErr_Format(PyExc_TypeError, 
		 "get room failed - it needs a room key/locale.");
    return NULL;
  }

  // try to find the room
  room = worldGetRoom(gameworld, get_fullkey_relative(room_key, get_script_locale()));

  if(room == NULL)
    return Py_BuildValue("O", Py_None);
  // create a python object for the new char, and return it
  return Py_BuildValue("O", roomGetPyFormBorrowed(room));
}

PyObject *PyRoom_loaded(PyObject *self, PyObject *args) {
  char     *room_key = NULL;
  
  if (!PyArg_ParseTuple(args, "s", &room_key)) {
    PyErr_Format(PyExc_TypeError, 
		 "load check failed - it needs a room key/locale.");
    return NULL;
  }

  bool val = worldRoomLoaded(gameworld, get_fullkey_relative(room_key, get_script_locale()));

  return Py_BuildValue("i", val);
}

PyObject *PyRoom_instance(PyObject *self, PyObject *args) {
  char *room_key = NULL;
  char   *as_key = NULL;
 
  if (!PyArg_ParseTuple(args, "ss", &room_key, &as_key)) {
    PyErr_Format(PyExc_TypeError, 
		 "Instance failed. Need source key and instance key.");
    return NULL;
  }

  // see if the room is already in existence
  ROOM_DATA *room = worldGetRoom(gameworld, as_key);
  if(room != NULL)
    return Py_BuildValue("O", roomGetPyFormBorrowed(room));
  else {
    PROTO_DATA *proto = worldGetType(gameworld, "rproto", room_key);
    if(proto == NULL) {
      PyErr_Format(PyExc_TypeError, "prototype %s does not exist.", room_key);
      return NULL;
    }
    as_key = strdupsafe(get_fullkey_relative(as_key, get_key_locale(room_key)));
    room = protoRoomInstance(proto, as_key);
    if(room == NULL)
      return NULL;
    worldPutRoom(gameworld, as_key, room);
    free(as_key);
    return Py_BuildValue("O", roomGetPyFormBorrowed(room));
  }
}

PyObject *PyRoom_is_abstract(PyObject *self, PyObject *args) {
  char     *room_key = NULL;
  if (!PyArg_ParseTuple(args, "s", &room_key)) {
    PyErr_Format(PyExc_TypeError, 
		 "is_abstract failed - it needs a room key/locale.");
    return NULL;
  }

  PROTO_DATA *proto = worldGetType(gameworld, "rproto", 
				   get_fullkey_relative(room_key, get_script_locale()));
  if(proto == NULL || protoIsAbstract(proto))
    return Py_BuildValue("i", 1);
  return Py_BuildValue("i", 0);
}



//*****************************************************************************
// getters and setters for the Room class
//*****************************************************************************
PyObject *PyRoom_getclass(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) return Py_BuildValue("s", roomGetClass(room));
  else             return NULL;
}

PyObject *PyRoom_getlocale(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) return Py_BuildValue("s",get_key_locale(roomGetClass(room)));
  else             return NULL;
}

PyObject *PyRoom_getprotoname(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) return Py_BuildValue("s",get_key_name(roomGetClass(room)));
  else             return NULL;
}

PyObject *PyRoom_getprotos(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) return Py_BuildValue("s", roomGetPrototypes(room));
  else             return NULL;
}

PyObject *PyRoom_getuid(PyRoom *self, void *closure) {
  return Py_BuildValue("i", self->uid);
}

PyObject *PyRoom_getname(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL)  return Py_BuildValue("s", roomGetName(room));
  else              return NULL;
}

PyObject *PyRoom_getterrain(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL)  
    return Py_BuildValue("s", terrainGetName(roomGetTerrain(room)));
  else
    return NULL;
}

PyObject *PyRoom_getdesc(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL)  return Py_BuildValue("s", roomGetDesc(room));
  else              return NULL;
}

PyObject *PyRoom_getexnames(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL)  return NULL;

  PyObject      *list = PyList_New(0);
  LIST       *ex_list = roomGetExitNames(room);
  LIST_ITERATOR *ex_i = newListIterator(ex_list);
  char           *dir = NULL;
  ITERATE_LIST(dir, ex_i) {
    PyObject    *cont = Py_BuildValue("s", dir);
    PyList_Append(list, cont);
    Py_DECREF(cont);
  } deleteListIterator(ex_i);
  deleteListWith(ex_list, free);
  return list;
}

PyObject *PyRoom_getchars(PyRoom *self, PyObject *args) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL)
    return NULL;
  else {
    LIST_ITERATOR *char_i = newListIterator(roomGetCharacters(room));
    PyObject *list = PyList_New(0);
    CHAR_DATA *ch;

    // for each char in the room list, add him to a Python list
    ITERATE_LIST(ch, char_i)
      PyList_Append(list, charGetPyFormBorrowed(ch));
    deleteListIterator(char_i);
    PyObject *retval = Py_BuildValue("O", list);
    Py_DECREF(list);
    return retval;
  }
}

PyObject *PyRoom_getobjs(PyRoom *self, PyObject *args) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL)
    return NULL;
  else {
    LIST_ITERATOR *obj_i = newListIterator(roomGetContents(room));
    PyObject *list = PyList_New(0);
    OBJ_DATA *obj;

    // for each obj in the room list, add him to a Python list
    ITERATE_LIST(obj, obj_i)
      PyList_Append(list, objGetPyFormBorrowed(obj));
    deleteListIterator(obj_i);
    PyObject *retval = Py_BuildValue("O", list);
    Py_DECREF(list);
    return retval;
  }
}

PyObject *PyRoom_getbits(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room!=NULL) return Py_BuildValue("s", bitvectorGetBits(roomGetBits(room)));
  else           return NULL;
}


//
// Standard check to make sure the room exists when trying to set a value for 
// it. If successful, assign the room to rm. Otherwise, return -1 (error)
#define PYROOM_CHECK_ROOM_EXISTS(uid, room)			  	\
  room = propertyTableGet(room_table, uid);				\
  if(room == NULL) {							\
    PyErr_Format(PyExc_TypeError,					\
		 "Tried to modify nonexistent room, %d", uid);		\
    return -1;                                                          \
  }                                                                            

int PyRoom_setname(PyRoom *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete room's name");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Room names must be strings");
    return -1;
  }

  ROOM_DATA *room;
  PYROOM_CHECK_ROOM_EXISTS(self->uid, room);
  roomSetName(room, PyString_AsString(value));
  return 0;
}

int PyRoom_setdesc(PyRoom *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete room's desc");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, 
                    "Room descs must be strings");
    return -1;
  }

  ROOM_DATA *room;
  PYROOM_CHECK_ROOM_EXISTS(self->uid, room);
  roomSetDesc(room, PyString_AsString(value));
  return 0;
}

int PyRoom_setterrain(PyRoom *self, PyObject *value, void *closure) {
  if (value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete room's terrain");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Room terrain type must be a string");
    return -1;
  }
  
  if(terrainGetNum(PyString_AsString(value)) == TERRAIN_NONE) {
    PyErr_Format(PyExc_TypeError, "Invalid terrain type, %s", 
		 PyString_AsString(value));
    return -1;
  }


  ROOM_DATA *room;
  PYROOM_CHECK_ROOM_EXISTS(self->uid, room);
  roomSetTerrain(room, terrainGetNum(PyString_AsString(value)));
  return 0;
}

int PyRoom_setbits(PyRoom *self, PyObject *value, void *closure) {
  if(value == NULL) {
    PyErr_Format(PyExc_TypeError, "Cannot delete room's bits");
    return -1;
  }
  
  if (!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Room bits must be strings");
    return -1;
  }

  ROOM_DATA *room;
  PYROOM_CHECK_ROOM_EXISTS(self->uid, room);
  bitClear(roomGetBits(room));
  bitSet(roomGetBits(room), PyString_AsString(value));
  return 0;
}



//*****************************************************************************
// methods for the room class
//*****************************************************************************

//
// Send a newline-tagged message to everyone in the room
PyObject *PyRoom_send(PyRoom *self, PyObject *value) {
  char *mssg = NULL;
  if (!PyArg_ParseTuple(value, "s", &mssg)) {
    PyErr_Format(PyExc_TypeError, 
                    "Characters may only be sent strings");
    return NULL;
  }

  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room) {
    send_to_list(roomGetCharacters(room), "%s\r\n", mssg);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
                    "Tried to send message to nonexistant room, %d.", 
		    self->uid);
    return NULL;
  }
}


//
// create a new extra description for the room
PyObject *PyRoom_edesc(PyRoom *self, PyObject *value) {
  char *keywords = NULL;
  char     *desc = NULL;

  if (!PyArg_ParseTuple(value, "ss", &keywords, &desc)) {
    PyErr_Format(PyExc_TypeError, "Extra descs must be strings.");
    return NULL;
  }

  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) {
    EDESC_DATA *edesc = newEdesc(keywords, desc);
    edescSetPut(roomGetEdescs(room), edesc);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError,
		 "Tried to set edesc for nonexistent room, %d.", self->uid);
    return NULL;
  }
}


//
// Get an exit in the room by its direction name
PyObject *PyRoom_get_exit(PyRoom *self, PyObject *value) {
  ROOM_DATA *room = NULL;
  char       *dir = NULL;

  if (!PyArg_ParseTuple(value, "s", &dir)) {
    PyErr_Format(PyExc_TypeError, "Direction of exit not supplied.");
    return NULL;
  }

  if((room = PyRoom_AsRoom((PyObject *)self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to get exit of nonexistent room, %d.", 
		 self->uid);
    return NULL;
  }

  // get the exit
  const char *cdir = dir;
  if(dirGetAbbrevNum(dir) != DIR_NONE)
    cdir = dirGetName(dirGetAbbrevNum(dir));

  EXIT_DATA *exit = roomGetExit(room, cdir);
  if(exit != NULL) {
    PyObject   *pyex = newPyExit(exit);
    PyObject *retval = Py_BuildValue("O", pyex);
    Py_DECREF(pyex);
    return retval;
  }
  else
    return Py_BuildValue("O", Py_None);
}


//
// Returns the direction of the exit
PyObject *PyRoom_get_exit_dir(PyObject *self, PyObject *args) {
  ROOM_DATA *room = NULL;
  EXIT_DATA *exit = NULL;
  PyObject  *pyex = NULL;

  if (!PyArg_ParseTuple(args, "O", &pyex)) {
    PyErr_Format(PyExc_TypeError, "Exit must be supplied.");
    return NULL;
  }

  if((room = PyRoom_AsRoom((PyObject *)self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to get exit dir of nonexistent room"
		 ", %d.", PyRoom_AsUid(self));
    return NULL;
  }

  // get the exit
  if(!PyExit_Check(pyex)) {
    PyErr_Format(PyExc_TypeError, "an exit must be supplied to exdir");
    return NULL;
  }

  // make sure the exit exists
  exit = PyExit_AsExit(pyex);
  if(exit == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to get direction of nonexistant "
		 "exit, %d.", PyExit_AsUid(pyex));
    return NULL;
  }

  return Py_BuildValue("z", roomGetExitDir(room, exit));
}


//
// Fills an exit in the given direction
PyObject *PyRoom_fill(PyRoom *self, PyObject *value) {
  ROOM_DATA *room = NULL;
  char       *dir = NULL;

  if (!PyArg_ParseTuple(value, "s", &dir)) {
    PyErr_Format(PyExc_TypeError, "Direction not supplied to fill.");
    return NULL;
  }

  if((room = PyRoom_AsRoom((PyObject *)self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to fill in non-existant room, %d.", 
		 self->uid);
    return NULL;
  }

  // remove the exit
  EXIT_DATA *exit = roomRemoveExit(room, dir);
  if(exit != NULL) {
    exit_from_game(exit);
    deleteExit(exit);

    // is it a special exit? If so, we may need to remove the command as well
    if(dirGetNum(dir) == DIR_NONE) {
      CMD_DATA *cmd = roomRemoveCmd(room, dir);
      if(cmd != NULL)
	deleteCmd(cmd);
    }
  }

  return Py_BuildValue("i", 1);
}


//
// Links a room to another room, in the specified direction
PyObject *PyRoom_dig(PyRoom *self, PyObject *value) {
  ROOM_DATA   *room = NULL;
  PyObject *py_dest = NULL;
  char         *dir = NULL;
  const char  *cdir = NULL;
  const char  *dest = NULL;

  if (!PyArg_ParseTuple(value, "sO", &dir, &py_dest)) {
    PyErr_Format(PyExc_TypeError, 
		 "When digging, a direction and destination are needed.");
    return NULL;
  }

  if((room = PyRoom_AsRoom((PyObject *)self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to dig in non-existant room, %d.", 
		 self->uid);
    return NULL;
  }

  // make sure we have a valid destination
  if(PyString_Check(py_dest))
    dest = PyString_AsString(py_dest); // get_fullkey_relative(PyString_AsString(py_dest),get_script_locale());
  else if(PyRoom_Check(py_dest)) {
    ROOM_DATA *to_room = PyRoom_AsRoom(py_dest);
    if(to_room != NULL)
      dest = roomGetClass(to_room);
    else {
      PyErr_Format(PyExc_StandardError, 
		   "Tried to dig from %s to invalid room, %d",
		   roomGetClass(room), PyRoom_AsUid(py_dest));
      return NULL;
    }
  }
  else {
    PyErr_Format(PyExc_TypeError, "Invalid destination type in room, %s.",
		 roomGetClass(room));
    return NULL;
  }

  // are we using a special direction name?
  int dir_num        = dirGetNum(dir);
  int dir_abbrev_num = dirGetAbbrevNum(dir);
  if(dir_abbrev_num != DIR_NONE)
    cdir = dirGetName(dir_abbrev_num);
  else
    cdir = dir;

  // do we already have an exit?
  EXIT_DATA *exit = roomGetExit(room, cdir);
  if(exit != NULL) {
    exitSetTo(exit, dest);
  }
  else {
    exit = newExit();
    exit_to_game(exit);
    exitSetTo(exit, dest);
    roomSetExit(room, cdir, exit);

    // if we're digging a special exit, add a cmd for it to the room cmd table
    if(get_cmd_move() && dir_num == DIR_NONE && dir_abbrev_num == DIR_NONE) {
      CMD_DATA *cmd = newPyCmd(cdir, get_cmd_move(), "player", TRUE);

      // add all of our movement checks
      LIST_ITERATOR *chk_i = newListIterator(get_move_checks());
      PyObject        *chk = NULL;
      ITERATE_LIST(chk, chk_i) {
	cmdAddPyCheck(cmd, chk);
      } deleteListIterator(chk_i);

      //cmdAddCheck(cmd, chk_can_move);
      roomAddCmd(room, cdir, NULL, cmd);
    }
  }

  PyObject   *pyex = newPyExit(exit);
  PyObject *retval = Py_BuildValue("O", pyex);
  Py_DECREF(pyex);
  return retval;
}


PyObject *PyRoom_attach(PyRoom *self, PyObject *args) {  
  char *key = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "To attach a script, the key must be supplied.");
    return NULL;
  }

  // pull out the room and do the attaching
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach script to nonexistant room, %d.", self->uid);
    return NULL;
  }

  TRIGGER_DATA *trig =
    worldGetType(gameworld, "trigger", 
		 get_fullkey_relative(key, get_script_locale()));
  if(trig != NULL) {
    triggerListAdd(roomGetTriggers(room), triggerGetKey(trig));
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to attach nonexistant script, %s, to room %s.",
		 key, roomGetClass(room));
    return NULL;
  }
}


PyObject *PyRoom_detach(PyRoom *self, PyObject *args) {  
  char *key = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "To detach a script, the key must be suppplied.");
    return NULL;
  }

  // pull out the room and do the attaching
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) {
    const char *fkey = get_fullkey_relative(key, get_script_locale());
    triggerListRemove(roomGetTriggers(room), fkey);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to detach script from nonexistant room, %d.",self->uid);
    return NULL;
  }
}


//
// adds a new command to the room
PyObject *PyRoom_add_cmd(PyRoom *self, PyObject *args) {
  PyObject *func = NULL;
  char *name  = NULL, *sort_by = NULL, *group = NULL;
  bool interrupts = FALSE;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "szOs|b", &name, &sort_by, &func,
  			&group, &interrupts)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not add new room command. Improper arguments supplied");
    return NULL;
  }

  // make sure the room exists
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to add command to nonexistent room, %d", self->uid);
    return NULL;
  }

  // add the command to the game
  roomAddCmd(room, name, sort_by, newPyCmd(name, func, group, TRUE));
  return Py_BuildValue("O", Py_None);
}

//
// adds a pre-check to a room command
PyObject *PyRoom_add_cmd_check(PyRoom *self, PyObject *args) {
  PyObject *func = NULL;
  char    *name  = NULL;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "sO", &name, &func)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not add new room command check. "
		 "Improper arguments supplied");
    return NULL;
  }

  // make sure the room exists
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to add command check to nonexistent room, %d", 
		 self->uid);
    return NULL;
  }

  // get the command
  CMD_DATA *cmd = roomGetCmd(room, name, FALSE);

  // command doesn't exist; add a null command so we can 
  // register just the check on larger-scale tables
  if(cmd == NULL) {
    cmd = newCmd(name, NULL, "", FALSE);
    roomAddCmd(room, name, NULL, cmd);
  }

  if(cmd != NULL)
    cmdAddPyCheck(cmd, func);
  return Py_BuildValue("O", Py_None);
}

//
// returns whether or not the character is an instance of the prototype
PyObject *PyRoom_isinstance(PyRoom *self, PyObject *args) {  
  char *type = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &type)) {
    PyErr_Format(PyExc_TypeError, "isinstance only accepts strings.");
    return NULL;
  }

  // pull out the object and check the type
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL)
    return Py_BuildValue("i", 
        roomIsInstance(room, get_fullkey_relative(type, get_script_locale())));
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to check instances of nonexistent room, %d.",self->uid);
    return NULL;
  }
}

//
// returns the specified piece of auxiliary data from the room
// if it is a piece of python auxiliary data.
PyObject *PyRoom_get_auxiliary(PyRoom *self, PyObject *args) {
  char *keyword = NULL;
  if(!PyArg_ParseTuple(args, "s", &keyword)) {
    PyErr_Format(PyExc_TypeError,
		 "getAuxiliary() must be supplied with the name that the "
		 "auxiliary data was installed under!");
    return NULL;
  }

  // make sure we exist
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to get auxiliary data for a nonexistant room.");
    return NULL;
  }

  // make sure the auxiliary data exists
  if(!pyAuxiliaryDataExists(keyword)) {
    PyErr_Format(PyExc_StandardError,
		 "No auxiliary data named '%s' exists!", keyword);
    return NULL;
  }

  PyObject *data = roomGetAuxiliaryData(room, keyword);
  if(data == NULL)
    data = Py_None;
  PyObject *retval = Py_BuildValue("O", data);
  //  Py_DECREF(data);
  return retval;
}


//
// Returns TRUE if the room has the given variable set
PyObject *PyRoom_hasvar(PyRoom *self, PyObject *arg) {
  char *var = NULL;
  if (!PyArg_ParseTuple(arg, "s", &var)) {
    PyErr_Format(PyExc_TypeError, 
                    "Room variables must have string names.");
    return NULL;
  }

  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL)
    return Py_BuildValue("b", roomHasVar(room, var));

  PyErr_Format(PyExc_TypeError, 
	       "Tried to get a variable value for nonexistant room, %d",
	       self->uid);
  return NULL;
}


//
// Delete the variable set on the room with the specified name
PyObject *PyRoom_deletevar(PyRoom *self, PyObject *arg) {
  char *var = NULL;
  if (!PyArg_ParseTuple(arg, "s", &var)) {
    PyErr_Format(PyExc_TypeError, 
                    "Room variables must have string names.");
    return NULL;
  }

  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) {
    roomDeleteVar(room, var);
    return Py_BuildValue("i", 1);
  }

  PyErr_Format(PyExc_TypeError, 
	       "Tried to get a variable value for nonexistant room, %d",
	       self->uid);
  return NULL;
}


//
// Get the value of a variable stored on the room
PyObject *PyRoom_getvar(PyRoom *self, PyObject *arg) {
  char *var = NULL;
  if (!PyArg_ParseTuple(arg, "s", &var)) {
    PyErr_Format(PyExc_TypeError, 
                    "Room variables must have string names.");
    return NULL;
  }

  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) {
    int vartype = roomGetVarType(room, var);
    if(vartype == DYN_VAR_INT)
      return Py_BuildValue("i", roomGetInt(room, var));
    else if(vartype == DYN_VAR_LONG)
      return Py_BuildValue("i", roomGetLong(room, var));
    else if(vartype == DYN_VAR_DOUBLE)
      return Py_BuildValue("d", roomGetDouble(room, var));
    else
      return Py_BuildValue("s", roomGetString(room, var));
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to get a variable value for nonexistant room, %d",
		 self->uid);
    return NULL;
  }
}


//
// Set the value of a variable assocciated with the character
PyObject *PyRoom_setvar(PyRoom *self, PyObject *args) {  
  char     *var = NULL;
  PyObject *val = NULL;

  if (!PyArg_ParseTuple(args, "sO", &var, &val)) {
    PyErr_Format(PyExc_TypeError, 
		 "Room setvar must be supplied with a var name and integer value.");
    return NULL;
  }

  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room != NULL) {
    if(PyInt_Check(val))
      roomSetInt(room, var, (int)PyInt_AsLong(val));
    else if(PyFloat_Check(val))
      roomSetDouble(room, var, PyFloat_AsDouble(val));
    else if(PyString_Check(val))
      roomSetString(room, var, PyString_AsString(val));
    else {
      PyErr_Format(PyExc_TypeError,
		   "Tried to store a room_var of invalid type on room %d.",
		   self->uid);
      return NULL;
    }
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to set a variable value for nonexistant room, %d",
		 self->uid);
    return NULL;
  }
}

//
// run all of a room's reset commands
PyObject *PyRoom_reset(PyRoom *self, void *closure) {
  ROOM_DATA *room = PyRoom_AsRoom((PyObject *)self);
  if(room == NULL) {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to run resets for nonexistant room, %d",
		 self->uid);
    return NULL;
  }
  hookRun("reset_room", hookBuildInfo("rm", room));
  return Py_BuildValue("");
}

PyObject *PyRoom_hasBit(PyObject *self, PyObject *args) {
  char *bits = NULL;

  // make sure we're getting passed the right type of data
  if (!PyArg_ParseTuple(args, "s", &bits)) {
    PyErr_Format(PyExc_TypeError, "hasBit only accepts strings.");
    return NULL;
  }

  // pull out the object and check the type
  ROOM_DATA *rm = PyRoom_AsRoom(self);
  if(rm != NULL)
    return Py_BuildValue("i", bitIsSet(roomGetBits(rm), bits));
  else {
    PyErr_Format(PyExc_StandardError, 
		 "Tried to check bits of nonexistent room, %d.", PyRoom_AsUid(self));
    return NULL;
  }
}



//*****************************************************************************
// structures to define our methods and classes
//*****************************************************************************
PyTypeObject PyRoom_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "room.Room",               /*tp_name*/
    sizeof(PyRoom),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyRoom_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    (cmpfunc)PyRoom_compare,   /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    (hashfunc)PyRoom_Hash,     /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "__init__(self, uid_or_string_key)\n\n"
    "Creates a new Python reference to a room, by uid. If a string database\n"
    "key is instead supplied, first try to generate a room by an rporoto of\n"
    "the same name. If no rproto exists, create a new blank room in the room\n"
    "table, and assign it the given key.",
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
    (initproc)PyRoom_init,     /* tp_init */
    0,                         /* tp_alloc */
    PyRoom_new,                /* tp_new */
};

PyMethodDef room_module_methods[] = {
  { "get_room", (PyCFunction)PyRoom_get_room, METH_VARARGS,
    "get_room(key)\n"
    "\n"
    "Takes a room key/locale and returns the matching room, or None." },
  { "is_loaded", (PyCFunction)PyRoom_loaded, METH_VARARGS,
    "is_loaded(key)\n"
    "\n"
    "Returns whether a room with the given key currently exists in game." },
  { "instance",  (PyCFunction)PyRoom_instance, METH_VARARGS,
    "instance(room_proto, as_key = None)\n"
    "\n"
    "Create an instanced copy of a room, specified by a room prototype name.\n"
    "If as_key is None, the instanced room's key will be a derivation of\n"
    "the original prototype name, with a uid number appended to it. Otherwise\n"
    "as_key is used for the room's key." },
  { "is_abstract",  (PyCFunction)PyRoom_is_abstract, METH_VARARGS,
    "is_abstract(proto)\n"
    "\n"
    "Returns whether a specified room prototype is abstract. Also return True\n"
    "if the prototype does not exist." },
  {NULL, NULL, 0, NULL}  /* Sentinel */
};



//*****************************************************************************
// implementation of pyroom.h
//*****************************************************************************
void PyRoom_addGetSetter(const char *name, void *g, void *s, const char *doc) {
  // make sure our list of get/setters is created
  if(pyroom_getsetters == NULL) pyroom_getsetters = newList();

  // make the GetSetter def
  PyGetSetDef *def = calloc(1, sizeof(PyGetSetDef));
  def->name        = strdup(name);
  def->get         = (getter)g;
  def->set         = (setter)s;
  def->doc         = (doc ? strdup(doc) : NULL);
  def->closure     = NULL;
  listPut(pyroom_getsetters, def);
}

void PyRoom_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pyroom_methods == NULL) pyroom_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pyroom_methods, def);
}


// initialize rooms for use. This must be called AFTER 
PyMODINIT_FUNC
init_PyRoom(void) {
    PyObject* module = NULL;

    // add all of the basic getsetters
    PyRoom_addGetSetter("name",    PyRoom_getname,     PyRoom_setname, 
      "The room's name, e.g., Town Square.");
    PyRoom_addGetSetter("desc",    PyRoom_getdesc,     PyRoom_setdesc, 
      "The room's description when, e.g., looked at.");
    PyRoom_addGetSetter("proto",   PyRoom_getclass,    NULL, 
      "The room's unique identifier key. For non-instanced rooms, equivalent\n"
      "to the main room prototype inherited from. Immutable.");
    PyRoom_addGetSetter("locale",  PyRoom_getlocale,   NULL, 
      "The zone a room belongs to. Immutable.");
    PyRoom_addGetSetter("protoname",PyRoom_getprotoname,NULL, 
      "The first half of the room's unique identifier key. Immutable.");
    PyRoom_addGetSetter("protos",  PyRoom_getprotos,   NULL, 
      "A comma-separated list of room prototypes this room inherits from. Immutable.");
    PyRoom_addGetSetter("chars",   PyRoom_getchars,    NULL, 
      "A list of all characters in the room. Immutable. See char.Char.room\n"
      "for changing the room a character is in.");
    PyRoom_addGetSetter("objs",  PyRoom_getobjs,       NULL, 
      "Alias for room.Room.contents");
    PyRoom_addGetSetter("contents",PyRoom_getobjs,     NULL, 
      "A list of objects in the room. Immutable. See obj.Obj.room for\n"
      "changing the room an object is in.");
    PyRoom_addGetSetter("exnames", PyRoom_getexnames,  NULL, 
      "A list of the room's exits, by direction. Immutable. See room.Room.dig\n"
      "for creating new links between rooms.");
    PyRoom_addGetSetter("uid",     PyRoom_getuid,      NULL,
      "The room's unique identification number. Immutable.");
    PyRoom_addGetSetter("terrain", PyRoom_getterrain,  PyRoom_setterrain,
      "The current terrain type of the room.");
    PyRoom_addGetSetter("bits",    PyRoom_getbits,     PyRoom_setbits,
      "A comma-separated list of setting bits currently toggled on the room.");

    // add all of the basic methods
    PyRoom_addMethod("attach", PyRoom_attach, METH_VARARGS,
      "attach(trigger)\n"
      "\n"
      "Attach a trigger to the room by key name.");
    PyRoom_addMethod("detach", PyRoom_detach, METH_VARARGS,
      "detach(trigger)\n"
      "\n"
      "Detach a trigger from the room by key name.");
    PyRoom_addMethod("dig", PyRoom_dig, METH_VARARGS,
      "dig(dir, dest)\n"
      "\n"
      "Link the room to another room via the specified direction. The\n"
      "destination room can be an actual room or a room key name. Returns\n"
      "the created exit. If an exit already exists in the specified\n"
      "direction, change its destination.");
    PyRoom_addMethod("fill", PyRoom_fill, METH_VARARGS,
      "fill(dir)\n"
      "\n"
      "Erases an exit in the specified direction.");
    PyRoom_addMethod("exit", PyRoom_get_exit, METH_VARARGS,
      "exit(dir)\n"
      "\n"
      "Returns an exit for the specified direction, or None.");
    PyRoom_addMethod("exdir", PyRoom_get_exit_dir, METH_VARARGS,
      "exdir(exit)\n"
      "\n"
      "Returns the direction for a specified exit, or None.");
    PyRoom_addMethod("send", PyRoom_send, METH_VARARGS,
      "send(mssg)\n"
      "\n"
      "Send a message to all characters in the room.");
    PyRoom_addMethod("edesc", PyRoom_edesc, METH_VARARGS,
      "edesc(keywords, desc)\n"
      "\n"
      "Create an extra description for the room, accessible via a comma-\n"
      "separated list of keywords.");
    PyRoom_addMethod("add_cmd", PyRoom_add_cmd, METH_VARARGS,
      "add_cmd(name, shorthand, cmd_func, user_group, interrupts = False)\n"
      "\n"
      "Add a new player command specific to the room. See mudsys.add_cmd for\n"
      "documentation on commands.");
    PyRoom_addMethod("add_cmd_check", PyRoom_add_cmd_check, METH_VARARGS,
      "add_cmd_check(cmd_name, check_func)\n"
      "\n"
      "Add a new command check function specific to the room. See \n"
      "mudsys.add_cmd_check for documentation on command checks.");
    PyRoom_addMethod("isinstance", PyRoom_isinstance, METH_VARARGS,
      "isinstance(prototype)\n"
      "\n"
      "returns whether the room inherits from a specified room prototype.");
    PyRoom_addMethod("getAuxiliary", PyRoom_get_auxiliary, METH_VARARGS,
      "getAuxiliary(name)\n"
      "\n"
      "Returns room's auxiliary data of the specified name.");
    PyRoom_addMethod("aux", PyRoom_get_auxiliary, METH_VARARGS,
      "Alias for room.Room.getAuxiliary(name)");
    PyRoom_addMethod("getvar", PyRoom_getvar, METH_VARARGS,
      "getvar(name)\n"
      "\n"
      "Return value of a special variable. Return 0 if no value has been set.");
    PyRoom_addMethod("setvar", PyRoom_setvar, METH_VARARGS,
      "setvar(name, val)\n"
      "\n"
      "Set value of a special variable for the room. Values must be strings \n"
      "or numbers. This function is intended to allow scripts and triggers to"
      "open-endedly add variables to rooms.");
    PyRoom_addMethod("hasvar", PyRoom_hasvar, METH_VARARGS,
      "hasvar(name)\n"
      "\n"
      "Return True if a room has the given special variable. False otherwise.");
    PyRoom_addMethod("deletevar", PyRoom_deletevar, METH_VARARGS,
      "deletevar(name)\n"
      "\n"
      "Deletes a special variable from a room if they have one by the\n"
      "given name.");
    PyRoom_addMethod("delvar", PyRoom_deletevar, METH_VARARGS,
      "Alias for room.Room.deletevar(name)");
    PyRoom_addMethod("reset", PyRoom_reset, METH_NOARGS,
      "reset()\n"
      "\n"
      "Runs a room's reset commands and reset hooks.");
    PyRoom_addMethod("hasBit", PyRoom_hasBit, METH_VARARGS,
      "hasBit(name)\n"
      "\n"
      "Return whether room has a bit toggled.\n");
    PyRoom_addMethod("do_trigs", py_gen_do_trigs, METH_KEYWORDS,
      "do_trigs(type, ch=None, obj=None, room=None, exit=None, cmd=None,\n"
      "         arg=None, opts=None)\n\n"
      "Run triggers of the specified type on the room. By default, the\n"
      "trigger owner is 'me'. Other variables can be specified. The opts\n"
      "variable can be a dictionary that maps optional variable names to their\n"
      "values.");

    // add in all the getsetters and methods
    makePyType(&PyRoom_Type, pyroom_getsetters, pyroom_methods);
    deleteListWith(pyroom_getsetters, free); pyroom_getsetters = NULL;
    deleteListWith(pyroom_methods,    free); pyroom_methods    = NULL;

    // make sure the room class is ready to be made
    if (PyType_Ready(&PyRoom_Type) < 0)
        return;

    // initialize the module
    module = Py_InitModule3("room", room_module_methods,
      "Contains the Python wrapper for rooms, and utilities for loading and\n"
      "instancing rooms.");

    // make sure the module parsed OK
    if (module == NULL)
      return;

    // add the Room class to the room module
    PyTypeObject *type = &PyRoom_Type;
    PyModule_AddObject(module, "Room", (PyObject *)type);
    Py_INCREF(&PyRoom_Type);
}

int PyRoom_AsUid(PyObject *room) {
  return ((PyRoom *)room)->uid;
}

ROOM_DATA *PyRoom_AsRoom(PyObject *room) {
  return propertyTableGet(room_table, PyRoom_AsUid(room));
}

int PyRoom_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PyRoom_Type);
}

PyObject *
newPyRoom(ROOM_DATA *room) {
  PyRoom *py_room = (PyRoom *)PyRoom_new(&PyRoom_Type, NULL, NULL);
  py_room->uid = roomGetUID(room);
  return (PyObject *)py_room;
}
