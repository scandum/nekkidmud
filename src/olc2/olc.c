//*****************************************************************************
//
// olc.c
//
// My first go at OLC was during the pre-module time, and it really wasn't all
// that well put together in the first place. This is the second go at it...
// this time, OLC is more like some general functions that make it easy to do
// online editing, rather than a full system. It will be amenable to adding
// new extentions to the general OLC framework from other modules, and it will
// essentially just be a bunch spiffier.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../world.h"
#include "../character.h"  // for POS_XXX <-- we should change the place of this
#include "../auxiliary.h"
#include "../prototype.h"
#include "../zone.h"
#include "../room_reset.h"
#include "../room.h"
#include "../handler.h"
#include "../object.h"
#include "../inform.h"
#include "olc.h"
#include "olc_extender.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pymudsys.h"
#include "../scripts/pysocket.h"
#include "../scripts/pyplugs.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_HELP2
#include "../help2/help.h"
#endif



//*****************************************************************************
// local data structures, defines, and functions
//*****************************************************************************

// extender data for our various OLCs
OLC_EXTENDER *medit_extend = NULL;
OLC_EXTENDER *oedit_extend = NULL;
OLC_EXTENDER *redit_extend = NULL;

typedef struct olc_data {
  void    (* menu)(SOCKET_DATA *, void *);
  int  (* chooser)(SOCKET_DATA *, void *, const char *);
  bool  (* parser)(SOCKET_DATA *, void *, int, const char *);
  void *(* copier)(void *);
  void  (* copyto)(void *, void *);
  void (* deleter)(void *);
  void   (* saver)(void *); 

  // the data we're working with
  void          *data;
  void  *working_copy;
  int             cmd;
} OLC_DATA;

OLC_DATA *newOLC(void    (* menu)(SOCKET_DATA *, void *),
		 int  (* chooser)(SOCKET_DATA *, void *, const char *),
		 bool  (* parser)(SOCKET_DATA *, void *, int, const char *),
		 void *(* copier)(void *),
		 void  (* copyto)(void *, void *),
		 void (* deleter)(void *),
		 void   (* saver)(void *),
		 void *data) {
  OLC_DATA *olc     = calloc(1, sizeof(OLC_DATA));
  olc->menu         = menu;
  olc->chooser      = chooser;
  olc->parser       = parser;
  olc->copier       = copier;
  olc->copyto       = copyto;
  olc->deleter      = deleter;
  olc->saver        = saver;
  olc->data         = data;
  olc->cmd          = MENU_NOCHOICE;
  olc->working_copy = (copier ? copier(data) : data);
  return olc;
}

void deleteOLC(OLC_DATA *olc) {
  // are we working with C functions or Python functions?
  if(olc->deleter)
    olc->deleter(olc->working_copy);
  free(olc);
}



//*****************************************************************************
// auxiliary data we put on the socket
//*****************************************************************************
typedef struct olc_aux_data {
  LIST *olc_stack; // the list of OLCs we have opened
} OLC_AUX_DATA;

OLC_AUX_DATA *
newOLCAuxData() {
  OLC_AUX_DATA *data = calloc(1, sizeof(OLC_AUX_DATA));
  data->olc_stack = newList();
  return data;
}

void
deleteOLCAuxData(OLC_AUX_DATA *data) {
  if(data->olc_stack) deleteListWith(data->olc_stack, deleteOLC);
  free(data);
}



//*****************************************************************************
// Implementation of the OLC framework
//*****************************************************************************

//
// display the current menu to the socket, as well as the generic prompt
//
void olc_menu(SOCKET_DATA *sock) {
  // send the current menu
  OLC_AUX_DATA *aux_olc = socketGetAuxiliaryData(sock, "olc_aux_data");
  OLC_DATA         *olc = listGet(aux_olc->olc_stack, 0);
  // don't display then menu if we've made a menu choice
  if(olc->cmd == MENU_NOCHOICE) {
    text_to_buffer(sock, CLEAR_SCREEN);
    olc->menu(sock, olc->working_copy);
#ifdef MODULE_HELP2
    text_to_buffer(sock, "\r\n{gEnter choice, ? [topic] for help, or Q to quit: ");
#else
    text_to_buffer(sock, "\r\n{gEnter choice, or Q to quit: ");
#endif
  }
}


//
// handle the cleanup process for exiting OLC; pop the top handler off of
// the OLC stack and delete it. Save changes if neccessary. Exit the OLC handler
// if we have no more OLC work to do. Return the new and improved version of
// whatever it was we were editing.
//
void *olc_exit(SOCKET_DATA *sock, bool save) {
  // pull up the OLC data to manipulate
  OLC_AUX_DATA *aux_olc = socketGetAuxiliaryData(sock, "olc_aux_data");
  OLC_DATA         *olc = listGet(aux_olc->olc_stack, 0);
  void            *data = olc->data;

  // pop our OLC off of the OLC stack
  listPop(aux_olc->olc_stack);

  // if we need to do any saving, do it now
  if(save) {
    // first, save the changes on the item
    if(olc->working_copy != olc->data && olc->copyto)
      olc->copyto(olc->working_copy, olc->data);
    // now, make sure the changes go to disk
    if(olc->saver)
      olc->saver(olc->data);
  }

  // delete our OLC data (this also deletes our working copy 
  // if it's not the same as our main copy)
  deleteOLC(olc);

  // if our OLC stack is empty, pop the OLC input handler
  if(listSize(aux_olc->olc_stack) == 0)
    socketPopInputHandler(sock);
  return data;
}


//
// Handle input while in OLC
//
void olc_handler(SOCKET_DATA *sock, char *arg) {
  // pull up the OLC data. Are we entering a new command, or are
  // we entering in an argument from a menu choice?
  OLC_AUX_DATA *aux_olc = socketGetAuxiliaryData(sock, "olc_aux_data");
  OLC_DATA         *olc = listGet(aux_olc->olc_stack, 0);

  // we're giving an argument for a menu choice we've already selected
  if(olc->cmd > MENU_NOCHOICE) {
    // the change went alright. Re-display the menu
    if(olc->parser(sock, olc->working_copy, olc->cmd, arg))
      olc->cmd = MENU_NOCHOICE;
    else
      text_to_buffer(sock, "Invalid choice!\r\nTry again: ");
  }

  // we're being prompted if we want to save our changes or not before quitting
  else if(olc->cmd == MENU_CHOICE_CONFIRM_SAVE) {
    switch(toupper(*arg)) {
    case 'Y':
    case 'N':
      olc_exit(sock, (toupper(*arg) == 'Y' ? TRUE : FALSE));
      break;
    default:
      text_to_buffer(sock, "Invalid choice!\r\nTry again: ");
      break;
    }
  }

  // we're entering a new choice from our current menu
  else {
    switch(*arg) {
      // we're quitting
    case 'q':
    case 'Q':
      // if our working copy is different from our actual data, prompt to
      // see if we want to save our changes or not
      if(olc->saver || olc->data != olc->working_copy) {
	text_to_buffer(sock, "Save changes (Y/N): ");
	olc->cmd = MENU_CHOICE_CONFIRM_SAVE;
      }
      else {
	olc_exit(sock, TRUE);
      }
      break;

#ifdef MODULE_HELP2
    case '?': {
      while(*arg == '?' || isspace(*arg))
	arg++;
      // we have a prompt; don't redisplay our menu
      HELP_DATA *data = get_help(arg, TRUE);
      if(data == NULL) {
	send_to_socket(sock, "No help available.\r\nTry again: ");
	olc->cmd = MENU_CHOICE_INVALID;
      }
      else if(*helpGetUserGroups(data) && socketGetChar(sock) &&
	      !bitIsOneSet(charGetUserGroups(socketGetChar(sock)),
			   helpGetUserGroups(data))) {
	send_to_socket(sock, "You may not view that help file.\r\nTry again: ");
	olc->cmd = MENU_CHOICE_INVALID;
      }
      else {
	// we've (tried to) switched handlers... no menu display
	BUFFER *buf = build_help(arg);
	olc->cmd = MENU_NOCHOICE;
	start_reader(sock, bufferString(buf));
	deleteBuffer(buf);
      }
      break;
    }
#endif

    default: {
      int cmd = olc->chooser(sock, olc->working_copy, arg);
      // the menu choice we entered wasn't a valid one. redisplay the menu
      if(cmd == MENU_CHOICE_INVALID)
	;//olc_menu(sock);
      // the menu choice was acceptable. Note this in our data
      else if(cmd > MENU_NOCHOICE)
	olc->cmd = cmd;
      break;
    }
    }
  }
}



//*****************************************************************************
// builder commands
//*****************************************************************************

//
// Load a copy of a specific mob/object
// usage: load <mob | obj> <vnum>
COMMAND(cmd_load) {
  if(!arg || !*arg)
    send_to_char(ch, "What did you want to load?\r\n");
  else {
    char   type[SMALL_BUFFER];
    char   name[SMALL_BUFFER];
    char locale[SMALL_BUFFER];
    char    key[SMALL_BUFFER];
    arg = one_arg(arg, type);
    if(!parse_worldkey_relative(ch, arg, name, locale)) {
      send_to_char(ch, "What did you want to load?\r\n");
      return;
    }

    sprintf(key, "%s@%s", name, locale);
    if(key_malformed(key))
      send_to_char(ch, "You entered a malformed content key.\r\n");
    else if(!strncasecmp("mobile", type, strlen(type))) {
      PROTO_DATA *proto = worldGetType(gameworld, "mproto", key);
      if(proto == NULL)
	send_to_char(ch, "No mobile prototype exists with that key.\r\n");
      else if(protoIsAbstract(proto))
	send_to_char(ch, "That prototype is abstract.\r\n");
      else {
	CHAR_DATA *mob = protoMobRun(proto);
	if(mob == NULL)
	  send_to_char(ch, "There was an error running the prototype.\r\n");
	else {
	  send_to_char(ch, "You create %s.\r\n", charGetName(mob));
	  char_to_room(mob, charGetRoom(ch));
	}
      }
    }

    else if(!strncasecmp("object", type, strlen(type))) {
      PROTO_DATA *proto = worldGetType(gameworld, "oproto", key);
      if(proto == NULL)
	send_to_char(ch, "No object prototype exists with that key.\r\n");
      else if(protoIsAbstract(proto))
	send_to_char(ch, "That prototype is abstract.\r\n");
      else {
	OBJ_DATA *obj = protoObjRun(proto);
	if(obj == NULL)
	  send_to_char(ch, "There was an error running the prototype.\r\n");
	else {
	  send_to_char(ch, "You create %s.\r\n", objGetName(obj));
	  obj_to_char(obj, ch);
	}
      }
    }

    // type not found
    else 
      send_to_char(ch, "What type of thing did you want to load?\r\n");
  }
}


//
// remove an object or player from the game. If no argument is supplied, all
// objects and non-player characters are removed from the current room.
//   usage: purge <target>
COMMAND(cmd_purge) {
  void    *found = NULL;
  int found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, "| { ch.room.noself obj.room }",
		 &found, &found_type))
    return;

  // purge everything in the current room
  if(found == NULL) {
    LIST_ITERATOR *list_i = newListIterator(roomGetContents(charGetRoom(ch)));
    OBJ_DATA *obj;
    CHAR_DATA *vict;

    send_to_char(ch, "You purge the room.\r\n");
    message(ch, NULL, NULL, NULL, FALSE, TO_ROOM,
	    "$n raises $s arms, and white flames engulf the entire room.");

    // purge all the objects. 
    ITERATE_LIST(obj, list_i)
      extract_obj(obj);
    deleteListIterator(list_i);

    // and now all of the non-characters
    list_i = newListIterator(roomGetCharacters(charGetRoom(ch)));
    ITERATE_LIST(vict, list_i) {
      if(vict == ch || !charIsNPC(vict)) 
	continue;
      extract_mobile(vict);
    }
    deleteListIterator(list_i);
  }

  // purge characters
  else if(found_type == PARSE_CHAR) {
    // we can only purge him if we have all the same groups as him, and more
    if(!charHasMoreUserGroups(ch, found))
      send_to_char(ch, "Erm, you better not try that on %s. %s has "
		   "just as much priviledges as you.\r\n", 
		   HIMHER(found), HESHE(found));
    else {
      send_to_char(ch, "You purge %s.\r\n", charGetName(found));
      message(ch, found, NULL, NULL, FALSE, TO_ROOM,
	      "$n raises $s arms, and white flames engulf $N.");
      extract_mobile(found);
    }
  }

  // purge objects
  else if(found_type == PARSE_OBJ) {
    send_to_char(ch, "You purge %s.\r\n", objGetName(found));
    message(ch, NULL, found, NULL, FALSE, TO_ROOM,
	    "$n raises $s arms, and white flames engulf $o.");
    obj_from_room(found);
    extract_obj(found);
  }
}


//
// reruns the room's load script, and replaces the old version of the room with
// the new one.
COMMAND(cmd_rreload) {
  PROTO_DATA   *proto = NULL;
  ROOM_DATA *old_room = NULL;
  ROOM_DATA *new_room = NULL;
  ZONE_DATA     *zone = NULL;
  const char     *key = roomGetClass(charGetRoom(ch));

  // unless an arg is supplied, we're working on the current room.
  if(arg && *arg)
    key = get_fullkey_relative(arg, get_key_locale(key));

  // make sure all of our requirements are met
  if(key_malformed(arg))
    send_to_char(ch, "You entered a malformed content key.\r\n");
  else if( (zone = worldGetZone(gameworld, get_key_locale(key))) == NULL)
    send_to_char(ch, "That zone does not exist!\r\n");
  else if(!canEditZone(zone, ch))
    send_to_char(ch, "You are not authorized to edit that zone.\r\n");
  else if( (proto = worldGetType(gameworld, "rproto", key)) == NULL)
    send_to_char(ch, "No prototype for that room exists.\r\n");
  else if(!worldRoomLoaded(gameworld, key))
    send_to_char(ch, "No room with that key is currently loaded.\r\n");
  else {
    // try running the proto to get our new room...
    old_room = worldGetRoom(gameworld, key);
    new_room = protoRoomRun(proto);
    if(new_room == NULL)
      send_to_char(ch, "There was an error reloading the room.\r\n");
    else {
      do_mass_transfer(old_room, new_room, TRUE, TRUE, TRUE);
      extract_room(old_room);
      worldPutRoom(gameworld, key, new_room);
      send_to_char(ch, "Room reloaded.\r\n");
    }
  }
}


//
// trigger all of a specified zone's reset scripts and such. If no vnum is
// supplied, the zone the user is currently in is reset.
//   usage: zreset <zone vnum>
COMMAND(cmd_zreset) {
  ZONE_DATA *zone = NULL;

  if(!arg || !*arg)
    zone= worldGetZone(gameworld,get_key_locale(roomGetClass(charGetRoom(ch))));
  else if(locale_malformed(arg))
    zone= NULL;
  else
    zone= worldGetZone(gameworld, arg);

  if(zone == NULL)
    send_to_char(ch, "Which zone did you want to reset?\r\n");
  else if(!canEditZone(zone, ch))
    send_to_char(ch, "You are not authorized to edit that zone.\r\n");
  else {
    send_to_char(ch, "%s has been reset.\r\n", zoneGetName(zone));
    zoneForceReset(zone);
  }
}

COMMAND(cmd_rdelete) {
  char *name = NULL;
  if(!parse_args(ch, TRUE, cmd, arg, "word", &name))
    return;
  if(do_delete(ch, "rproto", deleteProto, name)) {
    do_delete(ch, "reset", deleteResetList, name);
    send_to_char(ch, "If the room has already been used, do not forget to "
		 "also purge the current instance of it.\r\n");
  }
}

COMMAND(cmd_mdelete) {
  char *name = NULL;
  if(!parse_args(ch, TRUE, cmd, arg, "word", &name))
    return;
  do_delete(ch, "mproto", deleteProto, name);
}

COMMAND(cmd_odelete) {
  char *name = NULL;
  if(!parse_args(ch, TRUE, cmd, arg, "word", &name))
    return;
  do_delete(ch, "oproto", deleteProto, name);
}



//*****************************************************************************
// Functions for listing different types of data (zones, mobs, objs, etc...)
//*****************************************************************************
//
// returns yes/no if the prototype is abstract or not
const char *prototype_list_info(PROTO_DATA *data) {
  static char buf[SMALL_BUFFER];
  sprintf(buf, "%-50s %3s", 
	  (*protoGetParents(data) ? protoGetParents(data) : "-------"),
	  (protoIsAbstract(data)  ? "yes" : "no"));
  return buf;
}

// this is used for the header when printing out zone proto info
#define PROTO_LIST_HEADER \
"Parents                                       Abstract"


COMMAND(cmd_rlist) {
  do_list(ch, (arg&&*arg?arg:get_key_locale(roomGetClass(charGetRoom(ch)))),
	  "rproto", PROTO_LIST_HEADER, prototype_list_info);
}

COMMAND(cmd_mlist) {
  do_list(ch, (arg&&*arg?arg:get_key_locale(roomGetClass(charGetRoom(ch)))),
	  "mproto", PROTO_LIST_HEADER, prototype_list_info);
}

COMMAND(cmd_olist) {
  do_list(ch, (arg&&*arg?arg:get_key_locale(roomGetClass(charGetRoom(ch)))),
	  "oproto", PROTO_LIST_HEADER, prototype_list_info);
}

COMMAND(cmd_mrename) {
  char *from = NULL, *to = NULL;
  if(!parse_args(ch, TRUE, cmd, arg, "word word", &from, &to))
    return;
  do_rename(ch, "mproto", from, to);
}

COMMAND(cmd_rrename) {
  char *from = NULL, *to = NULL;
  if(!parse_args(ch, TRUE, cmd, arg, "word word", &from, &to))
    return;

  if(do_rename(ch, "rproto", from, to)) {
    do_rename(ch, "reset", from, to);
    send_to_char(ch, "No not forget to purge any instances of %s already "
		 "loaded.\r\n", from); 
  }
}

COMMAND(cmd_orename) {
  char *from = NULL, *to = NULL;
  if(!parse_args(ch, TRUE, cmd, arg, "word word", &from, &to))
    return;
  do_rename(ch, "oproto", from, to);
}

void view_proto(CHAR_DATA *ch, const char *type, const char *key) {
  if(!charGetSocket(ch))
    return;
  else if(key_malformed(key))
    send_to_char(ch, "You entered a malformed %s key.\r\n", type);
  else {
    PROTO_DATA *proto = 
      worldGetType(gameworld, type, get_fullkey_relative(key, 
			      get_key_locale(roomGetClass(charGetRoom(ch)))));
    if(proto == NULL)
      send_to_char(ch, "No %s exists with that key.\r\n", type);
    else {
      send_to_socket(charGetSocket(ch),
		     "--------------------------------------------------------------------------------\r\n"
		     "Key          : %s\r\n"
		     "Parents      : %s\r\n"
		     "Abstract     : %s\r\n"
		     "--------------------------------------------------------------------------------\r\n",
		     protoGetKey(proto), 
		     protoGetParents(proto),
		     YESNO(protoIsAbstract(proto)));
      script_display(charGetSocket(ch), protoGetScript(proto), FALSE);
    }
  }
}

COMMAND(cmd_mview) {
  view_proto(ch, "mproto", arg);
}

COMMAND(cmd_rview) {
  view_proto(ch, "rproto", arg);
}

COMMAND(cmd_oview) {
  view_proto(ch, "oproto", arg);
}

COMMAND(cmd_zlist) {
  LIST *keys = worldGetZoneKeys(gameworld);

  // first, order all the zones
  listSortWith(keys, strcasecmp);

  // now, iterate across them all and show them
  LIST_ITERATOR *zone_i = newListIterator(keys);
  ZONE_DATA       *zone = NULL;
  char             *key = NULL;

  send_to_char(ch,
" {wKey            Name                                             Editors  Timer\r\n"
"{b--------------------------------------------------------------------------------\r\n{n");

  ITERATE_LIST(key, zone_i) {
    if( (zone = worldGetZone(gameworld, key)) != NULL) {
      send_to_char(ch, " {c%-14s %-30s %25s  {w%5d\r\n", key, zoneGetName(zone),
		   zoneGetEditors(zone), zoneGetPulseTimer(zone));
    }
  } deleteListIterator(zone_i);
  deleteListWith(keys, free);
  send_to_char(ch, "{g");
}



//*****************************************************************************
// implementation of olc.h
//*****************************************************************************
void init_olc2() {
  // install our auxiliary data on the socket so 
  // we can keep track of our olc data
  auxiliariesInstall("olc_aux_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_SOCKET,
				       newOLCAuxData, deleteOLCAuxData,
				       NULL, NULL, NULL, NULL));

  // initialize all of our basic OLC commands. We should probably have init
  // functions to add each of the commands... consistency in style an all...
  extern COMMAND(cmd_redit);
  extern COMMAND(cmd_resedit);
  extern COMMAND(cmd_zedit);
  extern COMMAND(cmd_medit);
  extern COMMAND(cmd_oedit);
  extern COMMAND(cmd_accedit);
  extern COMMAND(cmd_pcedit);
  extern COMMAND(cmd_mpedit);
  extern COMMAND(cmd_opedit);
  extern COMMAND(cmd_rpedit);
  extern COMMAND(cmd_dig);
  extern COMMAND(cmd_fill);
  extern COMMAND(cmd_instantiate);

  add_cmd("zedit",   NULL, cmd_zedit,   "builder", TRUE);
  add_cmd("redit",   NULL, cmd_redit,   "builder", TRUE);
  add_cmd("resedit", NULL, cmd_resedit, "builder", TRUE);
  add_cmd("medit",   NULL, cmd_medit,   "builder", TRUE);
  add_cmd("oedit",   NULL, cmd_oedit,   "builder", TRUE);
  add_cmd("accedit", NULL, cmd_accedit, "admin",   TRUE);
  add_cmd("pcedit",  NULL, cmd_pcedit,  "admin",   TRUE);
  add_cmd("mpedit",  NULL, cmd_mpedit,  "scripter",TRUE);
  add_cmd("opedit",  NULL, cmd_opedit,  "scripter",TRUE);
  add_cmd("rpedit",  NULL, cmd_rpedit,  "scripter",TRUE);

  add_cmd("dig",     NULL, cmd_dig,     "builder", TRUE);
  add_cmd("fill",    NULL, cmd_fill,    "builder", TRUE);
  add_cmd("pack",    NULL, cmd_fill,    "builder", TRUE);
  add_cmd("instantiate", NULL, cmd_instantiate, "builder", TRUE);
  add_cmd("purge",   NULL, cmd_purge,   "wizard", FALSE);
  add_cmd("load",    NULL, cmd_load,    "wizard", FALSE);
  add_cmd("rcopy",   NULL, cmd_instantiate,"builder", TRUE);

  add_cmd("mlist",   NULL, cmd_mlist,   "builder", FALSE);
  add_cmd("mdelete", NULL, cmd_mdelete, "builder", FALSE);
  add_cmd("mrename", NULL, cmd_mrename, "builder", FALSE);
  add_cmd("olist",   NULL, cmd_olist,   "builder", FALSE);
  add_cmd("odelete", NULL, cmd_odelete, "builder", FALSE);
  add_cmd("orename", NULL, cmd_orename, "builder", FALSE);
  add_cmd("rreload", NULL, cmd_rreload, "builder", FALSE);
  add_cmd("rlist",   NULL, cmd_rlist,   "builder", FALSE);
  add_cmd("rdelete", NULL, cmd_rdelete, "builder", FALSE);
  add_cmd("rrename", NULL, cmd_rrename, "builder", FALSE);
  add_cmd("zlist",   NULL, cmd_zlist,   "builder", TRUE);
  add_cmd("zreset",  NULL, cmd_zreset,  "builder", FALSE);
  add_cmd("mview",   NULL, cmd_mview,   "builder", TRUE);
  add_cmd("rview",   NULL, cmd_rview,   "builder", TRUE);
  add_cmd("oview",   NULL, cmd_oview,   "builder", TRUE);

  // build our basic OLC extenders
  medit_extend = newExtender();
  extenderSetPyFunc(medit_extend, charGetPyFormBorrowed);
  oedit_extend = newExtender();
  extenderSetPyFunc(oedit_extend, objGetPyFormBorrowed);
  redit_extend = newExtender();
  extenderSetPyFunc(redit_extend, roomGetPyFormBorrowed);
}

void do_olc(SOCKET_DATA *sock,
	    void *menu,
	    void *chooser,
	    void *parser,
	    void *copier,
	    void *copyto,
	    void *deleter,
	    void *saver,
	    void *data) {
  OLC_DATA *olc = newOLC(menu, chooser, parser, copier, copyto, deleter,
			 saver, data);

  OLC_AUX_DATA *aux_olc = socketGetAuxiliaryData(sock, "olc_aux_data");
  listPush(aux_olc->olc_stack, olc);

  // if this is the only olc data on the stack, then enter the OLC handler
  if(listSize(aux_olc->olc_stack) == 1)
    socketPushInputHandler(sock, olc_handler, olc_menu, "olc");
}

void olc_from_proto(PROTO_DATA *proto, BUFFER *extra, void *me, void *aspy) {
  BUFFER *to_run = newBuffer(1);
  char line[MAX_BUFFER];
  const char *code = protoGetScript(proto);
  bool  extra_code = FALSE;

  // go line by line and parse everything that's relevant 
  // in 'to_run', and append everything that's not into 'extra'
  do {
    code = strcpyto(line, code, '\n');

    // is this a begin/end marker?
    if(!strcmp(ECODE_BEGIN, line))
      extra_code = TRUE;
    else if(!strcmp(ECODE_END, line))
      extra_code = FALSE;
    // is this relevant code, or "extra" code
    else if(extra_code == TRUE)
      bprintf(extra, "%s\n", line);
    else
      bprintf(to_run, "%s\n", line);
  } while(*code != '\0');

  // make all our arguments like functions
  void *(* aspy_func)(void *) = aspy;

  // make our Python stuff
  PyObject *dict = restricted_script_dict();
  PyObject *pyme = aspy_func(me);
  PyDict_SetItemString(dict, "me", pyme);  

  // run the script
  run_script(dict, bufferString(to_run), get_key_locale(protoGetKey(proto)));

  // make sure it ran ok
  if(!last_script_ok())
    log_pyerr("Error converting prototype to OLC editable structure: %s",
	      protoGetKey(proto));

  // clean up our garbage
  deleteBuffer(to_run);
  Py_DECREF(dict);
}


void olc_display_table(SOCKET_DATA *sock, const char *getName(int val),
		       int num_vals, int num_cols) {
  int i, print_room;
  char fmt[100];

  print_room = (80 - 10*num_cols)/num_cols;
  sprintf(fmt, "  {c%%2d{y) {g%%-%ds%%s", print_room);

  for(i = 0; i < num_vals; i++)
    send_to_socket(sock, fmt, i, getName(i), 
		   (i % num_cols == (num_cols - 1) ? "\r\n" : "   "));

  if(i % num_cols != 0)
    send_to_socket(sock, "\r\n");
}

void olc_display_list(SOCKET_DATA *sock, LIST *list, int num_cols) {
  char fmt[100];
  LIST_ITERATOR *list_i = newListIterator(list);
  int print_room, i = 0;
  char *str = NULL;
  
  print_room = (80 - 10*num_cols)/num_cols;
  sprintf(fmt, "  {c%%2d{y) {g%%-%ds%%s", print_room);

  ITERATE_LIST(str, list_i) {
    send_to_socket(sock, fmt, i, str, (i % num_cols == (num_cols - 1) ? 
				       "\r\n" : "   "));
    i++;
  } deleteListIterator(list_i);

  if(i % num_cols != 0)
    send_to_socket(sock, "\r\n");
}
