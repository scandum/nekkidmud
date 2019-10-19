//*****************************************************************************
//
// room_reset.c
//
// Personally, I prefer to do all of my zone resetting through scripts because
// of the flexibility it allows for. However, I know some people are not
// too inclined towards scripting, so this is an effort to provide an easy
// way for those people to handle room resets.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "storage.h"
#include "room.h"
#include "world.h"
#include "zone.h"
#include "character.h"
#include "body.h"
#include "object.h"
#include "exit.h"
#include "handler.h"
#include "prototype.h"
#include "hooks.h"
#include "room_reset.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "items/items.h"
#include "items/container.h"
#include "items/furniture.h"
#include "items/worn.h"
#include "scripts/scripts.h"
#include "scripts/pyplugs.h"



//*****************************************************************************
// reset list
//*****************************************************************************
struct reset_list {
  char       *key; // our key in the world
  LIST    *resets; // our list of resets
};

RESET_LIST *newResetList(void) {
  RESET_LIST *list = malloc(sizeof(RESET_LIST));
  list->resets = newList();
  list->key    = strdup("");
  return list;
}

void deleteResetList(RESET_LIST *list) {
  if(list->resets) deleteListWith(list->resets, deleteReset);
  if(list->key)    free(list->key);
  free(list);
}

RESET_LIST *resetListCopy(RESET_LIST *list) {
  RESET_LIST *newlist = malloc(sizeof(RESET_LIST));
  if(list->resets) newlist->resets = listCopyWith(list->resets, resetCopy);
  else             newlist->resets = newList();
  newlist->key =   strdupsafe(list->key);
  return newlist;
}

void resetListCopyTo(RESET_LIST *from, RESET_LIST *to) {
  if(to->resets)   deleteListWith(to->resets, deleteReset);
  if(from->resets) to->resets = listCopyWith(from->resets, resetCopy);
  else             to->resets = newList();
  if(to->key)      free(to->key);
  to->key =        strdupsafe(from->key);
}

STORAGE_SET *resetListStore(RESET_LIST *list) {
  STORAGE_SET *set = new_storage_set();
  store_list(set, "resets", gen_store_list(list->resets, resetStore));
  return set;
}

RESET_LIST *resetListRead(STORAGE_SET *set) {
  RESET_LIST *list = calloc(1, sizeof(RESET_LIST));
  list->resets = gen_read_list(read_list(set, "resets"), resetRead);
  return list;
}

LIST *resetListGetResets(RESET_LIST *list) {
  return list->resets;
}

void resetListAdd(RESET_LIST *list, RESET_DATA *reset) {
  listPut(list->resets, reset);
}

void resetListRemove(RESET_LIST *list, RESET_DATA *reset) {
  listRemove(list->resets, reset);
}

void resetListSetKey(RESET_LIST *list, const char *key) {
  if(list->key) free(list->key);
  list->key = strdupsafe(key);
}

const char *resetListGetKey(RESET_LIST *list) {
  return list->key;
}



//*****************************************************************************
// reset data
//*****************************************************************************
struct reset_data {
  int        type; // what kind of reset are we?
  int       times; // how many times should it be executed?
  int      chance; // what is our chance of success?
  int         max; // what is the max number of us that can be in the game?
  int    room_max; // what is the max number of us that can be in the room?
  BUFFER     *arg; // what is our reset arg (e.g. mob proto, direction name)
  LIST        *in; // what resets do we put into ourself?
  LIST        *on; // what resets do we put onto ourself?
  LIST      *then; // if this succeeds, what else do we do?
};

const char *reset_names[NUM_RESETS] = {
  "load object",
  "load mobile",
  "find object",
  "find mobile",
  "purge object",
  "purge mobile",
  "open exit or object",
  "close exit or object",
  "lock exit or object",
  "change mobile position",
  "run script"
};


const char     *resetTypeGetName (int type) {
  return reset_names[type];
}


RESET_DATA    *newReset         () {
  RESET_DATA *reset = calloc(1, sizeof(RESET_DATA));
  reset->type     = RESET_LOAD_OBJECT;
  reset->arg      = newBuffer(1);
  reset->times    = 1;
  reset->chance   = 100;
  reset->max      = 0;
  reset->room_max = 0;
  reset->in       = newList();
  reset->on       = newList();
  reset->then     = newList();
  return reset;
}


void           deleteReset      (RESET_DATA *reset) {
  // delete all that are attached to us
  deleteListWith(reset->in,   deleteReset);
  deleteListWith(reset->on,   deleteReset);
  deleteListWith(reset->then, deleteReset);
  deleteBuffer(reset->arg);
  free(reset);
}


void           resetCopyTo      (RESET_DATA *from, RESET_DATA *to) {
  // first, delete everything attached to us
  deleteListWith(to->in,   deleteReset);
  deleteListWith(to->on,   deleteReset);
  deleteListWith(to->then, deleteReset);

  to->in   = listCopyWith(from->in,   resetCopy);
  to->on   = listCopyWith(from->on,   resetCopy);
  to->then = listCopyWith(from->then, resetCopy);

  resetSetType(to,        resetGetType(from));
  resetSetTimes(to,       resetGetTimes(from));
  resetSetChance(to,      resetGetChance(from));
  resetSetMax(to,         resetGetMax(from));
  resetSetRoomMax(to,     resetGetRoomMax(from));
  resetSetArg(to,         resetGetArg(from));
}


RESET_DATA    *resetCopy        (RESET_DATA *reset) {
  RESET_DATA *newreset = newReset();
  resetCopyTo(reset, newreset);
  return newreset;
}


STORAGE_SET   *resetStore       (RESET_DATA *reset) {
  STORAGE_SET *set = new_storage_set();
  store_int   (set, "type",     reset->type);
  store_int   (set, "times",    reset->times);
  store_int   (set, "chance",   reset->chance);
  store_int   (set, "max",      reset->max);
  store_int   (set, "room_max", reset->room_max);
  store_string(set, "arg",      bufferString(reset->arg));
  store_list  (set, "in",       gen_store_list(reset->in,   resetStore));
  store_list  (set, "on",       gen_store_list(reset->on,   resetStore));
  store_list  (set, "then",     gen_store_list(reset->then, resetStore));
  return set;
}


RESET_DATA    *resetRead        (STORAGE_SET *set) {
  RESET_DATA *reset = malloc(sizeof(RESET_DATA));
  reset->type     = read_int(set, "type");
  reset->times    = read_int(set, "times");
  reset->chance   = read_int(set, "chance");
  reset->max      = read_int(set, "max");
  reset->room_max = read_int(set, "room_max");
  reset->on       = gen_read_list(read_list(set, "on"),   resetRead);
  reset->in       = gen_read_list(read_list(set, "in"),   resetRead);
  reset->then     = gen_read_list(read_list(set, "then"), resetRead);
  reset->arg = newBuffer(1);
  bufferCat(reset->arg, read_string(set, "arg"));
  return reset;
}


int            resetGetType     (const RESET_DATA *reset) {
  return reset->type;
}

int            resetGetTimes    (const RESET_DATA *reset) {
  return reset->times;
}

int            resetGetChance   (const RESET_DATA *reset) {
  return reset->chance;
}

int            resetGetMax      (const RESET_DATA *reset) {
  return reset->max;
}

int            resetGetRoomMax  (const RESET_DATA *reset) {
  return reset->room_max;
}

const char    *resetGetArg      (const RESET_DATA *reset) {
  return bufferString(reset->arg);
}

BUFFER *resetGetArgBuffer       (const RESET_DATA *reset) {
  return reset->arg;
}

LIST          *resetGetOn       (const RESET_DATA *reset) {
  return reset->on;
}

LIST          *resetGetIn       (const RESET_DATA *reset) {
  return reset->in;
}

LIST          *resetGetThen     (const RESET_DATA *reset) {
  return reset->then;
}

void           resetSetType     (RESET_DATA *reset, int type) {
  reset->type = type;
}

void           resetSetTimes    (RESET_DATA *reset, int times) {
  reset->times = times;
}

void           resetSetChance   (RESET_DATA *reset, int chance) {
  reset->chance = chance;
}

void           resetSetMax      (RESET_DATA *reset, int max) {
  reset->max = max;
}

void           resetSetRoomMax  (RESET_DATA *reset, int room_max) {
  reset->room_max = room_max;
}

void           resetSetArg      (RESET_DATA *reset, const char *arg) {
  bufferClear(reset->arg);
  bufferCat(reset->arg, arg);
}

void           resetAddOn       (RESET_DATA *reset, RESET_DATA *on) {
  listPut(reset->on, on);
}

void           resetAddIn       (RESET_DATA *reset, RESET_DATA *in) {
  listPut(reset->in, in);
}

void           resetAddThen     (RESET_DATA *reset, RESET_DATA *then) {
  listPut(reset->then, then);
}



//*****************************************************************************
// resetRun and all of its related functions
//*****************************************************************************

// needs to be declared...
bool resetRun(RESET_DATA *reset, void *initiator, int initiator_type,
	      const char *locale);

//
// Perform resetRun on all the reset commands in the list, using
// initiator and initiator_type
void resetRunOn(LIST *list, void *initiator, int initiator_type, 
		const char *locale) {
  if(listSize(list) > 0) {
    LIST_ITERATOR *list_i = newListIterator(list);
    RESET_DATA     *reset = NULL;
    ITERATE_LIST(reset, list_i)
      resetRun(reset, initiator, initiator_type, locale);
    deleteListIterator(list_i);
  }
}

//
// try performing an object load, based on the reset data we have
bool try_reset_load_object(RESET_DATA *reset, void *initiator, 
			   int initiator_type, const char *locale) {
  const char *fullkey = get_fullkey_relative(resetGetArg(reset), locale);
  PROTO_DATA   *proto = worldGetType(gameworld, "oproto", fullkey);
  // if there's no prototype, break out
  if(proto == NULL || protoIsAbstract(proto))
    return FALSE;

  // see if we're already at our max
  if(resetGetMax(reset) != 0 && 
     count_objs(NULL, object_list, NULL, fullkey, FALSE) >= 
     resetGetMax(reset))
    return FALSE;
  if(initiator_type == INITIATOR_ROOM && resetGetRoomMax(reset) != 0 &&
     (count_objs(NULL, roomGetContents(initiator), NULL, fullkey,
		 FALSE) >= resetGetRoomMax(reset)))
    return FALSE;

  OBJ_DATA *obj = protoObjRun(proto);
  if(obj == NULL)
    return FALSE;

  // to the room
  if(initiator_type == INITIATOR_ROOM)
    obj_to_room(obj, initiator);
  // inside of the object
  else if(initiator_type == INITIATOR_IN_OBJ)
    obj_to_obj(obj, initiator);
  // to inventory
  else if(initiator_type == INITIATOR_IN_MOB)
    obj_to_char(obj, initiator);
  // equip the mobile
  else if(initiator_type == INITIATOR_ON_MOB) {
    // see if we can equip it
    bool equipped = (objIsType(obj, "worn") && 
		     try_equip(initiator, obj, NULL, wornGetPositions(obj)));
    // we failed! Extract the object
    if(!equipped) {
      extract_obj(obj);
      return FALSE;
    }
  }

  // we're being loaded after a mobile was loaded... let's go to the same room
  else if(initiator_type == INITIATOR_THEN_MOB)
    obj_to_room(obj, charGetRoom(initiator));
  // we're being loaded after an object was loaded... let's go wherever it went
  else if(initiator_type == INITIATOR_THEN_OBJ) {
    if(objGetRoom(initiator))
      obj_to_room(obj, objGetRoom(initiator));
    else if(objGetContainer(initiator))
      obj_to_obj(obj, objGetContainer(initiator));
    else if(objGetCarrier(initiator))
      obj_to_char(obj, objGetCarrier(initiator));
    else if(objGetWearer(initiator)) {
      bool equipped = (objIsType(obj, "worn") && 
		       try_equip(objGetWearer(initiator), obj, NULL, 
				 wornGetPositions(obj)));
      // we failed! Extract the object
      if(!equipped) {
	extract_obj(obj);
	return FALSE;
      }
    }
  }
  // hmmm... we shouldn't get this far
  else {
    extract_obj(obj);
    return FALSE;
  }

  // now, run all of our stuff
  resetRunOn(reset->on,   obj, INITIATOR_ON_OBJ,   locale);
  resetRunOn(reset->in,   obj, INITIATOR_IN_OBJ,   locale);
  resetRunOn(reset->then, obj, INITIATOR_THEN_OBJ, locale);

  return TRUE;
}


//
// try performing a mobile load, based on the reset data we have
bool try_reset_load_mobile(RESET_DATA *reset, void *initiator, 
			   int initiator_type, const char *locale) {
  const char *fullkey = get_fullkey_relative(resetGetArg(reset), locale);
  PROTO_DATA   *proto = worldGetType(gameworld, "mproto", fullkey);
  // if there's no prototype, break out
  if(proto == NULL || protoIsAbstract(proto))
    return FALSE;

  // see if we're already at our max
  if(resetGetMax(reset) != 0 && 
     count_chars(NULL, mobile_list, NULL, fullkey, FALSE) >= resetGetMax(reset))
    return FALSE;
  if(initiator_type == INITIATOR_ROOM && resetGetRoomMax(reset) != 0 &&
     (count_chars(NULL, roomGetCharacters(initiator), NULL, fullkey,
		  FALSE) >= resetGetRoomMax(reset)))
    return FALSE;

  CHAR_DATA *mob = protoMobRun(proto);
  if(mob == NULL)
    return FALSE;

  // to the room
  if(initiator_type == INITIATOR_ROOM)
    char_to_room(mob, initiator);
  // to a seat
  else if(initiator_type == INITIATOR_ON_OBJ) {
    if(!objIsType(initiator, "furniture") || objGetRoom(initiator)==NULL) {
      extract_mobile(mob);
      return FALSE;
    }
    char_to_room(mob, objGetRoom(initiator));
    char_to_furniture(mob, initiator);
    charSetPos(mob, POS_SITTING);
  }
  // after an object
  else if(initiator_type == INITIATOR_THEN_OBJ) {
    if(objGetRoom(initiator) == NULL) {
      extract_mobile(mob);
      return FALSE;
    }
    char_to_room(mob, objGetRoom(initiator));
  }
  // after another mob
  else if(initiator_type == INITIATOR_THEN_MOB)
    char_to_room(mob, charGetRoom(initiator));
  // we shouldn't get this far
  else {
    extract_mobile(mob);
    return FALSE;
  }

  // now, run all of our followup stuff
  resetRunOn(reset->on,   mob, INITIATOR_ON_MOB,   locale);
  resetRunOn(reset->in,   mob, INITIATOR_IN_MOB,   locale);
  resetRunOn(reset->then, mob, INITIATOR_THEN_MOB, locale);

  return TRUE;
}


//
// handles "find" and "purge" in one function
bool try_reset_old_object(RESET_DATA *reset, void *initiator,int initiator_type,
			  int reset_cmd, const char *locale) {
  const char *fullkey = get_fullkey_relative(resetGetArg(reset), locale);
  OBJ_DATA       *obj = NULL;

  // is it the room?
  if(initiator_type == INITIATOR_ROOM)
    obj = find_obj(NULL, roomGetContents(initiator),  1, NULL, fullkey, FALSE);
  // is it in a container?
  else if(initiator_type == INITIATOR_IN_OBJ)
    obj = find_obj(NULL, objGetContents(initiator),   1, NULL, fullkey, FALSE);
  // is it in a person's inventory?
  else if(initiator_type == INITIATOR_IN_MOB)
    obj = find_obj(NULL, charGetInventory(initiator), 1, NULL, fullkey, FALSE);
  // is it in a person's equipment?
  else if(initiator_type == INITIATOR_ON_MOB) {
    LIST *eq = bodyGetAllEq(charGetBody(initiator));
    obj = find_obj(NULL, eq, 1, NULL, fullkey, FALSE);
    deleteList(eq);
  }

  // if we didn't find it, return false
  if(obj == NULL)
    return FALSE;

  // now, run our reset sscripts
  resetRunOn(reset->on,   obj, INITIATOR_ON_OBJ,   locale);
  resetRunOn(reset->in,   obj, INITIATOR_IN_OBJ,   locale);
  resetRunOn(reset->then, obj, INITIATOR_THEN_OBJ, locale);

  // if this is a purge and it wasn't our initiator, kill it
  // if we purge our initiator, we might run into some problems
  if(obj != initiator && reset_cmd == RESET_PURGE_OBJECT)
    extract_obj(obj);
    
  return TRUE;
}


//
// find an object
bool try_reset_find_object(RESET_DATA *reset, void *initiator, 
			   int initiator_type, const char *locale) {
  return try_reset_old_object(reset, initiator, initiator_type, 
			      RESET_FIND_OBJECT, locale);
}


//
// purge an object
bool try_reset_purge_object(RESET_DATA *reset, void *initiator, 
			    int initiator_type, const char *locale) {
  return try_reset_old_object(reset, initiator, initiator_type, 
			      RESET_PURGE_OBJECT, locale);
}


//
// handles "find" and "purge" in one function
bool try_reset_old_mobile(RESET_DATA *reset, void *initiator,int initiator_type,
			  int reset_cmd, const char *locale) {
  const char *fullkey = get_fullkey_relative(resetGetArg(reset), locale);
  CHAR_DATA      *mob = NULL;

  // is it the room?
  if(initiator_type == INITIATOR_ROOM)
    mob = find_char(NULL, roomGetCharacters(initiator),1, NULL, fullkey, FALSE);
  // is it furniture?
  else if(initiator_type == INITIATOR_ON_OBJ)
    mob = find_char(NULL, objGetUsers(initiator), 1, NULL, fullkey, FALSE);
  // after an object
  else if(initiator_type == INITIATOR_THEN_OBJ) {
    if(objGetRoom(initiator) == NULL)
      return FALSE;
    mob = find_char(NULL, roomGetCharacters(objGetRoom(initiator)), 1, NULL,
		    fullkey, FALSE);
  }
  // after another mob
  else if(initiator_type == INITIATOR_THEN_MOB)
    mob = find_char(NULL, roomGetCharacters(charGetRoom(initiator)), 1, NULL,
		    fullkey, FALSE);

  // if we didn't find it, return FALSE
  if(mob == NULL)
    return FALSE;

  // if we found it, do the reset of the commands
  resetRunOn(reset->on,   mob, INITIATOR_ON_MOB,   locale);
  resetRunOn(reset->in,   mob, INITIATOR_IN_MOB,   locale);
  resetRunOn(reset->then, mob, INITIATOR_THEN_MOB, locale);

  // if this is a purge and it wasn't our initiator, kill it
  // if we purge our initiator, we might run into some problems
  if(mob != initiator && reset_cmd == RESET_PURGE_MOBILE)
    extract_mobile(mob);

  return TRUE;
}


//
// find a mobile
bool try_reset_find_mobile(RESET_DATA *reset, void *initiator, 
			   int initiator_type, const char *locale) {
  return try_reset_old_mobile(reset, initiator, initiator_type, RESET_FIND_MOBILE, locale);
}


//
// purge a mobile
bool try_reset_purge_mobile(RESET_DATA *reset, void *initiator, 
			    int initiator_type, const char *locale) {
  return try_reset_old_mobile(reset, initiator, initiator_type, RESET_PURGE_MOBILE, locale);
}


//
// try forcing a mobile to change positions
bool try_reset_position(RESET_DATA *reset, void *initiator, int initiator_type){
  if(!initiator || initiator_type != INITIATOR_THEN_MOB)
    return FALSE;
  int pos = posGetNum(resetGetArg(reset));
  if(pos < 0 || pos >= NUM_POSITIONS)
    return FALSE;
  charSetPos(initiator, pos);
  return TRUE;
}


//
// the blanket function for reset_open/close/lock
bool try_reset_opening(RESET_DATA *reset, void *initiator, int initiator_type,
		       bool closed, bool locked) {
  // we're trying to open an exit
  if(initiator_type == INITIATOR_ROOM) {
    EXIT_DATA *exit = roomGetExit(initiator, resetGetArg(reset));
    int      dirnum = DIR_NONE;

    // are we using an abbreviation?
    if(exit == NULL && (dirnum = dirGetAbbrevNum(resetGetArg(reset)))!=DIR_NONE)
      exit = roomGetExit(initiator, dirGetName(dirnum));

    // did we find a valid exit?
    if(exit == NULL)
      return FALSE;

    exitSetLocked(exit, locked);
    exitSetClosed(exit, closed);
    return TRUE;
  }

  // we're trying to open a container
  else if(initiator_type == INITIATOR_THEN_OBJ) {
    if(!objIsType(initiator, "container"))
      return FALSE;
    containerSetLocked(initiator, locked);
    containerSetClosed(initiator, closed);
    return TRUE;
  }
  // we shouldn't get this far
  else
    return FALSE;
}


//
// try opening something
bool try_reset_open(RESET_DATA *reset, void *initiator, int initiator_type) {
  return try_reset_opening(reset, initiator, initiator_type, FALSE, FALSE);
}


//
// try closing and unlocking something 
bool try_reset_close(RESET_DATA *reset, void *initiator, int initiator_type) {
  return try_reset_opening(reset, initiator, initiator_type, TRUE, FALSE);
}


//
// Try closing and locking something
bool try_reset_lock(RESET_DATA *reset, void *initiator, int initiator_type) {
  return try_reset_opening(reset, initiator, initiator_type, TRUE, TRUE);
}

//
// Try running a script on the initiator
bool try_reset_script(RESET_DATA *reset, void *initiator, int initiator_type,
		      const char *locale) {
  PyObject *pyme = NULL;
  PyObject *dict = NULL;
  if(initiator_type == INITIATOR_ROOM)
    pyme = roomGetPyFormBorrowed(initiator);
  else if(initiator_type == INITIATOR_THEN_OBJ)
    pyme = objGetPyFormBorrowed(initiator);
  else if(initiator_type == INITIATOR_THEN_MOB)
    pyme = charGetPyFormBorrowed(initiator);
  else
    return FALSE;

  // build our dictionary and add ourself to it as 'me'
  dict = restricted_script_dict();
  PyDict_SetItemString(dict, "me", pyme);

  // run the script
  run_script(dict, resetGetArg(reset), locale);

  // check to see if we had an error
  if(!last_script_ok())
    log_pyerr("Reset script in locale %s terminated with an error:\r\n%s",
	      locale, resetGetArg(reset));

  // garbage collection and return our outcome
  Py_DECREF(dict);
  return last_script_ok();
}

//
// run the reset data
bool resetRun(RESET_DATA *reset, void *initiator, int initiator_type,
	      const char *locale) {
  //
  // possible problem: how do we know what to return if we're
  // running the reset data multiple times?
  bool ret_val = FALSE;

  // go through for however many times we need to
  int i;
  for(i = 0; i < resetGetTimes(reset); i++) {
    // If we don't make our reset chance, continue onto the next check
    if(rand_number(1, 100) > resetGetChance(reset))
      continue;
    switch(resetGetType(reset)) {
    case RESET_LOAD_OBJECT:
      ret_val = try_reset_load_object(reset, initiator, initiator_type, locale);
      break;
    case RESET_LOAD_MOBILE:
      ret_val = try_reset_load_mobile(reset, initiator, initiator_type, locale);
      break;
    case RESET_FIND_OBJECT:
      ret_val = try_reset_find_object(reset, initiator, initiator_type, locale);
      break;
    case RESET_FIND_MOBILE:
      ret_val = try_reset_find_mobile(reset, initiator, initiator_type, locale);
      break;
    case RESET_PURGE_OBJECT:
      ret_val = try_reset_purge_object(reset, initiator, initiator_type,locale);
      break;
    case RESET_PURGE_MOBILE:
      ret_val = try_reset_purge_mobile(reset, initiator, initiator_type,locale);
      break;
    case RESET_OPEN:
      ret_val = try_reset_open(reset, initiator, initiator_type);
      break;
    case RESET_CLOSE:
      ret_val = try_reset_close(reset, initiator, initiator_type);
      break;
    case RESET_LOCK:
      ret_val = try_reset_lock(reset, initiator, initiator_type);
      break;
    case RESET_POSITION:
      ret_val = try_reset_position(reset, initiator, initiator_type);
      break;
    case RESET_SCRIPT:
      ret_val = try_reset_script(reset, initiator, initiator_type, locale);
      break;
    default:
      return FALSE;
    }
  }
  return ret_val;
}



//*****************************************************************************
// initialization function
//*****************************************************************************

//
// run all of the resets for a specified room
void do_resets(ROOM_DATA *room) {
  // first apply all of our prototype resets
  LIST            *protos = parse_keywords(roomGetPrototypes(room));
  LIST_ITERATOR  *proto_i = newListIterator(protos);
  char             *proto = NULL;
  RESET_LIST        *list = NULL;

  // try to run each parent reset, and finally our own
  ITERATE_LIST(proto, proto_i) {
    if((list = worldGetType(gameworld, "reset", proto)) != NULL)
      resetRunOn(resetListGetResets(list), room, INITIATOR_ROOM, get_key_locale(proto));
  } deleteListIterator(proto_i);
  deleteListWith(protos, free);
}

//
// room reset hook. Whenever a room is reset, apply all of the reset rules for
// it and its parent.
void room_reset_hook(const char *info) {
  ROOM_DATA *room = NULL;
  hookParseInfo(info, &room);
  if(room != NULL)
    do_resets(room);
}

//
// zone reset hook. Whenever a zone is reset, apply all of its reset rules for
// each room in the zone.
void zone_reset_hook(const char *info) {
  char  *zone_key = NULL;
  hookParseInfo(info, &zone_key);
  ZONE_DATA *zone = worldGetZone(gameworld, zone_key);

  LIST_ITERATOR *res_i = newListIterator(zoneGetResettable(zone));
  char           *name = NULL;
  const char   *locale = zone_key;
  ROOM_DATA      *room = NULL;
  ITERATE_LIST(name, res_i) {
    if((room = worldGetRoom(gameworld, get_fullkey(name, locale))) != NULL) {
      do_resets(room);
    }
  } deleteListIterator(res_i);

  // garbage collection
  free(zone_key);
}

void init_room_reset(void) {
  hookAdd("reset_zone", zone_reset_hook);
  hookAdd("reset_room", room_reset_hook);
}
