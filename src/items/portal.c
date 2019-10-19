//*****************************************************************************
//
// portal.c
//
// handles all of the functioning of the portal item type. Stores data about 
// the destination vnums of a portal, and does all the legwork for retreiving 
// destinations when someone wants to interact with a portal.
//
//*****************************************************************************
#include "../mud.h"
#include "../utils.h"
#include "../storage.h"
#include "../character.h"
#include "../object.h"
#include "../socket.h"
#include "../room.h"
#include "../world.h"
#include "../inform.h"
#include "../handler.h"
#include "../hooks.h"
#include "items.h"

#include "../olc2/olc.h"
#include "iedit.h"
#include "portal.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pyobj.h"
#include "../scripts/pyroom.h"



//*****************************************************************************
// item data for portals
//*****************************************************************************
typedef struct portal_data {
  char        *dest;
  char  *leave_mssg;
  char  *enter_mssg;
} PORTAL_DATA;

PORTAL_DATA *newPortalData() {
  PORTAL_DATA *data = malloc(sizeof(PORTAL_DATA));
  data->dest       = strdup("");
  data->leave_mssg = strdup("");
  data->enter_mssg = strdup("");
  return data;
}

void deletePortalData(PORTAL_DATA *data) {
  if(data->dest)       free(data->dest);
  if(data->leave_mssg) free(data->leave_mssg);
  if(data->enter_mssg) free(data->enter_mssg);
  free(data);
}

void portalDataCopyTo(PORTAL_DATA *from, PORTAL_DATA *to) {
  if(to->dest)       free(to->dest);
  if(to->leave_mssg) free(to->leave_mssg);
  if(to->enter_mssg) free(to->enter_mssg);
  to->dest       = strdupsafe(from->dest);
  to->leave_mssg = strdupsafe(from->leave_mssg);
  to->enter_mssg = strdupsafe(from->enter_mssg);
  
}

PORTAL_DATA *portalDataCopy(PORTAL_DATA *data) {
  PORTAL_DATA *new_data = newPortalData();
  portalDataCopyTo(data, new_data);
  return new_data;
}

STORAGE_SET *portalDataStore(PORTAL_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "dest",  data->dest);
  store_string(set, "enter", data->enter_mssg);
  store_string(set, "leave", data->leave_mssg);
  return set;
}

PORTAL_DATA *portalDataRead(STORAGE_SET *set) {
  PORTAL_DATA *data = malloc(sizeof(PORTAL_DATA));
  data->dest        = strdupsafe(read_string(set, "dest"));
  data->enter_mssg  = strdupsafe(read_string(set, "enter"));
  data->leave_mssg  = strdupsafe(read_string(set, "leave"));
  return data;
}



//*****************************************************************************
// functions for interacting with portals
//*****************************************************************************
const char *portalGetDest(OBJ_DATA *obj) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  return data->dest;
}

const char *portalGetSmartDest(OBJ_DATA *obj) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  if(objGetRoom(obj))
    return get_fullkey_relative(data->dest, get_key_locale(roomGetClass(objGetRoom(obj))));
  return get_fullkey_relative(data->dest, get_key_locale(objGetClass(obj)));
}

const char *portalGetLeaveMssg(OBJ_DATA *obj) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  return data->leave_mssg;
}

const char *portalGetEnterMssg(OBJ_DATA *obj) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  return data->enter_mssg;
}

void portalSetDest(OBJ_DATA *obj, const char *dest) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  if(data->dest) free(data->dest);
  data->dest = strdupsafe(dest);
}

void portalSetLeaveMssg(OBJ_DATA *obj, const char *mssg) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  if(data->leave_mssg) free(data->leave_mssg);
  data->leave_mssg = strdupsafe(mssg);
}

void portalSetEnterMssg(OBJ_DATA *obj, const char *mssg) {
  PORTAL_DATA *data = objGetTypeData(obj, "portal");
  if(data->enter_mssg) free(data->enter_mssg);
  data->enter_mssg = strdupsafe(mssg);
}


//
// cmd_enter is used to go through portals
//   usage: enter <object>
//
//   examples:
//     enter portal         enter the thing called "portal" in your room
COMMAND(cmd_enter) {
  void *obj = NULL;

  if(!parse_args(ch, TRUE, cmd, arg, "obj.room", &obj))
    return;

  // we're trying to enter a portal
  if(!objIsType(obj, "portal"))
    send_to_char(ch, "You cannot seem to find an entrance.\r\n");
  else {
    ROOM_DATA *dest = worldGetRoom(gameworld, portalGetSmartDest(obj));
    //get_fullkey_relative(portalGetDest(obj), get_key_locale(objGetClass(obj))));
    if(dest == NULL)
      send_to_char(ch, "There is nothing on the other side...\r\n");
    else {
      if(*portalGetLeaveMssg(obj))
	message(ch, NULL, obj,NULL,TRUE,TO_ROOM, portalGetLeaveMssg(obj));
      else
	message(ch, NULL, obj, NULL, TRUE, TO_ROOM, "$n steps into $o.");
      
      // transfer our character and look
      char_from_room(ch);
      char_to_room(ch, dest);
      look_at_room(ch, dest);
      
      if(*portalGetEnterMssg(obj))
	message(ch, NULL, obj,NULL,TRUE,TO_ROOM, portalGetEnterMssg(obj));
      else
	message(ch, NULL, obj, NULL, TRUE, TO_ROOM,
		"$n arrives after travelling through $o.");
      hookRun("enter_portal", hookBuildInfo("ch obj", ch, obj));
      hookRun("enter", hookBuildInfo("ch rm", ch, dest));
    }
  }
}



//*****************************************************************************
// portal olc
//*****************************************************************************
#define IEDIT_PORTAL_DEST      1
#define IEDIT_PORTAL_ENTER     2
#define IEDIT_PORTAL_LEAVE     3

// the resedit olc needs these declared
void iedit_portal_menu(SOCKET_DATA *sock, PORTAL_DATA *data) {
  send_to_socket(sock, "{g1) Destination  : {c%s\r\n", data->dest);
  send_to_socket(sock, "{g2) Enter message:\r\n{c%s\r\n", data->enter_mssg);
  send_to_socket(sock, "{g3) Leave message:\r\n{c%s\r\n", data->leave_mssg);
}

int  iedit_portal_chooser(SOCKET_DATA *sock, PORTAL_DATA *data, 
			  const char *option) {
  switch(toupper(*option)) {
  case '1': 
    text_to_buffer(sock, "Enter new destination (return for none): ");
    return IEDIT_PORTAL_DEST;
  case '2':
    text_to_buffer(sock, "Enter message shown to room user arrives at: ");
    return IEDIT_PORTAL_ENTER;
  case '3':
    text_to_buffer(sock, "Enter message shown to room user leaves: ");
    return IEDIT_PORTAL_LEAVE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool iedit_portal_parser (SOCKET_DATA *sock, PORTAL_DATA *data, int choice, 
			  const char *arg) {
  switch(choice) {
  case IEDIT_PORTAL_DEST: {
    if(data->dest) free(data->dest);
    data->dest   = strdupsafe(arg);
    return TRUE;
  }
  case IEDIT_PORTAL_LEAVE: {
    if(data->leave_mssg) free(data->leave_mssg);
    data->leave_mssg = strdupsafe(arg);
    return TRUE;
  }
  case IEDIT_PORTAL_ENTER: {
    if(data->enter_mssg) free(data->enter_mssg);
    data->enter_mssg = strdupsafe(arg);
    return TRUE;
  }
  default:
    return FALSE;
  }
}

void portal_to_proto(PORTAL_DATA *data, BUFFER *buf) {
  if(*data->dest)
    bprintf(buf, "me.portal_dest = \"%s\"\n", data->dest);
  if(*data->leave_mssg)
    bprintf(buf, "me.portal_leave_mssg = \"%s\"\n", data->leave_mssg);
  if(*data->enter_mssg)
    bprintf(buf, "me.portal_enter_mssg = \"%s\"\n", data->enter_mssg);
}



//*****************************************************************************
// pyobj getters and setters
//*****************************************************************************
PyObject *PyObj_getportaldest(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL)
    return NULL;
  else if(objIsType(obj, "portal"))
    return Py_BuildValue("s", portalGetSmartDest(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only get destination for portals.");
    return NULL;
  }
}

PyObject *PyObj_getportalleavemssg(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL)
    return NULL;
  else if(objIsType(obj, "portal"))
    return Py_BuildValue("s", portalGetLeaveMssg(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only get leave message for portals.");
    return NULL;
  }
}

PyObject *PyObj_getportalentermssg(PyObject *self, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL)
    return NULL;
  else if(objIsType(obj, "portal"))
    return Py_BuildValue("s", portalGetEnterMssg(obj));
  else {
    PyErr_Format(PyExc_TypeError, "Can only get enter message for portals.");
    return NULL;
  }
}

int PyObj_setportaldest(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to set destination for "
		 "nonexistent portal, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "portal")) {
    PyErr_Format(PyExc_TypeError, "Tried to set destination for non-portal, %s",
		 objGetClass(obj));
    return -1;
  }

  if(PyString_Check(value))
    portalSetDest(obj, PyString_AsString(value));
  else if(PyRoom_Check(value))
    portalSetDest(obj, roomGetClass(PyRoom_AsRoom(value)));
  else {
    PyErr_Format(PyExc_TypeError, "portal dest must be a room or string.");
    return -1;
  }

  return 0;
}

int PyObj_setportalleavemssg(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to set leave message for "
		 "nonexistent portal, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "portal")) {
    PyErr_Format(PyExc_TypeError, "Tried to set leave mssg for non-portal, %s",
		 objGetClass(obj));
    return -1;
  }

  if(PyString_Check(value))
    portalSetLeaveMssg(obj, PyString_AsString(value));
  else {
    PyErr_Format(PyExc_TypeError, "portal leave message must be a string.");
    return -1;
  }

  return 0;
}

int PyObj_setportalentermssg(PyObject *self, PyObject *value, void *closure) {
  OBJ_DATA *obj = PyObj_AsObj(self);
  if(obj == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to set enter message for "
		 "nonexistent portal, %d", PyObj_AsUid(self));
    return -1;
  }
  else if(!objIsType(obj, "portal")) {
    PyErr_Format(PyExc_TypeError, "Tried to set enter mssg for non-portal, %s",
		 objGetClass(obj));
    return -1;
  }

  if(PyString_Check(value))
    portalSetEnterMssg(obj, PyString_AsString(value));
  else {
    PyErr_Format(PyExc_TypeError, "portal enter message must be a string.");
    return -1;
  }

  return 0;
}
  


//*****************************************************************************
// add our hookds
//*****************************************************************************
void portal_look_hook(const char *info) {
  OBJ_DATA *obj = NULL;
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &obj, &ch);

  if(objIsType(obj, "portal")) {
    ROOM_DATA *dest = worldGetRoom(gameworld, portalGetSmartDest(obj));
    //get_fullkey_relative(portalGetDest(obj), get_key_locale(objGetClass(obj))));
    if(dest != NULL) {
      send_to_char(ch, "You peer inside %s:\r\n\r\n", see_obj_as(ch, obj));
      look_at_room(ch, dest);
    }
  }
}



//*****************************************************************************
// install the portal item type
//*****************************************************************************

//
// this will need to be called by init_items() in items/items.c
void init_portal(void) {
  item_add_type("portal", 
		newPortalData, deletePortalData,
		portalDataCopyTo, portalDataCopy, 
		portalDataStore, portalDataRead);

  // set up our hooks
  hookAdd("look_at_obj", portal_look_hook);

  // set up the portal OLC too
  item_add_olc("portal", iedit_portal_menu, iedit_portal_chooser, 
	       iedit_portal_parser, NULL, portal_to_proto);

  // make it so we can set portal destinations in scripts
  PyObj_addGetSetter("portal_dest", PyObj_getportaldest, PyObj_setportaldest,
    "A string key specifying the destination of the portal. Can be set by\n"
    "a string or the actual room.");
  PyObj_addGetSetter("portal_enter_mssg", 
    PyObj_getportalentermssg, PyObj_setportalentermssg,
    "The message shown to the desination room when a character enters portal.");
  PyObj_addGetSetter("portal_leave_mssg", 
    PyObj_getportalleavemssg, PyObj_setportalleavemssg,
    "The message shown to a room after a character leaves it via a portal.");

  add_cmd("enter", NULL, cmd_enter, "player", TRUE);
  add_cmd_check("enter", chk_can_move);
}
