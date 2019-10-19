//*****************************************************************************
//
// furniture.c
//
// handles all of the functioning of the furniture item type. Stores data about 
// the capacity of the furniture, and what type of furniture it is. 
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../storage.h"
#include "../character.h"
#include "../object.h"
#include "../world.h"
#include "../socket.h"
#include "../hooks.h"
#include "../olc2/olc.h"

#include "items.h"
#include "iedit.h"
#include "furniture.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pyobj.h"



//*****************************************************************************
// item data for furnitures
//*****************************************************************************
typedef struct furniture_data {
  int capacity;
  int type;
} FURNITURE_DATA;

FURNITURE_DATA *newFurnitureData() {
  FURNITURE_DATA *data = malloc(sizeof(FURNITURE_DATA));
  data->capacity  = 0;
  data->type      = FURNITURE_AT;
  return data;
}

void deleteFurnitureData(FURNITURE_DATA *data) {
  free(data);
}

void furnitureDataCopyTo(FURNITURE_DATA *from, FURNITURE_DATA *to) {
  *to = *from;
}

FURNITURE_DATA *furnitureDataCopy(FURNITURE_DATA *data) {
  FURNITURE_DATA *new_data = newFurnitureData();
  furnitureDataCopyTo(data, new_data);
  return new_data;
}

STORAGE_SET *furnitureDataStore(FURNITURE_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_int(set, "capacity", data->capacity);
  store_int(set, "type",     data->type);
  return set;
}

FURNITURE_DATA *furnitureDataRead(STORAGE_SET *set) {
  FURNITURE_DATA *data = newFurnitureData();
  data->capacity = read_int(set, "capacity");
  data->type     = read_int(set, "type");
  return data;
}



//*****************************************************************************
// functions for interacting with furnitures
//*****************************************************************************
int furnitureGetCapacity(OBJ_DATA *obj) {
  FURNITURE_DATA *data = objGetTypeData(obj, "furniture");
  return data->capacity;
}

void furnitureSetCapacity(OBJ_DATA *obj, int capacity) {
  FURNITURE_DATA *data = objGetTypeData(obj, "furniture");
  data->capacity = capacity;
}

int furnitureGetType(OBJ_DATA *obj) {
  FURNITURE_DATA *data = objGetTypeData(obj, "furniture");
  return data->type;
}  

void furnitureSetType(OBJ_DATA *obj, int type) {
  FURNITURE_DATA *data = objGetTypeData(obj, "furniture");
  data->type = type;
}  

const char *furniture_names[NUM_FURNITURES] = {
  "at",
  "on"
};

const char *furnitureTypeGetName(int type) {
  return furniture_names[type];
}

int furnitureTypeGetNum(const char *type) {
  int i;
  for(i = 0; i < NUM_FURNITURES; i++)
    if(!strcasecmp(furniture_names[i], type))
      return i;
  return FURNITURE_NONE;
}



//*****************************************************************************
// furniture olc
//*****************************************************************************
#define IEDIT_FURNITURE_CAPACITY      1
#define IEDIT_FURNITURE_TYPE          2

// the resedit olc needs these declared
void iedit_furniture_menu(SOCKET_DATA *sock, FURNITURE_DATA *data) {
  send_to_socket(sock, 
		 "{g1) Capacity: {c%d\r\n"
		 "{g2) Sit Type: {c%s\r\n",
		 data->capacity,
		 furnitureTypeGetName(data->type));
}

int  iedit_furniture_chooser(SOCKET_DATA *sock, FURNITURE_DATA *data, 
			     const char *option) {
  switch(toupper(*option)) {
  case '1':
    text_to_buffer(sock, "Enter a new weight capacity for the furniture: ");
    return IEDIT_FURNITURE_CAPACITY;
  case '2':
    olc_display_table(sock, furnitureTypeGetName, NUM_FURNITURES, 1);
    text_to_buffer(sock, "Pick a furniture type: ");
    return IEDIT_FURNITURE_TYPE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool iedit_furniture_parser (SOCKET_DATA *sock, FURNITURE_DATA *data, int choice, 
			  const char *arg) {
  switch(choice) {
  case IEDIT_FURNITURE_CAPACITY: {
    int capacity = atoi(arg);
    if(capacity <= 0)
      return FALSE;
    data->capacity = capacity;
    return TRUE;
  }
  case IEDIT_FURNITURE_TYPE: {
    int type = atoi(arg);
    if(type < 0 || type >= NUM_FURNITURES || !isdigit(*arg))
      return FALSE;
    data->type = type;
    return TRUE;
  }
  default: return FALSE;
  }
}

void furniture_to_proto(FURNITURE_DATA *data, BUFFER *buf) {
  bprintf(buf, "me.furniture_capacity = %d\n",   data->capacity);
  bprintf(buf, "me.furniture_type     = \"%s\"\n", 
	  furnitureTypeGetName(data->type));
}



//*****************************************************************************
// pyobj getters and setters
//*****************************************************************************
PyObject *PyObj_getfurncapacity(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL)
    return NULL;
  else if(objIsType(obj, "furniture"))
    return Py_BuildValue("i", furnitureGetCapacity(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only get capacity for furniture.");
    return NULL;
  }
}

int PyObj_setfurncapacity(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to set capacity for nonexistent "
		 "furniture, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "furniture")) {
    PyErr_Format(PyExc_TypeError, "Tried to set capacity for non-furniture, %s",
		 objGetClass(obj));
    return -1;
  }

  if(!PyInt_Check(value)) {
    PyErr_Format(PyExc_TypeError, "furniture capacity must be an integer.");
    return -1;
  }

  furnitureSetCapacity(obj, PyInt_AsLong(value));
  return 0;
}

PyObject *PyObj_getfurntype(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL)
    return NULL;
  else if(objIsType(obj, "furniture"))
    return Py_BuildValue("s", furnitureTypeGetName(furnitureGetType(obj)));
  else {
    PyErr_Format(PyExc_TypeError, "Can only get furniture type for furniture.");
    return NULL;
  }
}

int PyObj_setfurntype(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to set furniture type for "
		 "nonexistent furniture, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "furniture")) {
    PyErr_Format(PyExc_TypeError, "Tried to set furniture type for "
		 "non-furniture, %s", objGetClass(obj));
    return -1;
  }

  if(!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "furniture type must be a string.");
    return -1;
  }
  else if(furnitureTypeGetNum(PyString_AsString(value)) == FURNITURE_NONE) {
    PyErr_Format(PyExc_TypeError, "Invalid furniture type, %s", 
		 PyString_AsString(value));
    return -1;
  }

  furnitureSetType(obj, furnitureTypeGetNum(PyString_AsString(value)));
  return 0;
}



//*****************************************************************************
// hooks
//*****************************************************************************
void furniture_append_hook(const char *info) {
  OBJ_DATA *obj = NULL;
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &obj, &ch);

  if(objIsType(obj, "furniture")) {
    int num_sitters = listSize(objGetUsers(obj));

    // print out how much room there is left on the furniture
    int seats_left = (furnitureGetCapacity(obj) - num_sitters);
    if(seats_left > 0)
      bprintf(charGetLookBuffer(ch), 
	      " It looks like it could fit %d more %s.\r\n",
	      seats_left, (seats_left == 1 ? "person" : "people"));

    // print character names
    if(num_sitters > 0) {
      LIST *can_see = find_all_chars(ch, objGetUsers(obj), "", NULL, TRUE);
      listRemove(can_see, ch);

      char *chars = print_list(can_see, charGetName, charGetMultiName);
      if(*chars) bprintf(charGetLookBuffer(ch), "%s %s %s %s%s.\r\n",
			 chars, (listSize(can_see) == 1 ? "is" : "are"),
			 (furnitureGetType(obj) == FURNITURE_AT ? "at":"on"),
			 see_obj_as(ch, obj),
			 (charGetFurniture(ch) == obj ? " with you" : ""));
      deleteList(can_see);
      free(chars);
    }
  }
}



//*****************************************************************************
// install the furniture item type
//*****************************************************************************

//
// this will need to be called by init_items() in items/items.c
void init_furniture(void) {
  item_add_type("furniture", 
  		newFurnitureData, deleteFurnitureData,
  		furnitureDataCopyTo, furnitureDataCopy, 
  		furnitureDataStore, furnitureDataRead);

  // add our hooks
  hookAdd("append_obj_desc", furniture_append_hook);

  // set up the furniture OLC too
  item_add_olc("furniture", iedit_furniture_menu, iedit_furniture_chooser, 
  	       iedit_furniture_parser, NULL, furniture_to_proto);

  // add our getters and setters for furniture
  PyObj_addGetSetter("furniture_capacity", 
    PyObj_getfurncapacity, PyObj_setfurncapacity,
    "The number of characters a furniture object can accomodate.");
  PyObj_addGetSetter("furniture_type", PyObj_getfurntype, PyObj_setfurntype,
    "The type of furniture this is: 'at' or 'on' like e.g., tables vs. couches.");
}
