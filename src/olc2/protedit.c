//*****************************************************************************
//
// protedit.c
//
// protedit (prototype edit) handles the OLC editing of prototypes.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../prototype.h"
#include "../socket.h"
#include "../character.h"
#include "../room.h"
#include "../world.h"
#include "olc.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../editor/editor.h"
#include "../scripts/scripts.h"
#include "../scripts/script_editor.h"



//*****************************************************************************
// olc functions
//*****************************************************************************
#define PROTEDIT_PARENTS    1

void protedit_menu(SOCKET_DATA *sock, PROTO_DATA *data) {
  send_to_socket(sock,
		 "{y[{c%s{y]\r\n"
		 "{g1) parents : {c%s\r\n"
		 "{g2) abstract: {c%s\r\n"
		 "{g3) prototype code\r\n",
		 protoGetKey(data), protoGetParents(data), 
		 (protoIsAbstract(data) ? "yes" : "no"));
  script_display(sock, protoGetScript(data), FALSE);
}

int protedit_chooser(SOCKET_DATA *sock, PROTO_DATA *data, const char *option) {
  switch(toupper(*option)) {
  case '1':
    send_to_socket(sock, "Enter a comma-separated list of the parent prototypes: ");
    return PROTEDIT_PARENTS;
  case '2':
    protoSetAbstract(data, (protoIsAbstract(data) + 1) % 2);
    return MENU_NOCHOICE;
  case '3':
    socketStartEditor(sock, script_editor, protoGetScriptBuffer(data));
    return MENU_NOCHOICE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool protedit_parser(SOCKET_DATA *sock, PROTO_DATA *data, int choice, 
		     const char *arg) {
  switch(choice) {
  case PROTEDIT_PARENTS:
    protoSetParents(data, arg);
    return TRUE;
  default:
    return FALSE;
  }
}

void save_mproto(PROTO_DATA *mproto) {
  worldSaveType(gameworld, "mproto", protoGetKey(mproto));
}

void save_oproto(PROTO_DATA *oproto) {
  worldSaveType(gameworld, "oproto", protoGetKey(oproto));
}

void save_rproto(PROTO_DATA *rproto) {
  worldSaveType(gameworld, "rproto", protoGetKey(rproto));
}

void gen_cmd_protoedit(CHAR_DATA *ch, const char *type, const char *arg,
		       void (* saver)(PROTO_DATA *)) {
  ZONE_DATA    *zone = NULL;
  PROTO_DATA  *proto = NULL;

  // we need a key
  if(!arg || !*arg)
    send_to_char(ch, "What is the name of the %s you want to edit?\r\n", type);
  else if(key_malformed(arg))
    send_to_char(ch, "You entered an invalid content key.\r\n");
  else {
    char locale[SMALL_BUFFER];
    char   name[SMALL_BUFFER];
    if(!parse_worldkey_relative(ch, arg, name, locale))
      send_to_char(ch, "Which %s are you trying to edit?\r\n", type);
    // make sure we can edit the zone
    else if((zone = worldGetZone(gameworld, locale)) == NULL)
      send_to_char(ch, "No such zone exists.\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit that zone.\r\n");
    else {
      // try to pull up the prototype
      proto = worldGetType(gameworld, type, get_fullkey(name, locale));
      if(proto == NULL) {
	proto = newProto();
	protoSetAbstract(proto, TRUE);
	protoSetScript(proto, 
		       "# prototype code goes here\n"
		       "# make sure to comment it with pounds (#)\n");
	worldPutType(gameworld, type, get_fullkey(name, locale), proto);
      }
      
      do_olc(charGetSocket(ch), protedit_menu, protedit_chooser,protedit_parser,
 	     protoCopy, protoCopyTo, deleteProto, saver, proto);
    }
  }
}

COMMAND(cmd_mpedit) {
  gen_cmd_protoedit(ch, "mproto", arg, save_mproto);
}

COMMAND(cmd_opedit) {
  gen_cmd_protoedit(ch, "oproto", arg, save_oproto);
}

COMMAND(cmd_rpedit) {
  gen_cmd_protoedit(ch, "rproto", 
		    (arg && *arg ? arg : roomGetClass(charGetRoom(ch))), 
		    save_rproto);
}
