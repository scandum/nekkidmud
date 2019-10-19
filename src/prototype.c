//*****************************************************************************
//
// prototype.c
//
// generic prototype datastructure for anything that can be generated with a
// python script (e.g. object, room, character). Supports inheritance from
// other prototypes.
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "prototype.h"
#include "storage.h"
#include "room.h"
#include "character.h"
#include "object.h"
#include "world.h"
#include "zone.h"
#include "handler.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/scripts.h"
#include "scripts/pyplugs.h"
#include "scripts/pychar.h"
#include "scripts/pyroom.h"
#include "scripts/pyobj.h"



//*****************************************************************************
// local datastructures, functions, and variables
//*****************************************************************************
struct prototype_data {
  char       *key;
  char   *parents;
  bool   abstract;
  BUFFER  *script;
  PyObject  *code;
};



//*****************************************************************************
// implementation of prototype.h
//*****************************************************************************
PROTO_DATA *newProto(void) {
  PROTO_DATA *data = malloc(sizeof(PROTO_DATA));
  data->key      = strdup("");
  data->parents  = strdup("");
  data->abstract = TRUE;
  data->script   = newBuffer(1);
  data->code     = NULL;
  return data;
}

void deleteProto(PROTO_DATA *data) {
  if(data->key)     free(data->key);
  if(data->parents) free(data->parents);
  if(data->script)  deleteBuffer(data->script);
  Py_XDECREF(data->code);
  free(data);
}

void protoCopyTo(PROTO_DATA *from, PROTO_DATA *to) {
  protoSetKey(to,      protoGetKey(from));
  protoSetParents(to,  protoGetParents(from));
  protoSetScript(to,   protoGetScript(from));
  protoSetAbstract(to, protoIsAbstract(from));
  Py_XDECREF(to->code);
  Py_XINCREF(from->code);
  to->code = from->code;
}

PROTO_DATA *protoCopy(PROTO_DATA *data) {
  PROTO_DATA *newproto = newProto();
  protoCopyTo(data, newproto);
  return newproto;
}

STORAGE_SET   *protoStore(PROTO_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "parents",  data->parents);
  store_bool  (set, "abstract", data->abstract);
  store_string(set, "script",   bufferString(data->script));
  return set;
}

PROTO_DATA *protoRead(STORAGE_SET *set) {
  PROTO_DATA *data = newProto();
  protoSetParents(data,  read_string(set, "parents"));
  protoSetAbstract(data, read_bool  (set, "abstract"));
  protoSetScript(data,   read_string(set, "script"));
  return data;
}

void protoSetKey(PROTO_DATA *data, const char *key) {
  if(data->key) free(data->key);
  data->key = strdupsafe(key);
}

void  protoSetParents(PROTO_DATA *data, const char *parents) {
  if(data->parents) free(data->parents);
  data->parents = strdupsafe(parents);
}

void   protoSetScript(PROTO_DATA *data, const char *script) {
  bufferClear(data->script);
  bufferCat(data->script, script);
  Py_XDECREF(data->code);
  data->code = NULL;
}

void protoSetAbstract(PROTO_DATA *data, bool abstract) {
  data->abstract = abstract;
}

const char *protoGetKey(PROTO_DATA *data) {
  return data->key;
}

const char  *protoGetParents(PROTO_DATA *data) {
  return data->parents;
}

bool protoIsAbstract(PROTO_DATA *data) {
  return data->abstract;
}

const char   *protoGetScript(PROTO_DATA *data) {
  return bufferString(data->script);
}

BUFFER *protoGetScriptBuffer(PROTO_DATA *data) {
  return data->script;
}

bool protoRunAs(PROTO_DATA *proto, const char *type, const char *as, 
		void *pynewfunc, void *protoaddfunc, void *protoclassfunc, 
		void *me) {
  // parse and run all of our parents
  LIST           *parents = parse_keywords(proto->parents);
  LIST_ITERATOR *parent_i = newListIterator(parents);
  char        *one_parent = NULL;
  bool         parents_ok = TRUE;

  // try to run each parent
  ITERATE_LIST(one_parent, parent_i) {
    PROTO_DATA *parent = NULL;
    // does our parent have a locale? If so, find it. If not, use ours
    int separator_pos = next_letter_in(one_parent, '@');
    if(separator_pos == -1)
      parent = worldGetType(gameworld, type, 
			    get_fullkey(one_parent,get_key_locale(proto->key)));
    else
      parent = worldGetType(gameworld, type, one_parent);
    
    if(parent == NULL) {
      log_string("ERROR: could not find parent %s for %s %s.", one_parent,
		 type, protoGetKey(proto));
      parents_ok = FALSE;
    }
    else if(!protoRun(parent, type, pynewfunc, protoaddfunc, protoclassfunc,me))
      parents_ok = FALSE;

    // if we had a problem running the proto, report it
    if(parents_ok == FALSE)
      break;
  } deleteListIterator(parent_i);

  // do garbage collection for our parent list
  deleteListWith(parents, free);

  // did we encounter a problem w/ our parents?
  if(parents_ok == FALSE)
    return FALSE;

  // now, do us
  PyObject *dict = restricted_script_dict();
  PyObject *pyme = ((PyObject *(*)(void *))pynewfunc)(me);
  if(protoaddfunc)
    ((void (*)(void *, const char *))protoaddfunc)(me, protoGetKey(proto));
  if(protoclassfunc)
    ((void (*)(void *, const char *))protoclassfunc)(me, as);

  PyDict_SetItemString(dict, "me", pyme);

  // do we have our own code already, or do we need to compile from source?
  if(proto->code == NULL) {
    proto->code = run_script_forcode(dict, bufferString(proto->script), 
				     get_key_locale(as));
  }
  // we already have a code object. Evaluate it.
  else {
    run_code(proto->code, dict, get_key_locale(as));
    
    if(!last_script_ok())
      log_pyerr("Prototype %s terminated with an error:\r\n%s",
		proto->key, bufferString(proto->script));
  }

  // remove us from the dictionary just incase it doesn't GC immediately. It
  // happens sometimes if we define a new method in the prototype
  PyDict_DelItemString(dict, "me");
  // PyDict_SetItemString(dict, "me", Py_None);

  // garbage collection
  Py_DECREF(dict);
  // Py_DECREF(pyme);
  return last_script_ok();
}

bool protoRun(PROTO_DATA *proto, const char *type, void *pynewfunc, 
	      void *protoaddfunc, void *protoclassfunc, void *me) {
  const char *as = protoGetKey(proto);
  return protoRunAs(proto,type,as,pynewfunc,protoaddfunc,protoclassfunc,me);
}

CHAR_DATA *protoMobRun(PROTO_DATA *proto) {
  if(protoIsAbstract(proto))
    return NULL;
  CHAR_DATA *ch = newMobile();
  char_exist(ch);
  if(protoRun(proto, "mproto", charGetPyFormBorrowed, charAddPrototype, charSetClass, ch))
    char_to_game(ch);
  else {
    // should this be char_unexist? Check to see what difference it makes
    extract_mobile(ch);
    ch = NULL;
  }

  return ch;
}

OBJ_DATA *protoObjRun(PROTO_DATA *proto) {
  if(protoIsAbstract(proto))
    return NULL;
  OBJ_DATA *obj = newObj();
  obj_exist(obj);
  if(protoRun(proto, "oproto", objGetPyFormBorrowed, objAddPrototype, objSetClass, obj))
    obj_to_game(obj);
  else {
    // should this be obj_unexist? Check to see what difference it makes
    extract_obj(obj);
    obj = NULL;
  }

  return obj;
}

ROOM_DATA *protoRoomRun(PROTO_DATA *proto) {
  if(protoIsAbstract(proto))
    return NULL;
  ROOM_DATA *room = newRoom();
  room_exist(room);
  if(protoRun(proto, "rproto", roomGetPyFormBorrowed, roomAddPrototype,roomSetClass,room))
    room_to_game(room);
  else {
    // should this be room_unexist? Check to see what difference it makes
    extract_room(room);
    room = NULL;
  }

  return room;
}

ROOM_DATA *protoRoomInstance(PROTO_DATA *proto, const char *as) {
  if(protoIsAbstract(proto))
    return NULL;
  ROOM_DATA *room = newRoom();
  room_exist(room);
  if(protoRun(proto, "rproto", roomGetPyFormBorrowed, roomAddPrototype, 
	      roomSetClass, room)) {
    roomSetClass(room, as);
    room_to_game(room);
  }
  else {
    // should this be room_unexist? Check to see what difference it makes
    extract_room(room);
    room = NULL;
  }

  return room;
}
