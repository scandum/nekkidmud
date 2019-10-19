//*****************************************************************************
//
// container.c
//
// handles all of the functioning of the container item type. Stores data about 
// the contents and capacity of a container, and all the functions for 
// interacting with container types.
//
//*****************************************************************************
#include "../mud.h"
#include "../storage.h"
#include "../object.h"
#include "../character.h"
#include "../world.h"
#include "../socket.h"
#include "../utils.h"
#include "../hooks.h"
#include "../inform.h"
#include "../olc2/olc.h"

#include "items.h"
#include "iedit.h"
#include "container.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pyobj.h"



//*****************************************************************************
// item data for containers
//*****************************************************************************
typedef struct container_data {
  double capacity;
  char  *key;
  int    pick_diff;
  bool   closable;
  bool   closed;
  bool   locked;
} CONTAINER_DATA;

CONTAINER_DATA *newContainerData() {
  CONTAINER_DATA *data = malloc(sizeof(CONTAINER_DATA));
  data->capacity  = 0;
  data->key       = strdup("");
  data->pick_diff = 0;
  data->closed    = FALSE;
  data->locked    = FALSE;
  return data;
}

void deleteContainerData(CONTAINER_DATA *data) {
  if(data->key) free(data->key);
  free(data);
}

void containerDataCopyTo(CONTAINER_DATA *from, CONTAINER_DATA *to) {
  *to = *from;
  to->key = strdupsafe(from->key);
}

CONTAINER_DATA *containerDataCopy(CONTAINER_DATA *data) {
  CONTAINER_DATA *new_data = newContainerData();
  containerDataCopyTo(data, new_data);
  return new_data;
}

STORAGE_SET *containerDataStore(CONTAINER_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_double(set, "capacity", data->capacity);
  store_string(set, "key",      data->key);
  store_int   (set, "pick_diff",data->pick_diff);
  store_int   (set, "closable", data->closable);
  store_int   (set, "closed",   data->closed);
  store_int   (set, "locked",   data->locked);
  return set;
}

CONTAINER_DATA *containerDataRead(STORAGE_SET *set) {
  CONTAINER_DATA *data = newContainerData();
  data->key      = strdupsafe(read_string(set, "key"));
  data->capacity = read_double(set, "capacity");
  data->pick_diff= read_int   (set, "pick_diff");
  data->closable = read_int   (set, "closable");
  data->closed   = read_int   (set, "closed");
  data->locked   = read_int   (set, "locked");
  return data;
}



//*****************************************************************************
// functions for interacting with containers
//*****************************************************************************
double containerGetCapacity(OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->capacity;
}

void containerSetCapacity(OBJ_DATA *obj, double capacity) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->capacity = capacity;
}

bool containerIsClosable (OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->closable;
}

bool containerIsClosed   (OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->closed;
}

bool containerIsLocked   (OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->locked;
}

const char *containerGetKey(OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->key;
}

int  containerGetPicKDiff(OBJ_DATA *obj) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  return data->pick_diff;
}

void containerSetClosed  (OBJ_DATA *obj, bool closed) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->closed = closed;
}

void   containerSetClosable(OBJ_DATA *obj, bool closable) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->closable = closable;
}

void containerSetLocked  (OBJ_DATA *obj, bool locked) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->locked = locked;
}

void containerSetKey(OBJ_DATA *obj, const char *key) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  if(data->key) free(data->key);
  data->key = strdup(key);
}

void containerSetPickDiff(OBJ_DATA *obj, int  diff) {
  CONTAINER_DATA *data = objGetTypeData(obj, "container");
  data->pick_diff = diff;
}



//*****************************************************************************
// container olc
//*****************************************************************************
#define IEDIT_CONTAINER_CAPACITY      1
#define IEDIT_CONTAINER_KEY           2
#define IEDIT_CONTAINER_PICK_DIFF     3

// the resedit olc needs these declared
void iedit_container_menu   (SOCKET_DATA *sock, CONTAINER_DATA *data) {
  send_to_socket(sock, 
		 "{g1) Capacity : {c%1.2lf\r\n"
		 "{g2) Key      : {c%s\r\n"
		 "{g3) Closable : {c%s\r\n"
		 "{g4) Pick diff: {c%d\r\n",
		 data->capacity,
		 data->key,
		 (data->closable ? "yes" : "no"),
		 data->pick_diff);
}

int  iedit_container_chooser(SOCKET_DATA *sock, CONTAINER_DATA *data, 
			     const char *option) {
  switch(toupper(*option)) {
  case '1':
    text_to_buffer(sock, "Enter a new weight capacity for the container: ");
    return IEDIT_CONTAINER_CAPACITY;
  case '2':
    text_to_buffer(sock, "Enter akey vnum for the container: ");
    return IEDIT_CONTAINER_KEY;
  case '3':
    data->closable = (data->closable + 1) % 2;
    return MENU_NOCHOICE;
  case '4':
    text_to_buffer(sock, "How difficult is the lock to pick: ");
    return IEDIT_CONTAINER_PICK_DIFF;
  default: return MENU_CHOICE_INVALID;
  }
}

bool iedit_container_parser (SOCKET_DATA *sock, CONTAINER_DATA *data, int choice, 
			  const char *arg) {
  switch(choice) {
  case IEDIT_CONTAINER_CAPACITY: {
    double capacity = atof(arg);
    if(capacity <= 0)
      return FALSE;
    data->capacity = capacity;
    return TRUE;
  }
  case IEDIT_CONTAINER_KEY: {
    if(data->key) free(data->key);
    data->key = strdupsafe(arg);
    return TRUE;
  }
  case IEDIT_CONTAINER_PICK_DIFF: {
    int diff = atoi(arg);
    if(diff < 0 || !isdigit(*arg))
      return FALSE;
    data->pick_diff = diff;
    return TRUE;
  }
  default: return FALSE;
  }
}



//*****************************************************************************
// pyobj getters and setters
//*****************************************************************************
PyObject *PyObj_getcontainercapacity(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL)
    return NULL;
  else if(objIsType(obj, "container"))
    return Py_BuildValue("d", containerGetCapacity(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only get capacity for container.");
    return NULL;
  }
}

int PyObj_setcontainercapacity(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to set capacity for nonexistent "
		 "container, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "container")) {
    PyErr_Format(PyExc_TypeError, "Tried to set capacity for non-container, %s",
		 objGetClass(obj));
    return -1;
  }

  if(!PyFloat_Check(value)) {
    PyErr_Format(PyExc_TypeError, "container capacity must be a double.");
    return -1;
  }

  containerSetCapacity(obj, PyFloat_AsDouble(value));
  return 0;
}

PyObject *PyObj_getcontainerkey(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL)
    return NULL;
  else if(objIsType(obj, "container"))
    return Py_BuildValue("s", containerGetKey(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only get keys for containers.");
    return NULL;
  }
}

int PyObj_setcontainerkey(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA   *obj = PyObj_AsObj(self);
  const char *key = NULL;
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to set key for nonexistent "
		 "container, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "container")) {
    PyErr_Format(PyExc_TypeError, "Tried to set key for non-container, %s",
		 objGetClass(obj));
    return -1;
  }

  if(PyString_Check(value))
    key =get_fullkey(PyString_AsString(value),get_key_locale(objGetClass(obj)));
  else if(PyObj_Check(value)) {
    OBJ_DATA *key_obj = PyObj_AsObj(value);
    if(key_obj != NULL)
      key = objGetClass(key_obj);
    else {
      PyErr_Format(PyExc_TypeError, "Tried to set key for %s as nonexistaent "
		   "obj, %d", objGetClass(obj), PyObj_AsUid(value));
      return -1;
    }
  }

  // make sure we have a key
  if(key == NULL) {
    PyErr_Format(PyExc_TypeError, "Container keys must be strings or objects.");
    return -1;
  }

  containerSetKey(obj, key);
  return 0;
}

PyObject *PyObj_getcontainerclosable(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) return NULL;
  else if(objIsType(obj, "container"))
    return Py_BuildValue("i", containerIsClosable(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only check if containers are closable.");
    return NULL;
  }
}

PyObject *PyObj_getcontainerclosed  (PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) return NULL;
  else if(objIsType(obj, "container"))
    return Py_BuildValue("i", containerIsClosed(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only check if containers are closed.");
    return NULL;
  }
}

PyObject *PyObj_getcontainerlocked  (PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) return NULL;
  else if(objIsType(obj, "container"))
    return Py_BuildValue("i", containerIsLocked(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only check if containers are locked.");
    return NULL;
  }
}

PyObject *PyObj_getcontainerpickdiff(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) return NULL;
  else if(objIsType(obj, "container"))
    return Py_BuildValue("i", containerGetPicKDiff(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only check pick diffs on containers.");
    return NULL;
  }
}

int PyObj_setcontainerpickdiff(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to change pickdiff on nonexistent "
		 "container, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "container")) {
    PyErr_Format(PyExc_TypeError, "Tried to set pick for non-container, %s",
		 objGetClass(obj));
    return -1;
  }

  if(!PyInt_Check(value)) {
    PyErr_Format(PyExc_TypeError, "container pickdiff must be an integer.");
    return -1;
  }

  containerSetPickDiff(obj, PyInt_AsLong(value));
  return 0;
}

int PyObj_setcontainerclosable(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to change closable on nonexistent "
		 "container, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "container")) {
    PyErr_Format(PyExc_TypeError, "Tried to set closable for non-container, %s",
		 objGetClass(obj));
    return -1;
  }

  if(!PyInt_Check(value)) {
    PyErr_Format(PyExc_TypeError, "container closability must be a boolean.");
    return -1;
  }

  containerSetClosable(obj, PyInt_AsLong(value));
  if(PyInt_AsLong(value) == 0) {
    containerSetLocked(obj, FALSE);
    containerSetClosed(obj, FALSE);
  }
  return 0;
}

int PyObj_setcontainerclosed  (PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to change closed on nonexistent "
		 "container, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "container")) {
    PyErr_Format(PyExc_TypeError, "Tried to set closed for non-container, %s",
		 objGetClass(obj));
    return -1;
  }

  if(!PyInt_Check(value)) {
    PyErr_Format(PyExc_TypeError, "container close status must be a boolean.");
    return -1;
  }

  containerSetClosed(obj, PyInt_AsLong(value));
  if(PyInt_AsLong(value) == 0)
    containerSetLocked(obj, FALSE);
  else
    containerSetClosable(obj, TRUE);
  return 0;
}

int PyObj_setcontainerlocked  (PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to change locked on nonexistent "
		 "container, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "container")) {
    PyErr_Format(PyExc_TypeError, "Tried to set locked for non-container, %s",
		 objGetClass(obj));
    return -1;
  }

  if(!PyInt_Check(value)) {
    PyErr_Format(PyExc_TypeError, "container lock status must be a boolean.");
    return -1;
  }

  containerSetLocked(obj, PyInt_AsLong(value));
  if(PyInt_AsLong(value) != 0) {
    containerSetClosable(obj, TRUE);
    containerSetClosed(obj, TRUE);
  }
  return 0;
}

void container_to_proto(CONTAINER_DATA *data, BUFFER *buf) {
  if(data->capacity > 0)
    bprintf(buf, "me.container_capacity    = %1.3lf\n", data->capacity);
  if(*data->key)
    bprintf(buf, "me.container_key         = \"%s\"\n", data->key);
  if(data->pick_diff > 0)
    bprintf(buf, "me.container_pick_diff   = %d\n", data->pick_diff);
  if(data->closable == TRUE)
    bprintf(buf, "me.container_is_closable = True\n");
}



//*****************************************************************************
// hooks
//*****************************************************************************
void container_append_hook(const char *info) {
  OBJ_DATA *obj = NULL;
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &obj, &ch);

  if(objIsType(obj, "container")) {
    bprintf(charGetLookBuffer(ch), " It is %s%s.", 
	    (containerIsClosed(obj) ? "closed":"open"),
	    (containerIsLocked(obj) ? " and locked" : ""));
  }
}

void container_look_hook(const char *info) {
  OBJ_DATA *obj = NULL;
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &obj, &ch);

  if(objIsType(obj, "container") && !containerIsClosed(obj)) {
    LIST *vis_contents = find_all_objs(ch, objGetContents(obj), "", 
				       NULL, TRUE);
      // make sure we can still see things
      if(listSize(vis_contents) > 0) {
	send_to_char(ch, "It contains:\r\n");
	show_list(ch, vis_contents, objGetName, objGetMultiName);
      }
      deleteList(vis_contents);
  }
}



//*****************************************************************************
// install the container item type
//*****************************************************************************

//
// this will need to be called by init_items() in items/items.c
void init_container(void) {
  item_add_type("container", 
  		newContainerData, deleteContainerData,
  		containerDataCopyTo, containerDataCopy, 
  		containerDataStore, containerDataRead);

  // add our hooks
  hookAdd("append_obj_desc", container_append_hook);
  hookAdd("look_at_obj",     container_look_hook);

  // set up the container OLC too
  item_add_olc("container", iedit_container_menu, iedit_container_chooser, 
  	       iedit_container_parser, NULL, container_to_proto);

  // set up our Python extensions
  PyObj_addGetSetter("container_capacity",
    PyObj_getcontainercapacity, PyObj_setcontainercapacity,
    "The maximum amount of weight c container can hold.");
  PyObj_addGetSetter("container_key", 
    PyObj_getcontainerkey, PyObj_setcontainerkey,
    "An object prototype that acts as a key for this container.");
  PyObj_addGetSetter("container_pick_diff",
    PyObj_getcontainerpickdiff, PyObj_setcontainerpickdiff,
    "An integer representing how difficult a container's lock is to pick.");
  PyObj_addGetSetter("container_is_closable", 
    PyObj_getcontainerclosable, PyObj_setcontainerclosable,
    "True or False if the container can be closed.");
  PyObj_addGetSetter("container_is_closed", 
    PyObj_getcontainerclosed, PyObj_setcontainerclosed,
    "True or False if the container is closed.");
  PyObj_addGetSetter("container_is_locked", 
    PyObj_getcontainerlocked, PyObj_setcontainerlocked,
    "True or False if the container is locked.");
}
