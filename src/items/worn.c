//*****************************************************************************
//
// worn.c
//
// handles all of the functioning of wearable items. Perhaps this could
// eventually be extended to include armors? I think, perhaps, one of the 
// weirdest things about (most) DIKUs is that worn items and armors are two
// different item types; really, wearable items are just armors that provide
// no armor class. Fusing the two item types into one might be a much more
// fruitful route to take. Or perhaps another route would be to just make 
// another item type called "armor" that only functions if the item is also of
// type "worn"; it calculates armor class/protection/whatnot based on the type
// of worn item the item is.
//
// that said, I'm not going to do it. Well, not for NakedMud anyways; I don't
// want to burden other developers with my conception of what a good way to do
// armor class is. Therefore, if you agree with me, I leave the exercise up to
// you :)
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../storage.h"
#include "../character.h"
#include "../socket.h"
#include "../room.h"
#include "../world.h"
#include "../object.h"
#include "../inform.h"
#include "../handler.h"
#include "../hooks.h"

#include "../olc2/olc.h"
#include "iedit.h"
#include "items.h"
#include "worn.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pyobj.h"
#include "../scripts/pymudsys.h"



//*****************************************************************************
// local functions, variables, datastructures, and defines
//*****************************************************************************
HASHTABLE *worn_table = NULL;

typedef struct worn_entry {
  char *type;
  char *positions;
} WORN_ENTRY;  

WORN_ENTRY *newWornEntry(const char *type, const char *positions) {
  WORN_ENTRY *entry = malloc(sizeof(WORN_ENTRY));
  entry->type      = strdupsafe(type);
  entry->positions = strdupsafe(positions);
  return entry;
}

void deleteWornEntry(WORN_ENTRY *entry) {
  if(entry->positions) free(entry->positions);
  if(entry->type)      free(entry->type);
  free(entry);
}

const char *wornTypeGetPositions(const char *type) {
  WORN_ENTRY *entry = hashGet(worn_table, type);
  return (entry ? entry->positions : "");
}

//
// append information about where the item can be worn
void append_worn_hook(const char *info) {
  OBJ_DATA *obj = NULL;
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &obj, &ch);

  if(objIsType(obj, "worn")) {
    bprintf(charGetLookBuffer(ch),"When worn, this item covers bodyparts: %s.",
	    wornGetPositions(obj));
  }
}



//*****************************************************************************
// item data for worns
//*****************************************************************************
typedef struct worn_data {
  char *type;
} WORN_DATA;

WORN_DATA *newWornData() {
  WORN_DATA *data = malloc(sizeof(WORN_DATA));
  data->type      = strdup("");
  return data;
}

void deleteWornData(WORN_DATA *data) {
  if(data->type) free(data->type);
  free(data);
}

void wornDataCopyTo(WORN_DATA *from, WORN_DATA *to) {
  if(to->type) free(to->type);
  to->type = strdupsafe(from->type);
}

WORN_DATA *wornDataCopy(WORN_DATA *data) {
  WORN_DATA *new_data = newWornData();
  wornDataCopyTo(data, new_data);
  return new_data;
}

STORAGE_SET *wornDataStore(WORN_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "type", data->type);
  return set;
}

WORN_DATA *wornDataRead(STORAGE_SET *set) {
  WORN_DATA *data = malloc(sizeof(WORN_DATA));
  data->type = strdup(read_string(set, "type"));
  return data;
}



//*****************************************************************************
// functions for interacting with worns
//*****************************************************************************
const char *wornGetType(OBJ_DATA *obj) {
  WORN_DATA *data = objGetTypeData(obj, "worn");
  return data->type;
}

const char *wornGetPositions(OBJ_DATA *obj) {
  WORN_DATA *data = objGetTypeData(obj, "worn");
  return wornTypeGetPositions(data->type);
}

void wornSetType(OBJ_DATA *obj, const char *type) {
  WORN_DATA *data = objGetTypeData(obj, "worn");
  if(data->type) free(data->type);
  data->type = strdupsafe(type);
}



//*****************************************************************************
// worn olc
//*****************************************************************************
#define IEDIT_WORN_TYPE     1

void iedit_worn_show_types(SOCKET_DATA *sock) {
  // we want to display them all by alphabetical order
  LIST *types = newList();

  HASH_ITERATOR *hash_i = newHashIterator(worn_table);
  const char       *key = NULL;
  WORN_ENTRY       *val = NULL;

  // collect all of the types
  ITERATE_HASH(key, val, hash_i)
    listPutWith(types, strdup(key), strcasecmp);
  deleteHashIterator(hash_i);

  // display all of the types
  LIST_ITERATOR *type_i = newListIterator(types);
  int col = 0;

  text_to_buffer(sock, "{wEditable item types:{g\r\n");
  ITERATE_LIST(key, type_i) {
    col++;
    send_to_socket(sock, "  %-14s%s",
		   key, ((col != 0 && col % 4 == 0) ? "\r\n": "   "));
  }
  deleteListIterator(type_i);
  deleteListWith(types, free);
  if(col % 4 != 0) text_to_buffer(sock, "\r\n");
}


//
// the resedit olc needs these declared
void iedit_worn_menu   (SOCKET_DATA *sock, WORN_DATA *data) {
  send_to_socket(sock, 
		 "{g1) type     : {c%s\r\n"
		 "{g   equips to: {c%s\r\n",
		 data->type, wornTypeGetPositions(data->type));
}

int  iedit_worn_chooser(SOCKET_DATA *sock, WORN_DATA *data, const char *option){
  switch(toupper(*option)) {
  case '1':
    iedit_worn_show_types(sock);
    text_to_buffer(sock, "enter choice: ");
    return IEDIT_WORN_TYPE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool iedit_worn_parser (SOCKET_DATA *sock, WORN_DATA *data, int choice, 
			  const char *arg) {
  switch(choice) {
  case IEDIT_WORN_TYPE:
    if(!hashIn(worn_table, arg))
      return FALSE;
    if(data->type) free(data->type);
    data->type = strdup(arg);
    return TRUE;
  default:
    return FALSE;
  }
}

void worn_to_proto(WORN_DATA *worn, BUFFER *buf) {
  bprintf(buf, "me.worn_type = \"%s\"\n", worn->type);
}



//*****************************************************************************
// python extentions
//*****************************************************************************
PyObject *PyObj_getwornlocs(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL)
    return NULL;
  else if(objIsType(obj, "worn"))
    return Py_BuildValue("s", wornGetPositions(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only get wornlocs for wearable items.");
    return NULL;
  }
}

PyObject *PyObj_getworntype(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL)
    return NULL;
  else if(objIsType(obj, "worn"))
    return Py_BuildValue("s", wornGetType(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only get worntype for wearable items.");
    return NULL;
  }
}

int PyObj_setworntype(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to set worntype for nonexistent "
		 "clothing, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "worn")) {
    PyErr_Format(PyExc_TypeError, "Tried to set worntype for non-clothing, %s",
		 objGetClass(obj));
    return -1;
  }

  if(!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Clothing worntype must be a string.");
    return -1;
  }
  
  if(!hashIn(worn_table, PyString_AsString(value))) {
    PyErr_Format(PyExc_TypeError, "Invalid worn type, %s.", 
		 PyString_AsString(value));
    return -1;
  }

  wornSetType(obj, PyString_AsString(value));
  return 0;
}

PyObject *PyMudSys_AddWornType(PyObject *self, PyObject *args) {
  char *type = NULL;
  char  *pos = NULL;

  if(!PyArg_ParseTuple(args, "ss", &type, &pos)) {
    PyErr_Format(PyExc_TypeError, "add_worn_type requires worn type "
		 "and position list.");
    return NULL;
  }

  worn_add_type(type, pos);
  return Py_BuildValue("i", 1);
}



//*****************************************************************************
// install the worn item type
//*****************************************************************************
void worn_add_type(const char *type, const char *required_positions) {
  WORN_ENTRY *entry = NULL;

  // make sure we don't currently have an entry
  if((entry = hashRemove(worn_table, type)) != NULL)
    deleteWornEntry(entry);
  hashPut(worn_table, type, newWornEntry(type, required_positions));
}


//
// this will need to be called by init_items() in items/items.c
void init_worn(void) {
  worn_table = newHashtable();
  item_add_type("worn", 
		newWornData, deleteWornData,
		wornDataCopyTo, wornDataCopy, 
		wornDataStore, wornDataRead);

  // set up the worn OLC too
  item_add_olc("worn", iedit_worn_menu, iedit_worn_chooser, iedit_worn_parser,
	       NULL, worn_to_proto);

  // attach our hooks to display worn info on look
  hookAdd("append_obj_desc", append_worn_hook);

  // add our new python get/setters
  PyObj_addGetSetter("worn_locs", PyObj_getwornlocs, NULL,
    "The position names this worn type must be equipped to. Immutable.");
  PyObj_addGetSetter("worn_type", PyObj_getworntype, PyObj_setworntype,
    "The type of worn item this is.");
  PyMudSys_addMethod("add_worn_type", PyMudSys_AddWornType, METH_VARARGS,
    "add_worn_type(type, postypes)\n"
    "\n"
    "Register a new type of worn item. Postypes is a comma-separated list of\n"
    "body position types this object must be equipped to e.g., \n"
    "shirt : torso, arm, arm.");
  
  // add in our basic worn types
  worn_add_type("shirt",              "torso, arm, arm");
  /*
    Removed as of v3.3 -- These can now be added via Python with the function,
    mudsys.add_worn_type(<type>, <position list>)
  worn_add_type("gloves",       "left hand, right hand");
  worn_add_type("left glove",               "left hand");
  worn_add_type("right glove",             "right hand");
  worn_add_type("earrings",                  "ear, ear");
  worn_add_type("earring",                        "ear");
  worn_add_type("ring",                        "finger");
  */
}
