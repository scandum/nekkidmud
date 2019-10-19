//*****************************************************************************
//
// zedit.c
//
// zedit (zone edit) is a utility to allow builders to edit zone data within the
// game. Contains all the functions for editing zones.
//
//*****************************************************************************

#include <sys/stat.h>

#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../character.h"
#include "../room.h"
#include "../world.h"
#include "../zone.h"
#include "../editor/editor.h"

#include "olc.h"



//*****************************************************************************
// functions for editing zone reset lists
//*****************************************************************************
#define ZRESLIST_NEW     1
#define ZRESLIST_DELETE  2

void zreslist_menu(SOCKET_DATA *sock, LIST *list) {
  if(listSize(list) > 0) {
    listSortWith(list, strcasecmp);

    LIST_ITERATOR *room_i = newListIterator(list);
    char            *room = NULL;

    send_to_socket(sock, "{wRooms reset on zone pulse:\r\n");
    ITERATE_LIST(room, room_i) {
      send_to_socket(sock, "{g  %s\r\n", room);
    } deleteListIterator(room_i);
    send_to_socket(sock, "\r\n");
  }

  send_to_socket(sock,
		 "  N) new room\r\n"
		 "  D) delete room\r\n");
}

int zreslist_chooser(SOCKET_DATA *sock, LIST *list, const char *option) {
  switch(toupper(*option)) {
  case 'N':
    send_to_socket(sock, "Enter the room key: ");
    return ZRESLIST_NEW;
  case 'D':
    send_to_socket(sock, "Enter the room key: ");
    return ZRESLIST_DELETE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool zreslist_parser(SOCKET_DATA *sock, LIST *list, int choice, 
		     const char *arg) {
  // ignore length-zero commands
  if(strlen(arg) == 0)
    return TRUE;

  switch(choice) {
  case ZRESLIST_NEW: {
    if(!listGetWith(list, arg, strcasecmp))
      listPutWith(list, strdup(arg), strcasecmp);
    return TRUE;
  }
  case ZRESLIST_DELETE: {
    char *found = listRemoveWith(list, arg, strcasecmp);
    if(found != NULL) free(found);
    return TRUE;
  }
  default:
    return FALSE;
  }
}



//*****************************************************************************
// room editing functions
//*****************************************************************************
// the different fields of a room we can edit
#define ZEDIT_NAME       1
#define ZEDIT_EDITORS    2
#define ZEDIT_TIMER      3

void zedit_menu(SOCKET_DATA *sock, ZONE_DATA *zone) {
  send_to_socket(sock,
		 "{y[{c%s{y]\r\n"
		 "{g1) Name\r\n{c%s\r\n"
		 "{g2) Editors\r\n{c%s\r\n"
		 "{g3) Reset timer: {c%d{g min%s\r\n"
		 "{g4) Resettable rooms: {c%d\r\n"
		 "{g5) Description\r\n{c%s\r\n"
		 ,
		 zoneGetKey(zone), zoneGetName(zone), zoneGetEditors(zone),
		 zoneGetPulseTimer(zone), (zoneGetPulseTimer(zone)==1 ? "":"s"),
		 listSize(zoneGetResettable(zone)), zoneGetDesc(zone));
}

int zedit_chooser(SOCKET_DATA *sock, ZONE_DATA *zone, const char *option) {
  switch(toupper(*option)) {
  case '1':
    text_to_buffer(sock, "Enter a new zone name: ");
    return ZEDIT_NAME;
  case '2':
    text_to_buffer(sock, "Enter a new list of editors: ");
    return ZEDIT_EDITORS;
  case '3':
    text_to_buffer(sock, "Enter a new reset timer: ");
    return ZEDIT_TIMER;
  case '4':
    do_olc(sock, zreslist_menu, zreslist_chooser, zreslist_parser, 
	   NULL, NULL, NULL, NULL, zoneGetResettable(zone));
    return MENU_NOCHOICE;
  case '5':
    text_to_buffer(sock, "Enter a new description:\r\n");
    socketStartEditor(sock, text_editor, zoneGetDescBuffer(zone));
    return MENU_NOCHOICE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool zedit_parser(SOCKET_DATA *sock, ZONE_DATA *zone, int choice, 
		  const char *arg) {
  switch(choice) {
  case ZEDIT_NAME:
    zoneSetName(zone, arg);
    return TRUE;
  case ZEDIT_EDITORS:
    zoneSetEditors(zone, arg);
    return TRUE;
  case ZEDIT_TIMER:
    zoneSetPulseTimer(zone, MAX(-1, atoi(arg)));
    return TRUE;
  default:
    return FALSE;
  }
}

// saves a zone to disk
void save_zone(ZONE_DATA *zone) {
  zoneSave(zone);
}


COMMAND(cmd_zedit) {
  // we want to create a new zone?
  if(!strncasecmp(arg, "new ", 4)) {
    if(!bitIsSet(charGetUserGroups(ch), "admin")) {
      send_to_char(ch, "You are not authorized to create new zones.\r\n");
      return;
    }

    char key[100];

    // scan for the parameters
    sscanf(arg+4, "%s", key);

    if(locale_malformed(key))
      send_to_char(ch, "The zone name you entered was malformed.");
    else if(!strcasecmp(arg, "new"))
      send_to_char(ch, "'new' is not a valid zone name.");
    else if(worldGetZone(gameworld, key))
      send_to_char(ch, "A zone already exists with that key.\r\n");
    else {
      char buf[MAX_BUFFER];
      ZONE_DATA *zone = newZone(key);
      sprintf(buf, "%s's zone", charGetName(ch));
      zoneSetName(zone, buf);
      sprintf(buf, "A new zone created by %s\r\n", charGetName(ch));
      zoneSetDesc(zone, buf);
      zoneSetEditors(zone, charGetName(ch));

      worldPutZone(gameworld, zone);
      send_to_char(ch, "You create a new zone (key %s).\r\n", key);
      worldSave(gameworld, WORLD_PATH);
    }
  }

  // we want to edit a preexisting zone
  else if(locale_malformed(arg))
    send_to_char(ch, "The zone name you entered was malformed.");
  else {
    ZONE_DATA *zone = 
      (*arg ? worldGetZone(gameworld, arg) : 
       worldGetZone(gameworld, 
		    get_key_locale(roomGetClass(charGetRoom(ch)))));

    // make sure there is a corresponding zone ...
    if(zone == NULL)
      send_to_char(ch, "No such zone exists. To create a new one, use "
		       "zedit new <key>\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit this zone.\r\n");  
    else {
      do_olc(charGetSocket(ch), zedit_menu, zedit_chooser, zedit_parser,
	     zoneCopy, zoneCopyTo, deleteZone, save_zone, zone);
    }
  }
}
