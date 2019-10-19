//*****************************************************************************
//
// trighooks.c
//
// Triggers attach on to rooms, objects, and mobiles as hooks. When a hook
// event occurs, all of the triggers of the right type will run. This header is
// just to allow scripts to initialize the hooks into the game. The init 
// function here should not be touched by anything other than scripts.c
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../hooks.h"
#include "../character.h"
#include "../room.h"
#include "../object.h"
#include "../world.h"
#include "../zone.h"

#include "scripts.h"
#include "pychar.h"
#include "pyobj.h"
#include "pyroom.h"
#include "pyexit.h"
#include "pymudsys.h"
#include "trighooks.h"



//*****************************************************************************
// local datastructures and defines
//*****************************************************************************

// a table matching trigger types to what they can attach to (obj, mob, room)
HASHTABLE *tedit_opts = NULL;

// used for providing additional variables to gen_do_trig that are not standard
struct opt_var {
  char *name;
  void *data;
  int   type;
};

OPT_VAR *newOptVar(const char *name, void *data, int type) {
  OPT_VAR *var = malloc(sizeof(OPT_VAR));
  var->name    = strdupsafe(name);
  var->data    = data;
  var->type    = type;
  return var;
}

void deleteOptVar(OPT_VAR *var) {
  if(var->name) free(var->name);
  free(var);
}

HASHTABLE *get_tedit_opts(void) {
  return tedit_opts;
}

void register_tedit_opt(const char *type, const char *desc) {
  if(tedit_opts == NULL)
    tedit_opts = newHashtable();
  hashPut(tedit_opts, type, strdupsafe(desc));
}



//*****************************************************************************
// trigger handlers
//*****************************************************************************

//
// generalized function for setting up a dictionary and running a trigger. The
// common types of variables can be supplied in the function. Additional ones
// can be added in the optional list, which must be deleted after use
void gen_do_trig(TRIGGER_DATA *trig, 
		 void *me, int me_type, CHAR_DATA *ch, OBJ_DATA *obj,
		 ROOM_DATA *room, EXIT_DATA *exit, const char *command,
		 const char *arg, LIST *optional) {
  // make our basic dictionary, and fill it up with these new variables
  PyObject *dict = restricted_script_dict();
  LIST *varnames = newList(); 
  // now, import all of our variables
  if(command) {
    PyObject *pycmd = PyString_FromString(command);
    PyDict_SetItemString(dict, "cmd", pycmd);
    listPut(varnames, strdup("cmd"));
    Py_DECREF(pycmd);
  }
  if(arg) {
    PyObject *pyarg = PyString_FromString(arg);
    PyDict_SetItemString(dict, "arg", pyarg);
    listPut(varnames, strdup("arg"));
    Py_DECREF(pyarg);
  }
  if(ch) {
    PyObject *pych = charGetPyForm(ch);
    PyDict_SetItemString(dict, "ch", pych);
    listPut(varnames, strdup("ch"));
    Py_DECREF(pych);
  }
  if(room) {
    PyObject *pyroom = roomGetPyForm(room);
    PyDict_SetItemString(dict, "room", pyroom);
    listPut(varnames, strdup("room"));
    Py_DECREF(pyroom);
  }    
  if(obj) {
    PyObject *pyobj = objGetPyForm(obj);
    PyDict_SetItemString(dict, "obj", pyobj);
    listPut(varnames, strdup("obj"));
    Py_DECREF(pyobj);
  }
  if(exit) {
    PyObject *pyexit = newPyExit(exit);
    PyDict_SetItemString(dict, "ex", pyexit);
    listPut(varnames, strdup("ex"));
    Py_DECREF(pyexit);
  }

  // add the thing the trigger is attached to
  if(me) {
    PyObject *pyme = NULL;
    switch(me_type) {
    case TRIGVAR_CHAR:  pyme = charGetPyForm(me); break;
    case TRIGVAR_OBJ:   pyme = objGetPyForm(me);  break;
    case TRIGVAR_ROOM:  pyme = roomGetPyForm(me); break;
    }
    PyDict_SetItemString(dict, "me", pyme);
    listPut(varnames, strdup("me"));
    Py_DECREF(pyme);
  }

  // now, add any optional variables
  if(optional) {
    LIST_ITERATOR *opt_i = newListIterator(optional);
    OPT_VAR         *opt = NULL;
    PyObject      *pyopt = NULL;
    ITERATE_LIST(opt, opt_i) {
      pyopt = NULL;
      switch(opt->type) {
      case TRIGVAR_CHAR:  pyopt = charGetPyForm(opt->data); break;
      case TRIGVAR_OBJ:   pyopt = objGetPyForm(opt->data);  break;
      case TRIGVAR_ROOM:  pyopt = roomGetPyForm(opt->data); break;
      }
      PyDict_SetItemString(dict, opt->name, pyopt);
      listPut(varnames, strdup(opt->name));
      Py_XDECREF(pyopt);
    } deleteListIterator(opt_i);
  }

  // run the script, then kill our dictionary
  triggerRun(trig, dict);

  // if triggers create methods, it increases the reference count on our
  // dictionary. This is a known memory leak. We'll try to mitigate it by
  // cleaning out some of the contents. A better solution is needed, that
  // makes sure the dictionary itself is not leaked
  LIST_ITERATOR *vname_i = newListIterator(varnames);
  char            *vname = NULL;
  ITERATE_LIST(vname, vname_i) {
    PyDict_DelItemString(dict, vname);
  } deleteListIterator(vname_i);
  deleteListWith(varnames, free);
  Py_XDECREF(dict);
}

//
// generalized function for running all triggers of a specified type.
void gen_do_trigs(void *me, int me_type, const char *type,
		  CHAR_DATA *ch,OBJ_DATA *obj, ROOM_DATA *room, EXIT_DATA *exit,
		  const char *command, const char *arg, LIST *optional) {
  // find our list of triggers
  LIST *trig_keys = NULL;
  if(me_type == TRIGVAR_CHAR)
    trig_keys = charGetTriggers(me);
  else if(me_type == TRIGVAR_OBJ)
    trig_keys = objGetTriggers(me);
  else if(me_type == TRIGVAR_ROOM)
    trig_keys = roomGetTriggers(me);

  if(trig_keys == NULL || listSize(trig_keys) == 0)
    return;
  
  char        *trig_key = NULL;
  LIST_ITERATOR *trig_i = newListIterator(trig_keys);
  TRIGGER_DATA    *trig = NULL;
  ITERATE_LIST(trig_key, trig_i) {
    if((trig = worldGetType(gameworld, "trigger", trig_key)) != NULL &&
       !strcasecmp(triggerGetType(trig), type))
      gen_do_trig(trig,me,me_type,ch,obj,room,exit,command,arg,optional);
  } deleteListIterator(trig_i);
}



//*****************************************************************************
// trighooks
//*****************************************************************************
void do_give_trighooks(const char *info) {
  CHAR_DATA   *ch = NULL;
  CHAR_DATA *recv = NULL;
  OBJ_DATA   *obj = NULL;
  hookParseInfo(info, &ch, &recv, &obj);

  gen_do_trigs(ch,TRIGVAR_CHAR,"give",recv,obj,NULL,NULL,NULL,NULL,NULL);
  gen_do_trigs(recv,TRIGVAR_CHAR,"receive",ch,obj,NULL,NULL,NULL,NULL,NULL);

  LIST *opts = newList();
  listPut(opts, newOptVar("vict", recv, TRIGVAR_CHAR));
  gen_do_trigs(obj,TRIGVAR_OBJ,"give",ch,NULL,NULL,NULL,NULL,NULL,opts);

  // garbage collection
  deleteListWith(opts, deleteOptVar);
}

void do_get_trighooks(const char *info) {
  CHAR_DATA *ch = NULL;
  OBJ_DATA *obj = NULL;
  hookParseInfo(info, &ch, &obj);

  gen_do_trigs(obj,TRIGVAR_OBJ,"get",ch,NULL,NULL,NULL,NULL,NULL,NULL);
  gen_do_trigs(charGetRoom(ch),TRIGVAR_ROOM,"get",ch,obj,NULL,NULL,NULL,NULL,NULL);
}

void do_drop_trighooks(const char *info) {
  CHAR_DATA *ch = NULL;
  OBJ_DATA *obj = NULL;
  hookParseInfo(info, &ch, &obj);
  gen_do_trigs(obj,TRIGVAR_OBJ,"drop",ch,NULL,NULL,NULL,NULL,NULL,NULL);
  gen_do_trigs(charGetRoom(ch),TRIGVAR_ROOM,"drop",ch,obj,NULL,NULL,NULL,NULL,NULL);
}

void do_enter_trighooks(const char *info) {
  CHAR_DATA   *ch = NULL;
  ROOM_DATA *room = NULL;
  hookParseInfo(info, &ch, &room);

  LIST_ITERATOR *mob_i = newListIterator(roomGetCharacters(room));
  CHAR_DATA       *mob = NULL;
  ITERATE_LIST(mob, mob_i) {
    if(ch != mob)
      gen_do_trigs(mob,TRIGVAR_CHAR,"enter",ch,NULL,NULL,NULL,NULL,NULL,NULL);
  } deleteListIterator(mob_i);
  gen_do_trigs(room,TRIGVAR_ROOM,"enter",ch,NULL,NULL,NULL,NULL,NULL,NULL);
  gen_do_trigs(ch,TRIGVAR_CHAR,"self enter",NULL,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_exit_trighooks(const char *info) {
  CHAR_DATA   *ch = NULL;
  ROOM_DATA *room = NULL;
  EXIT_DATA *exit = NULL;
  hookParseInfo(info, &ch, &room, &exit);

  LIST_ITERATOR *mob_i = newListIterator(roomGetCharacters(room));
  CHAR_DATA       *mob = NULL;
  ITERATE_LIST(mob, mob_i) {
    if(ch != mob)
      gen_do_trigs(mob,TRIGVAR_CHAR,"exit",ch,NULL,NULL,exit,NULL,NULL,NULL);
  } deleteListIterator(mob_i);
  gen_do_trigs(room,TRIGVAR_ROOM,"exit",ch,NULL,NULL,exit,NULL,NULL,NULL);
  gen_do_trigs(ch,TRIGVAR_CHAR,"self exit",NULL,NULL,NULL,exit,NULL,NULL,NULL);
}

void do_ask_trighooks(const char *info) {
  CHAR_DATA       *ch = NULL;
  CHAR_DATA *listener = NULL;
  char        *speech = NULL;
  hookParseInfo(info, &ch, &listener, &speech);
  gen_do_trigs(listener,TRIGVAR_CHAR,"speech",ch,NULL,NULL,NULL,NULL,speech,NULL);

  // garbage collection
  free(speech);
}

void do_say_trighooks(const char *info) {
  CHAR_DATA *ch = NULL;
  char  *speech = NULL;
  hookParseInfo(info, &ch, &speech);

  LIST_ITERATOR *mob_i = newListIterator(roomGetCharacters(charGetRoom(ch)));
  CHAR_DATA       *mob = NULL;
  ITERATE_LIST(mob, mob_i) {
    if(ch != mob)
     gen_do_trigs(mob,TRIGVAR_CHAR,"speech",ch,NULL,NULL,NULL,NULL,speech,NULL);
  } deleteListIterator(mob_i);
  gen_do_trigs(charGetRoom(ch),TRIGVAR_ROOM,"speech",ch,NULL,NULL,NULL,NULL,speech,NULL);

  // garbage collection
  free(speech);
}

void do_greet_trighooks(const char *info) {
  CHAR_DATA      *ch = NULL;
  CHAR_DATA *greeted = NULL;
  hookParseInfo(info, &ch, &greeted);
  gen_do_trigs(greeted,TRIGVAR_CHAR,"greet",ch,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_wear_trighooks(const char *info) {
  CHAR_DATA *ch = NULL;
  OBJ_DATA *obj = NULL;
  hookParseInfo(info, &ch, &obj);
  gen_do_trigs(ch,TRIGVAR_CHAR,"wear",NULL,obj,NULL,NULL,NULL,NULL,NULL);
  gen_do_trigs(obj,TRIGVAR_OBJ,"wear",ch,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_remove_trighooks(const char *info) {
  CHAR_DATA *ch = NULL;
  OBJ_DATA *obj = NULL;
  hookParseInfo(info, &ch, &obj);
  gen_do_trigs(ch,TRIGVAR_CHAR,"remove",NULL,obj,NULL,NULL,NULL,NULL,NULL);
  gen_do_trigs(obj,TRIGVAR_OBJ,"remove",ch,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_reset_trighooks(const char *info) {
  char  *zone_key = NULL;
  hookParseInfo(info, &zone_key);
  ZONE_DATA *zone = worldGetZone(gameworld, zone_key);

  LIST_ITERATOR *res_i = newListIterator(zoneGetResettable(zone));
  char           *name = NULL;
  const char   *locale = zoneGetKey(zone);
  ROOM_DATA      *room = NULL;
  ITERATE_LIST(name, res_i) {
    room = worldGetRoom(gameworld, get_fullkey(name, locale));
    if(room != NULL)
     gen_do_trigs(room,TRIGVAR_ROOM,"reset",NULL,NULL,NULL,NULL,NULL,NULL,NULL);
  } deleteListIterator(res_i);

  // garbage collection
  free(zone_key);
}

void do_open_door_trighooks(const char *info) {
  CHAR_DATA *ch = NULL;
  EXIT_DATA *ex = NULL;
  hookParseInfo(info, &ch, &ex);
  gen_do_trigs(charGetRoom(ch),TRIGVAR_ROOM,"open",ch,NULL,NULL,ex,NULL,NULL,NULL);
}

void do_open_obj_trighooks(const char *info) {
  CHAR_DATA *ch = NULL;
  OBJ_DATA *obj = NULL;
  hookParseInfo(info, &ch, &obj);
  gen_do_trigs(obj,TRIGVAR_OBJ,"open",ch,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_close_door_trighooks(const char *info) {
  CHAR_DATA *ch = NULL;
  EXIT_DATA *ex = NULL;
  hookParseInfo(info, &ch, &ex);
  gen_do_trigs(charGetRoom(ch),TRIGVAR_ROOM,"close",ch,NULL,NULL,ex,NULL,NULL,NULL);
}

void do_close_obj_trighooks(const char *info) {
  CHAR_DATA *ch = NULL;
  OBJ_DATA *obj = NULL;
  hookParseInfo(info, &ch, &obj);
  gen_do_trigs(obj,TRIGVAR_OBJ,"close",ch,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_look_at_obj_trighooks(const char *info) {
  OBJ_DATA     *obj = NULL;
  CHAR_DATA *looker = NULL;
  hookParseInfo(info, &obj, &looker);
  gen_do_trigs(obj,TRIGVAR_OBJ,"look",looker,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_look_at_room_trighooks(const char *info) {
  ROOM_DATA   *room = NULL;
  CHAR_DATA *looker = NULL;
  hookParseInfo(info, &room, &looker);
  gen_do_trigs(room,TRIGVAR_ROOM,"look",looker,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_look_at_char_trighooks(const char *info) {
  CHAR_DATA     *ch = NULL;
  CHAR_DATA *looker = NULL;
  hookParseInfo(info, &ch, &looker);
  gen_do_trigs(ch,TRIGVAR_CHAR,"look",looker,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_obj_to_game_trighooks(const char *info) {
  OBJ_DATA *obj = NULL;
  hookParseInfo(info, &obj);
  gen_do_trigs(obj,TRIGVAR_OBJ,"to_game",NULL,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_char_to_game_trighooks(const char *info) {
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &ch);
  gen_do_trigs(ch, TRIGVAR_CHAR,"to_game",NULL,NULL,NULL,NULL,NULL,NULL,NULL);
}

void do_room_to_game_trighooks(const char *info) {
  ROOM_DATA *rm = NULL;
  hookParseInfo(info, &rm);
  gen_do_trigs(rm,TRIGVAR_ROOM,"to_game",NULL,NULL,NULL,NULL,NULL,NULL,NULL);
}



//*****************************************************************************
// implementation of trighooks.h
//*****************************************************************************
void init_trighooks(void) {
  // add all of our hooks to the game
  hookAdd("give",           do_give_trighooks);
  hookAdd("get",            do_get_trighooks);
  hookAdd("drop",           do_drop_trighooks);
  hookAdd("enter",          do_enter_trighooks);
  hookAdd("exit",           do_exit_trighooks);
  hookAdd("ask",            do_ask_trighooks);
  hookAdd("say",            do_say_trighooks);
  hookAdd("greet",          do_greet_trighooks);
  hookAdd("wear",           do_wear_trighooks);
  hookAdd("remove",         do_remove_trighooks);
  hookAdd("reset",          do_reset_trighooks);
  hookAdd("open_door",      do_open_door_trighooks);
  hookAdd("open_obj",       do_open_obj_trighooks);
  hookAdd("close_door",     do_close_door_trighooks);
  hookAdd("close_obj",      do_close_obj_trighooks);
  hookAdd("look_at_obj",    do_look_at_obj_trighooks);
  hookAdd("look_at_char",   do_look_at_char_trighooks);
  hookAdd("look_at_room",   do_look_at_room_trighooks);
  hookAdd("obj_to_game",    do_obj_to_game_trighooks);
  hookAdd("char_to_game",   do_char_to_game_trighooks);
  hookAdd("room_to_game",   do_room_to_game_trighooks);

  // add our trigger displays
  register_tedit_opt("speech",         "mob, room" );
  register_tedit_opt("greet",          "mob"       );
  register_tedit_opt("enter",          "mob, room" );
  register_tedit_opt("exit",           "mob, room" );
  register_tedit_opt("self enter",     "mob"       ),
  register_tedit_opt("self exit",      "mob"       );
  register_tedit_opt("drop",           "obj, room" );
  register_tedit_opt("get",            "obj, room" );
  register_tedit_opt("give",           "obj, mob"  );
  register_tedit_opt("receive",        "mob"       );
  register_tedit_opt("wear",           "obj, mob"  );
  register_tedit_opt("remove",         "obj, mob"  );
  register_tedit_opt("reset",          "room"      );
  register_tedit_opt("look",           "obj, mob, room" );
  register_tedit_opt("open",           "obj, room" );
  register_tedit_opt("close",          "obj, room" );
  register_tedit_opt("to_game",        "obj, mob, room" );
}
