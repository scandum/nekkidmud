//*****************************************************************************
//
// utils.c
//
// This file contains all sorts of utility functions used
// all sorts of places in the code.
//
//*****************************************************************************
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h> 
#include <time.h>

// include main header file
#include "mud.h"
#include "prototype.h"
#include "character.h"
#include "object.h"
#include "world.h"
#include "zone.h"
#include "room.h"
#include "room_reset.h"
#include "exit.h"
#include "socket.h"
#include "utils.h"
#include "save.h"
#include "handler.h"
#include "inform.h"
#include "event.h"
#include "action.h"
#include "hooks.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "items/items.h"



//*****************************************************************************
// implementation of utils.h
//*****************************************************************************
void  extract_obj_final(OBJ_DATA *obj) {
  obj_from_game(obj);
  deleteObj(obj);
}

void extract_obj(OBJ_DATA *obj) {
  // make sure we're not attached to anything
  CHAR_DATA *sitter = NULL;
  while( (sitter = (CHAR_DATA *)listGet(objGetUsers(obj), 0)) != NULL)
    char_from_furniture(sitter);

  OBJ_DATA *content = NULL;
  while( (content = (OBJ_DATA *)listGet(objGetContents(obj), 0)) != NULL)
    extract_obj(content);

  if(objGetRoom(obj))
    obj_from_room(obj);
  if(objGetWearer(obj))
    try_unequip(objGetWearer(obj), obj);
  if(objGetCarrier(obj))
    obj_from_char(obj);
  if(objGetContainer(obj))
    obj_from_obj(obj);

  if(!listIn(objs_to_delete, obj))
    listPut(objs_to_delete, obj);
}

void extract_mobile_final(CHAR_DATA *ch) {
  char_from_game(ch);
  if(charIsNPC(ch) || !player_exists(charGetName(ch)))
    deleteChar(ch);
  else
    unreference_player(ch);
}

void extract_mobile(CHAR_DATA *ch) {
  // unequip everything the character is wearing
  // and send it to inventory
  unequip_all(ch);
  // extract everything in the character's inventory
  LIST_ITERATOR *obj_i = newListIterator(charGetInventory(ch));
  OBJ_DATA        *obj = NULL;
  ITERATE_LIST(obj, obj_i) {
    extract_obj(obj);
  } deleteListIterator(obj_i);

  // make sure we're not attached to anything
  if(charGetFurniture(ch))
    char_from_furniture(ch);
  if(charGetRoom(ch))
    char_from_room(ch);

  // make sure we close down the socket
  if(charGetSocket(ch) != NULL) {
    close_socket(charGetSocket(ch), FALSE);
    charSetSocket(ch, NULL);
  }

  if(!listIn(mobs_to_delete, ch))
    listPut(mobs_to_delete, ch);
}

void extract_room_final(ROOM_DATA *room) {
  room_from_game(room);
  deleteRoom(room);
}

void extract_room(ROOM_DATA *room) {
  roomSetExtracted(room);

  // extract all of the objects characters in us
  CHAR_DATA *ch = NULL;
  while( (ch = listGet(roomGetCharacters(room), 0)) != NULL)
    extract_mobile(ch);

  // and the objects
  OBJ_DATA *obj = NULL;
  while( (obj = listGet(roomGetContents(room), 0)) != NULL)
    extract_obj(obj);

  // remove us from the world we're in ... the if should always be true if
  // we are in fact part of the world.
  if(worldRoomLoaded(gameworld, roomGetClass(room)) &&
     worldGetRoom(gameworld, roomGetClass(room)) == room)
    worldRemoveRoom(gameworld, roomGetClass(room));

  // If anyone's last room was us, make sure we set the last room to NULL
  LIST_ITERATOR *ch_i = newListIterator(mobile_list);
  ITERATE_LIST(ch, ch_i) {
    if(charGetLastRoom(ch) == room)
      charSetLastRoom(ch, NULL);
  } deleteListIterator(ch_i);

  if(!listIn(rooms_to_delete, room))
    listPut(rooms_to_delete, room);
}

void communicate(CHAR_DATA *dMob, char *txt, int range)
{
  switch(range) {
  default:
    bug("Communicate: Bad Range %d.", range);
    return;

  case COMM_LOG:
    send_to_groups("admin, scripter", "[LOG: %s]\r\n", txt);
    break;
  }
}


/*
 * load the world and its inhabitants, as well as other misc game data
 */
void load_muddata() {  
  if(gameworld == NULL) {
    log_string("ERROR: Could not boot game world.");
    abort();
  }

  worldAddType(gameworld, "mproto", protoRead,  protoStore,  deleteProto, 
	       protoSetKey);
  worldAddType(gameworld, "oproto", protoRead,  protoStore,  deleteProto,
	       protoSetKey);
  worldAddType(gameworld, "rproto", protoRead,  protoStore,  deleteProto,
	       protoSetKey);
  worldAddType(gameworld, "reset",  resetListRead, resetListStore, 
	       deleteResetList, resetListSetKey);

  worldSetPath(gameworld, WORLD_PATH);
  worldInit(gameworld);

  greeting = read_file("../lib/txt/greeting");
  motd     = read_file("../lib/txt/motd");
}


char *get_time()
{
  static char buf[16];
  char *strtime;
  int i;

  strtime = ctime(&current_time);
  for (i = 0; i < 15; i++)   
    buf[i] = strtime[i + 4];
  buf[15] = '\0';

  return buf;
}

// try entering into the game
// disconnect the socket if we fail.
bool try_enter_game(CHAR_DATA *ch) {
  // find the loadroom
  ROOM_DATA *loadroom = worldGetRoom(gameworld, charGetLoadroom(ch));

  // if the loadroom doesn't exist, 
  // try loading the character to the start room
  if(loadroom == NULL) {
    send_to_char(ch, 
		 "Your loadroom seems to be missing. Attempting to "
		 "load you to the start room.\r\n");
    log_string("ERROR: %s had invalid loadroom, %s", 
	       charGetName(ch), charGetLoadroom(ch));
    loadroom = worldGetRoom(gameworld, START_ROOM);
  }
  
  // we have a loadroom... all is good
  if(loadroom != NULL) {
    // add him to the game
    char_to_game(ch);
    char_to_room(ch, loadroom);
    return TRUE;
  }

  // oops! No rooms seem to exist
  else {
    send_to_char(ch,
		 "The gameworld was not properly loaded. The MUD may "
		 "be going through maintenance. Try again later.\r\n");
    log_string("ERROR: When loading %s, start room did not exist!",
	       charGetName(ch));

    return FALSE;
  }
}

CHAR_DATA *check_reconnect(const char *player) {
  CHAR_DATA *dMob;
  LIST_ITERATOR *mob_i = newListIterator(mobile_list);

  ITERATE_LIST(dMob, mob_i) {
    if (!charIsNPC(dMob) && charIsName(dMob, player)) {
      if (charGetSocket(dMob)) {
        close_socket(charGetSocket(dMob), TRUE);
	charSetSocket(dMob, NULL);
      }
      break;
    }
  } deleteListIterator(mob_i);
  return dMob;
}

void do_mass_transfer(ROOM_DATA *from, ROOM_DATA *to, bool chars, bool mobs,
		      bool objs) {
  if(chars || mobs) {
    LIST_ITERATOR *ch_i = newListIterator(roomGetCharacters(from));
    CHAR_DATA       *ch = NULL;
    ITERATE_LIST(ch, ch_i) {
      if(mobs || (chars && !charIsNPC(ch))) {
	char_from_room(ch);
	char_to_room(ch, to);
      }
    } deleteListIterator(ch_i);
  }

  if(objs) {
    LIST_ITERATOR *obj_i = newListIterator(roomGetContents(from));
    OBJ_DATA        *obj = NULL;
    ITERATE_LIST(obj, obj_i) {
      obj_from_room(obj);
      obj_to_room(obj, to);
    } deleteListIterator(obj_i);
  }
}

const char *exitGetToFull(EXIT_DATA *exit) {
  if(exitGetRoom(exit) == NULL)
    return exitGetTo(exit);
  else
    return get_fullkey_relative(exitGetTo(exit), get_key_locale(roomGetClass(exitGetRoom(exit))));
}

ROOM_DATA *objGetRootRoom(OBJ_DATA *obj) {
  if(objGetCarrier(obj))
    return charGetRoom(objGetCarrier(obj));
  else if(objGetWearer(obj))
    return charGetRoom(objGetWearer(obj));
  else if(objGetContainer(obj))
    return objGetRootRoom(objGetContainer(obj));
  return objGetRoom(obj);
}


int can_see_hidden(CHAR_DATA *ch) {
  return 0;
}

int can_see_invis(CHAR_DATA *ch) {
  return 0;
}

LIST *char_see_checks = NULL;
LIST *obj_see_checks  = NULL;
LIST *exit_see_checks = NULL;

void register_char_see( bool (* check)(CHAR_DATA *ch, CHAR_DATA *target)) {
  if(char_see_checks == NULL)
    char_see_checks = newList();
  listPut(char_see_checks, check);
}

void register_obj_see( bool (* check)(CHAR_DATA *ch, OBJ_DATA *target)) {
  if(obj_see_checks == NULL)
    obj_see_checks = newList();
  listPut(obj_see_checks, check);
}

void register_exit_see(bool (* check)(CHAR_DATA *ch, EXIT_DATA *target)) {
  if(exit_see_checks == NULL)
    exit_see_checks = newList();
  listPut(exit_see_checks, check);
}

bool  can_see_char(CHAR_DATA *ch, CHAR_DATA *target) {
  if(ch == target)
    return TRUE;
  if(poscmp(charGetPos(ch), POS_SLEEPING) <= 0)
    return FALSE;

  bool ret = TRUE;
  if(char_see_checks != NULL) {
    LIST_ITERATOR *chk_i = newListIterator(char_see_checks);
    bool (*chk)(CHAR_DATA *, CHAR_DATA *) = NULL;
    ITERATE_LIST(chk, chk_i) {
      ret = chk(ch, target);
      if(ret == FALSE)
	break;
    } deleteListIterator(chk_i);
  }
  return ret;
}

bool can_see_obj(CHAR_DATA *ch, OBJ_DATA  *target) {
  if(poscmp(charGetPos(ch), POS_SLEEPING) <= 0)
    return FALSE;

  bool ret = TRUE;
  if(obj_see_checks != NULL) {
    LIST_ITERATOR *chk_i = newListIterator(obj_see_checks);
    bool (*chk)(CHAR_DATA *, OBJ_DATA *) = NULL;
    ITERATE_LIST(chk, chk_i) {
      ret = chk(ch, target);
      if(ret == FALSE)
	break;
    } deleteListIterator(chk_i);
  }
  return ret;
}

bool can_see_exit(CHAR_DATA *ch, EXIT_DATA *target) {
  if(poscmp(charGetPos(ch), POS_SLEEPING) <= 0)
    return FALSE;

  bool ret = TRUE;
  if(exit_see_checks != NULL) {
    LIST_ITERATOR *chk_i = newListIterator(exit_see_checks);
    bool (*chk)(CHAR_DATA *, EXIT_DATA *) = NULL;
    ITERATE_LIST(chk, chk_i) {
      ret = chk(ch, target);
      if(ret == FALSE)
	break;
    } deleteListIterator(chk_i);
  }
  return ret;
}

const char *see_exit_as(CHAR_DATA *ch, EXIT_DATA *target) {
  if(!can_see_exit(ch, target))
    return SOMEWHERE;
  else {
    BUFFER     *buf = newBuffer(1);
    const char *dir = NULL;
    ROOM_DATA *dest = NULL;

    if(exitGetRoom(target))
      dir = roomGetExitDir(exitGetRoom(target), target);

    // build up what the exit looks like
    if(exitIsClosed(target)) {
      if(*exitGetName(target))
	bprintf(buf, exitGetName(target));
      else
	bprintf(buf, "closed");
    }
    else if( (dest = worldGetRoom(gameworld, exitGetToFull(target))) != NULL)
      bprintf(buf, "%s", roomGetName(dest));
    else
      bprintf(buf, "%s", SOMEWHERE);

    listPush(bufs_to_delete, buf);
    return bufferString(buf);
  }
}

const char *see_char_as(CHAR_DATA *ch, CHAR_DATA *target) {
  if(!can_see_char(ch, target))
    return SOMEONE;
  // put the interaction commands around our name
  else
    return charGetName(target);
}

//
// finds the UID of the room the object is in
int find_obj_room(OBJ_DATA *obj) {
  if(objGetRoom(obj))
    return roomGetUID(objGetRoom(obj));
  else if(objGetCarrier(obj))
    return roomGetUID(charGetRoom(objGetCarrier(obj)));
  else if(objGetWearer(obj))
    return roomGetUID(charGetRoom(objGetWearer(obj)));
  else if(objGetContainer(obj))
    return find_obj_room(objGetContainer(obj));
  return NOWHERE;
}

const char *see_obj_as(CHAR_DATA *ch, OBJ_DATA  *target) {
  if(!can_see_obj(ch, target))
    return SOMETHING;
  else 
    return objGetName(target);
}

//
// returns a name as a UID, or -1 if it does not translate
int name_as_uid(const char *name) {
  if(name == NULL)
    return NOBODY;
  int i = 0, uid = NOBODY;
  for(; name[i] != '\0'; i++)
    if(!isdigit(name[i]))
      return NOBODY;
  sscanf(name, "%d", &uid);
  return uid;
}

int count_objs(CHAR_DATA *looker, LIST *list, const char *name, 
	       const char *prototype, bool must_see) {
  LIST_ITERATOR *obj_i = newListIterator(list);
  OBJ_DATA *obj;
  int count = 0;
  int   uid = name_as_uid(name);

  ITERATE_LIST(obj, obj_i) {
    if(must_see && !can_see_obj(looker, obj))
      continue;
    // if we have a name, search by it
    if(objGetUID(obj) == uid || (name && *name && objIsName(obj, name)))
      count++;
    // otherwise search by prototype
    else if(prototype && *prototype && objIsInstance(obj, prototype))
      count++;
  }
  deleteListIterator(obj_i);
  return count;
}

int count_chars(CHAR_DATA *looker, LIST *list, const char *name,
		const char *prototype, bool must_see) {
  LIST_ITERATOR *char_i = newListIterator(list);
  CHAR_DATA *ch;
  int count = 0;
  int   uid = name_as_uid(name);

  ITERATE_LIST(ch, char_i) {
    if(must_see && !can_see_char(looker, ch))
      continue;
    // if we have a name, search by it
    if((name && *name && charIsName(ch, name)) || charGetUID(ch) == uid)
      count++;
    // otherwise, search by prototype
    else if(prototype && *prototype && charIsInstance(ch, prototype))
      count++;
  } deleteListIterator(char_i);

  return count;
}

//
// find the numth occurance of the character with name, "name"
// if name is not supplied, search by prototype. If name is supplied as m.XXX,
// then search by the mob's UID
//
CHAR_DATA *find_char(CHAR_DATA *looker, LIST *list, int num, const char *name,
		     const char *prototype, bool must_see) {
  if(num <= 0)
    return NULL;
  CHAR_DATA *ch = NULL;

  // is everything in the description a number? Search by UID
  int uid = name_as_uid(name);
  if(uid != NOBODY) {
    ch = propertyTableGet(mob_table, uid);
    if(listIn(list, ch) && can_see_char(looker, ch))
      return ch;
    return NULL;
  }

  LIST_ITERATOR *char_i = newListIterator(list);
  ITERATE_LIST(ch, char_i) {
    if(must_see && !can_see_char(looker, ch))
      continue;
    // if we have a name, search by name
    if(name && *name && charIsName(ch, name))
      num--;
    // otherwise search by prototype
    else if(prototype && *prototype && charIsInstance(ch, prototype))
      num--;
    if(num == 0)
      break;
  } deleteListIterator(char_i);
  return ch;
}


//
// find the numth occurance of the object with name, "name"
// if name is not supplied, search by prototype
//
OBJ_DATA *find_obj(CHAR_DATA *looker, LIST *list, int num, 
		   const char *name, const char *prototype, bool must_see) {
  if(num == 0)
    return NULL;
  OBJ_DATA *obj = NULL;

  // is everything in the description a number? Search by UID
  int uid = name_as_uid(name);
  if(uid != NOTHING) {
    obj = propertyTableGet(obj_table, uid);
    if(listIn(list, obj) && can_see_obj(looker, obj))
      return obj;
    return NULL;
  }

  LIST_ITERATOR *obj_i = newListIterator(list);
  ITERATE_LIST(obj, obj_i) {
    if(must_see && !can_see_obj(looker, obj))
      continue;
    // if a name is supplied, search by name
    if(name && *name && objIsName(obj, name))
      num--;
    // otherwise, search by prototype
    else if(prototype && *prototype && objIsInstance(obj, prototype))
      num--;
    if(num == 0)
      break;
  } deleteListIterator(obj_i);
  return obj;
}


//
// Find all characters in the list with the given name. Or all characters
// if name is NULL
//
LIST *find_all_chars(CHAR_DATA *looker, LIST *list, const char *name,
		     const char *prototype, bool must_see) {
  LIST_ITERATOR *char_i = newListIterator(list);
  LIST *char_list = newList();
  int         uid = name_as_uid(name);
  CHAR_DATA *ch;

  ITERATE_LIST(ch, char_i) {
    if(must_see && !can_see_char(looker, ch))
      continue;
    if((name && (!*name || charIsName(ch, name))) || charGetUID(ch) == uid)
      listPut(char_list, ch);
    else if(prototype && *prototype && charIsInstance(ch, prototype))
      listPut(char_list, ch);
  } deleteListIterator(char_i);
  return char_list;
}


//
// Returns a llist of all the chars found with a specific name in the list.
// the list must be deleted after use.
//
LIST *find_all_objs(CHAR_DATA *looker, LIST *list, const char *name, 
		    const char *prototype, bool must_see) {

  LIST_ITERATOR *obj_i = newListIterator(list);
  LIST       *obj_list = newList();
  int              uid = name_as_uid(name);
  OBJ_DATA *obj;

  ITERATE_LIST(obj, obj_i) {
    if(must_see && !can_see_obj(looker, obj))
      continue;
    if((name && (!*name || objIsName(obj, name))) || objGetUID(obj) == uid)
      listPut(obj_list, obj);
    else if(prototype && *prototype && objIsInstance(obj, prototype))
      listPut(obj_list, obj);
  } deleteListIterator(obj_i);
  return obj_list;
}

//
// separates a joint item-count (e.g. 2.sword, all.woman) into its
// count and its word
//
void get_count(const char *buf, char *target, int *count) {
  // hmmm... we can probably condense these two checks into one, non?
  if(!strncasecmp(buf, "all.", 4)) {
    //sscanf(buf, "all.%s", target);
    strcpyto(target, buf+4, '\0');
    *count = COUNT_ALL;
  }
  else if(!strcasecmp(buf, "all")) {
    *target = '\0';
    *count = COUNT_ALL;
  }
  else if(!strstr(buf, ".")) {
    strcpyto(target, buf, '\0');
    //sscanf(buf, "%s", target);
    *count = 1;
  }
  else {
    //*count = 1;
    //strcpyto(target, buf, '\0');
    sscanf(buf, "%d", count);
    strcpyto(target, buf+next_letter_in(buf, '.')+1, '\0');
  }
}


//
// concats an item count and a target into a single term
//
void print_count(char *buf, const char *target, int count) {
  if(count > 1)               
    sprintf(buf, "%d.%s", count, target);
  else if(count == COUNT_ALL && *target)
    sprintf(buf, "all.%s", target);
  else if(count == COUNT_ALL)
    sprintf(buf, "all");
  else          
    sprintf(buf, "%s", target);
}


//
// Calculates how many letters until we hit the next one
// of a specified type
//
int next_letter_in(const char *string, char marker) {
  int i = 0;
  for(i = 0; string[i] != '\0'; i++)
    if(string[i] == marker)
      return i;
  return -1; // none found
}


//
// Calculates how many characters until we hit the next whitespace. Newlines,
// tabs, and spaces are treated as whitespace.
int next_space_in(const char *string) {
  int i = 0;
  for(i = 0; string[i] != '\0'; i++)
    if(isspace(string[i]))
      return i;
  return -1; // none found
}

//
// If we're at the beginning of a new paragraph, return where the new paragraph
// starts. Otherwise, return our current position (index)
int is_paragraph_marker(const char *string, int index) {
  int nl_count = 0;
  int i = 0;
  for(i = index; isspace(string[i]); i++) {
    if(string[i] == '\n')
      nl_count++;
  }
  if(nl_count > 1)
    return i;
  else
    return index;
}

//
// hashing array 1 for pearson hashing
int pearson_table1[] = { 66, 93, 11, 153, 155, 113, 214, 132, 91, 193, 240, 82, 175, 145, 84, 34, 76, 217, 250, 230, 139, 172, 65, 254, 196, 56, 165, 116, 48, 219, 199, 142, 35, 27, 210, 149, 45, 127, 41, 150, 85, 87, 253, 100, 234, 216, 192, 226, 154, 106, 78, 146, 131, 38, 120, 151, 177, 29, 50, 231, 68, 168, 227, 161, 126, 141, 36, 191, 110, 81, 197, 190, 9, 236, 140, 0, 20, 162, 23, 189, 42, 130, 117, 86, 243, 123, 237, 249, 64, 135, 61, 167, 39, 57, 96, 148, 118, 13, 235, 188, 19, 71, 49, 115, 21, 182, 15, 200, 179, 251, 75, 77, 204, 32, 180, 16, 218, 22, 171, 88, 30, 248, 47, 238, 105, 94, 92, 67, 28, 69, 33, 215, 1, 7, 241, 109, 209, 98, 12, 208, 156, 52, 195, 89, 185, 55, 170, 104, 17, 173, 122, 138, 4, 202, 136, 247, 169, 222, 163, 211, 144, 252, 2, 186, 201, 40, 207, 107, 18, 24, 46, 129, 44, 14, 174, 26, 124, 194, 37, 223, 102, 183, 99, 114, 70, 158, 53, 111, 147, 119, 73, 152, 79, 203, 157, 221, 10, 97, 133, 62, 229, 178, 205, 184, 164, 176, 198, 80, 58, 245, 31, 59, 128, 101, 60, 181, 246, 232, 63, 143, 121, 213, 187, 206, 43, 134, 6, 225, 228, 72, 54, 233, 224, 5, 8, 239, 112, 244, 255, 137, 3, 74, 108, 159, 83, 125, 103, 51, 220, 25, 166, 90, 160, 212, 95, 242 };

int pearson_table2[] = { 202, 142, 134, 22, 233, 237, 13, 31, 41, 97, 141, 148, 74, 165, 7, 162, 53, 117, 210, 226, 174, 88, 5, 163, 17, 49, 170, 99, 93, 39, 69, 108, 207, 244, 254, 101, 159, 30, 188, 67, 235, 150, 24, 136, 208, 221, 234, 43, 96, 12, 78, 10, 25, 81, 239, 120, 37, 21, 42, 183, 121, 213, 14, 161, 137, 23, 9, 255, 245, 209, 222, 236, 119, 199, 216, 71, 115, 110, 63, 107, 173, 70, 20, 89, 91, 102, 227, 1, 177, 113, 104, 111, 253, 181, 95, 243, 72, 6, 124, 131, 190, 86, 164, 85, 251, 3, 50, 154, 217, 155, 8, 105, 82, 28, 75, 147, 0, 80, 252, 140, 29, 2, 242, 160, 57, 34, 247, 167, 47, 126, 144, 168, 18, 231, 44, 58, 206, 77, 125, 171, 36, 76, 98, 26, 241, 180, 61, 151, 194, 15, 45, 84, 212, 32, 196, 92, 192, 139, 112, 229, 100, 109, 189, 248, 156, 94, 153, 145, 127, 146, 175, 138, 215, 195, 198, 128, 182, 218, 19, 250, 132, 40, 214, 103, 83, 73, 106, 133, 135, 186, 123, 219, 158, 38, 152, 232, 116, 64, 172, 52, 200, 204, 240, 224, 203, 249, 59, 157, 114, 191, 230, 48, 166, 122, 65, 228, 184, 4, 205, 220, 225, 62, 169, 79, 27, 197, 11, 87, 193, 179, 223, 56, 68, 178, 187, 149, 143, 16, 55, 35, 201, 118, 185, 51, 66, 33, 54, 176, 60, 46, 211, 246, 238, 130, 90, 129 };

int pearson_table3[] = { 44, 181, 139, 127, 174, 243, 236, 14, 5, 200, 235, 180, 195, 185, 193, 116, 161, 110, 72, 121, 9, 3, 104, 224, 136, 182, 15, 94, 222, 84, 186, 20, 239, 147, 21, 34, 183, 93, 61, 164, 189, 89, 17, 226, 56, 205, 176, 51, 128, 201, 73, 75, 190, 163, 96, 178, 102, 33, 150, 130, 98, 76, 120, 240, 79, 158, 24, 196, 78, 4, 202, 168, 48, 255, 227, 97, 138, 101, 207, 170, 58, 27, 45, 225, 179, 215, 251, 123, 209, 38, 63, 36, 6, 175, 62, 208, 86, 65, 107, 188, 249, 146, 198, 194, 254, 230, 253, 103, 46, 133, 192, 26, 108, 219, 191, 77, 137, 81, 145, 66, 91, 106, 7, 10, 29, 95, 165, 88, 119, 151, 40, 22, 1, 122, 114, 134, 37, 214, 217, 68, 0, 83, 199, 70, 80, 156, 140, 144, 90, 118, 42, 173, 111, 157, 204, 171, 212, 241, 53, 11, 148, 135, 206, 172, 23, 109, 13, 220, 245, 197, 177, 47, 59, 210, 49, 74, 54, 25, 234, 115, 43, 169, 218, 50, 87, 31, 143, 248, 69, 92, 160, 141, 41, 228, 184, 233, 117, 2, 231, 252, 28, 216, 125, 153, 18, 100, 166, 8, 52, 124, 250, 132, 67, 244, 12, 246, 32, 64, 213, 242, 247, 223, 82, 131, 112, 221, 113, 155, 71, 211, 60, 129, 149, 237, 30, 85, 55, 152, 229, 35, 232, 187, 19, 105, 203, 39, 159, 238, 167, 162, 57, 126, 99, 154, 142, 16 };

int pearson_table4[] = { 231, 223, 174, 160, 113, 173, 104, 49, 122, 229, 13, 8, 232, 81, 38, 90, 64, 131, 123, 176, 33, 84, 166, 241, 16, 190, 159, 144, 189, 193, 66, 137, 163, 162, 111, 26, 15, 9, 1, 119, 255, 136, 167, 178, 94, 130, 114, 32, 110, 99, 78, 153, 239, 179, 243, 249, 133, 76, 86, 242, 165, 169, 19, 155, 152, 67, 215, 220, 36, 24, 213, 105, 117, 168, 128, 204, 248, 238, 177, 161, 222, 205, 108, 186, 116, 51, 230, 68, 181, 58, 39, 47, 71, 211, 209, 101, 251, 164, 60, 10, 12, 83, 180, 50, 172, 55, 158, 20, 70, 107, 147, 170, 91, 118, 40, 140, 3, 252, 184, 34, 146, 127, 28, 254, 150, 57, 228, 126, 192, 14, 253, 45, 227, 225, 30, 4, 17, 44, 95, 201, 18, 149, 124, 65, 129, 154, 195, 102, 212, 219, 6, 75, 132, 244, 52, 187, 240, 198, 85, 217, 185, 224, 208, 21, 250, 87, 216, 183, 245, 203, 79, 61, 100, 200, 246, 80, 138, 109, 120, 92, 247, 143, 202, 72, 134, 156, 207, 233, 218, 37, 23, 125, 82, 234, 56, 151, 106, 112, 197, 41, 88, 96, 171, 31, 175, 93, 182, 188, 135, 48, 148, 5, 226, 103, 29, 97, 139, 115, 0, 77, 89, 74, 35, 42, 157, 59, 69, 2, 62, 236, 142, 196, 145, 7, 54, 237, 206, 194, 73, 121, 98, 199, 27, 235, 11, 221, 46, 141, 43, 63, 25, 22, 210, 53, 191, 214 };

unsigned long pearson_hash8(const char *string, int *table) {
  unsigned long h = 0;
  for(; *string; string++)
    h = table[h ^ tolower(*string)];
  return h;
}

unsigned long pearson_hash8_1(const char *string) {
  return pearson_hash8(string, pearson_table1);
}

unsigned long pearson_hash8_2(const char *string) {
  return pearson_hash8(string, pearson_table2);
}

unsigned long pearson_hash8_3(const char *string) {
  return pearson_hash8(string, pearson_table3);
}

unsigned long pearson_hash8_4(const char *string) {
  return pearson_hash8(string, pearson_table4);
}

// concatinate two strings of 8 bits
unsigned long pearson_hash16_1(const char *string) {
  return pearson_hash8_1(string)  | (pearson_hash8_2(string) << 8);
}

// again, different hashing functions
unsigned long pearson_hash16_2(const char *string) {
  return pearson_hash8_3(string)  | (pearson_hash8_4(string) << 8);
}

// concatinate four strings of 8 bits
unsigned long pearson_hash32(const char *string) {
  return ((pearson_hash8_1(string))       | (pearson_hash8_2(string) << 8) |
	  (pearson_hash8_3(string) << 16) | (pearson_hash8_4(string) << 24));
}

//
// just a generic function for hashing a string. This could be 
// sped up tremendously if it's performance becoming a problem.
unsigned long string_hash(const char *key) {
  return pearson_hash32(key);
}

bool endswith(const char *string, const char *end) {
  int slen = strlen(string);
  int elen = strlen(end);
  return (slen >= elen && !strcasecmp(string + slen - elen, end));
}

bool startswith(const char *string, const char *start) {
  return !strncasecmp(string, start, strlen(start));
}

const char *strcpyto(char *to, const char *from, char end) {
  // copy everything up to end, and then delimit our destination buffer
  for(; *from != '\0' && *from != end; to++, from++)
    *to = *from;
  *to = '\0';

  // skip our end character and return whatever's left
  if(*from != '\0')
    from++;
  return from;
}

//
// counts how many times ch occurs in string
//
int count_letters(const char *string, const char ch, const int strlen) {

  int i, n;
  for(i = n = 0; i < strlen; i++)
    if(string[i] == ch)
      n++;

  return n;
}

//
// counts how many times word occurs in string. Assumes strlen(word) >= 1
//
int count_occurences(const char *string, const char *word) {
  int count = 0, i = 0, word_len = strlen(word);
  for(; string[i] != '\0'; i++) {
    if(!strncmp(string+i, word, word_len)) {
      count++;
      i += word_len;
    }
  }
  return count;
}

//
// return a pointer to the start of the line num (lines are ended with \n's).
// return NULL if the line does not exist
//
char *line_start(char *string, int line) {
  // skip forward to the appropriate line
  int i, count = 1;

  // are we looking for the start?
  if(line == 1) return string;

  for(i = 0; string[i] != '\0'; i++) {
    if(string[i] == '\n')
      count++;
    if(count == line)
      return string+i+1;
  }

  return NULL;
}


//
// trim trailing and leading whitespace
//
void trim(char *string) {
  int len = strlen(string);
  int max = len-1;
  int min = 0;
  int i;

  // kill all whitespace to the right
  for(max = len - 1; max >= 0; max--) {
    if(isspace(string[max]))
      string[max] = '\0';
    else
      break;
  }

  // find our first non-whitespace
  while(isspace(string[min]))
    min++;

  // shift everything to the left
  for(i = 0; i <= max-min; i++)
    string[i] = string[i+min];
  string[i] = '\0';
}


//
// Returns true if "word" is found in the comma-separated list of
// keywords
//
bool is_keyword(const char *keywords, const char *word, bool abbrev_ok) {
  int word_len = strlen(word);
  if(word_len < 1)
    return FALSE;

  // while we haven't reached the end of the string
  while(*keywords != '\0') {
    // skip all spaces and commas
    while(isspace(*keywords) || *keywords == ',')
      keywords = keywords+1;
    // figure out the length of the current keyword
    int keyword_len = next_letter_in(keywords, ',');
    if(keyword_len == -1)
      keyword_len = strlen(keywords);

    // see if we compare to the current keyword
    if(!abbrev_ok && !strncasecmp(keywords, word, keyword_len) &&
       keyword_len == word_len)
      return TRUE;
    if(abbrev_ok && !strncasecmp(keywords, word, word_len))
      return TRUE;
    // we didn't. skip this current keyword, and find
    // the start of the next keyword or the end of the string
    else while(!(*keywords == '\0' || *keywords == ','))
      keywords = keywords+1;
  }
  return FALSE;
}


//
// Return 0 if the head of the string does not match any of our
// keywords. Returns the length of the keyword if it does match
int find_keyword(const char *keywords, const char *string) {
  LIST           *words = parse_keywords(keywords);
  LIST_ITERATOR *word_i = newListIterator(words);
  char            *word = NULL;
  int               len = 0;

  // try to find the longest keyword
  ITERATE_LIST(word, word_i) {
    int word_len = strlen(word);
    if(!strncasecmp(word, string, word_len) && word_len > len)
      len = word_len;
  } deleteListIterator(word_i);
  deleteListWith(words, free);

  return len;
}

//
// returns whether or not a keyword exists twice in the list
bool dup_keywords_exist(const char *keywords) {
  LIST           *keys = parse_keywords(keywords);
  char            *key = NULL;
  bool       dup_found = FALSE;

  while( !dup_found && (key = listPop(keys)) != NULL) {
    if(listGetWith(keys, key, strcasecmp) != NULL)
      dup_found = TRUE;
    free(key);
  } deleteListWith(keys, free);

  return dup_found;
}

//
// Return a list of all the strings in this list. String are separated by the
// delimeter. The list and contents must be deleted after use.
LIST *parse_strings(const char *string, char delimeter) {
  // make our list that we will be returning
  LIST *list = newList();

  // first, we check if the string have any non-spaces
  int i;
  bool nonspace_found = FALSE;
  for(i = 0; string[i] != '\0'; i++) {
    if(!isspace(string[i])) {
      nonspace_found = TRUE;
      break;
    }
  }

  // we didn't find any non-spaces. Return NULL
  if(!nonspace_found)
    return list;

  // find all of our keywords
  while(*string != '\0') {
    i = 0;
    // find the endpoint
    while(string[i] != delimeter && string[i] != '\0')
      i++;

    char buf[i+1];
    strncpy(buf, string, i); buf[i] = '\0';
    trim(buf); // skip all whitespaces

    // make sure something still exists. If it does, queue it
    if(*buf != '\0')
      listQueue(list, strdup(buf));

    // skip everything we just copied, plus our delimeter
    string = &string[i+(string[i] != '\0' ? 1 : 0)];
  }

  // return whatever we found
  return list;
}


//
// return a list of all the unique keywords found in the keywords string
// keywords can be more than one word long (e.g. blue flame) and each
// keyword must be separated by a comma
LIST *parse_keywords(const char *keywords) {
  return parse_strings(keywords, ',');
}


//
// If the keyword does not already exist, add it to the keyword list
// keywords may be freed and re-built in the process, to make room
void add_keyword(char **keywords_ptr, const char *word) {
  // if it's already a keyword, do nothing
  if(!is_keyword(*keywords_ptr, word, FALSE)) {
    BUFFER *buf = newBuffer(MAX_BUFFER);
    // make our new keyword list
    bprintf(buf, "%s%s%s", *keywords_ptr, (**keywords_ptr ? ", " : ""), word);
    // free the old string, copy the new one
    free(*keywords_ptr);
    *keywords_ptr = strdup(bufferString(buf));
    // clean up our garbage
    deleteBuffer(buf);
  }
}


//
// go through the keywords and if the word is found, remove it
void remove_keyword(char *keywords, const char *word) {
  LIST *words = parse_keywords(keywords);
  char  *copy = NULL;
  int   count = 0;

  // remove all copies of it
  while( (copy = listRemoveWith(words, word, strcasecmp)) != NULL) {
    free(copy);
    count++;
  }

  // did we find a copy of the bad keyword?
  if(count > 0) {
    LIST_ITERATOR *word_i = newListIterator(words);
    int             key_i = 0;
    count = 0;

    // clear the current keywords... we'll rebuild them
    *keywords = '\0';

    ITERATE_LIST(copy, word_i) {
      count++;
      key_i += sprintf(keywords+key_i, "%s", copy);
      if(count < listSize(words) - 1)
	key_i += sprintf(keywords+key_i, ", ");
    } deleteListIterator(word_i);
  }

  // clean up our garbage
  deleteListWith(words, free);
}


//
// print the string on the buffer, centered for a line of length linelen.
// if border is TRUE, put a border to either side of the string.
//
void center_string(char *buf, const char *string, int linelen, int buflen, 
		   bool border) {
  char fmt[32];
  int str_len = strlen(string);
  int spaces  = (linelen - str_len)/2;

  if(border) {
    int i, buf_i = 0;
    sprintf(fmt, "{n%%-%ds[{c", spaces-2);
    buf_i  = snprintf(buf, buflen, fmt, " ");
    // replace all of the spaces with -
    for(i = 0; buf[i] != '\0'; i++) if(buf[i] == ' ') buf[i] = '-';

    buf_i += snprintf(buf+buf_i, buflen-buf_i, " %s ", string);
    sprintf(fmt, "{n]%%-%ds\r\n", 
	    spaces-2 + (((linelen-str_len) % 2) == 1 ? 1 : 0));

    i = buf_i;
    buf_i += snprintf(buf+buf_i, buflen-buf_i, fmt, " ");
    // replace all of the spaces with -
    for(; buf[i] != '\0'; i++) if(buf[i] == ' ') buf[i] = '-';
  }
  else {
    int buf_i = 0;
    sprintf(fmt, "%%-%ds", spaces);
    buf_i  = snprintf(buf, buflen, fmt, " ");
    buf_i += snprintf(buf+buf_i, buflen-buf_i, "%s\r\n", string);
  }
}


//
// fill up the buffer with characters until
// a newline is reached, or we hit our critical
// length. Return how many characters were read
//
int fgetline(FILE *file, char *p, int maxlen)
{
  int count = 0;

  while(!feof(file) && count < maxlen - 1) {

    p[count] = fgetc(file);
    if(p[count] == '\n')
      break;

    count++;
  }

  p[count] = '\0';
  return count;
}

int rand_number(int min, int max) {
  if(min > max) {
    log_string("ERROR: rand_number passed a min (%d) higher than its max (%d).",
	       min, max);
    return 0;
  }

  return min + rand() % (max-min + 1);
}

double rand_percent(void) {
  double rnd = rand_number(0, RAND_MAX);
  return rnd / (double)RAND_MAX;
}

double rand_gaussian(void) {
  return sqrt(-2.0 * log(rand_percent())) * cos(2.0 * PI * rand_percent());
}

double sigmoid(double val) {
  return 1.0 / (1.0 + pow(MATH_E, -val));
}


//
// returns "st", "nd", "rd", or "th", based on the number passed in
//
const char *numth(int num) {
  if(num != 11 && num % 10 == 1)
    return "st";
  else if(num != 12 && num % 10 == 2)
    return "nd";
  else if(num != 13 && num % 10 == 3)
    return "rd";
  else
    return "th";
}


//
// Strips all occurances of a word from the string
//
// used to just replace occurances of 'word' with nothing,
// but this doesn't work so well since replace_string will
// replace it, even if the 'word' is embedded inside of another
// word. This is undesirable.
//
//  replace_string(&string, word, "", TRUE);
//
void strip_word(char *string, const char *word) {
  int word_len = strlen(word);
  int i = 0;

  for(i = 0; string[i] != '\0'; i++) {

    // skip all spaces and newline stuff
    while(isspace(string[i]) || string[i] == '\n' || string[i] == '\r')
      i++;

    // we've found a match
    if(!strncmp(string+i, word, word_len) &&
       (string[i+word_len] == '\0' || isspace(string[i+word_len]) ||
	string[i+word_len] == '\n' || string[i+word_len] == '\r')) {

      // count all of the spaces following us, so we can remove them too
      int space_count = 0;
      while(isspace(string[i+word_len+space_count]))
	space_count++;

      // shuffle everything down
      int j = i-1;
      do {
	j++;
	string[j] = string[j+word_len+space_count];
      } while(string[j+word_len+space_count] != '\0');
    }
  }
}


char *tag_keywords(const char *string, const char *keywords, 
		   const char *start_tag, const char *end_tag) {
  char buf[strlen(string) * 2];
  int i = 0, j = 0;
  int start_len = strlen(start_tag);
  int end_len   = strlen(end_tag);

  while(*string) {
    int keyword_len;
    // check for a match
    if( (keyword_len = find_keyword(keywords, string)) != 0) {
      for(j = 0; j < start_len; j++)
	buf[i++] = start_tag[j];
      while(keyword_len > 0) {
	buf[i++] = *string;
	string++;
	keyword_len--;
      }
      for(j = 0; j < end_len; j++)
	buf[i++] = end_tag[j];
    }

    // skip ahead one word
    else {
      while(!isspace(*string) && *string != '\0') {
	buf[i++] = *string;
	string++;
      }
      // put in our space
      if(*string != '\0') {
	buf[i++] = *string;
	string++;
      }
    }
  }
  buf[i++] = '\0';//dialogue++;

  return strdup(buf);
}


void buf_tag_keywords(BUFFER *buf, const char *keywords, const char *start_tag,
		      const char *end_tag) {
  char *new_str = tag_keywords(bufferString(buf), keywords, start_tag, end_tag);
  bufferClear(buf);
  bufferCat(buf, new_str);
  free(new_str);
}

bitvector_t parse_bits(const char *string) {
  bitvector_t bits = 0;

  while(*string) {
    SET_BIT(bits, (1 << (*string-'a')));
    string = string+1;
  }

  return bits;
}

const char *write_bits(bitvector_t bits) {
  static char buf[SMALL_BUFFER]; // really, we only need about 64...
  int buf_i = 0, i = 0;

  for(i = 0; i < BITS_PER_BITVECTOR; i++)
    if(IS_SET(bits, (1 << i)))
      buf[buf_i++] = 'a'+i;

  buf[buf_i] = '\0';
  return buf;
}


void print_bits(bitvector_t bits, const char **names, char *buf) {
  int i, count = 0;
  for(i = 0; i < BITS_PER_BITVECTOR; i++) {
    if(IS_SET(bits, (1 << i))) {
      count++;
      sprintf(buf, "%s%s%s",
	      (count == 1 ? "" : buf ),
	      (count == 1 ? "" : ", "),
	      names[i]);
    }
  }

  if(count == 0)
    sprintf(buf, "NOBITS");
}


char *print_list(LIST *list, void *descriptor, void *multi_descriptor) {
  const char             *(* desc_func)(void *) = descriptor;
  const char            *(* multi_desc)(void *) = multi_descriptor;

  if(listSize(list) == 0)
    return strdup("");
  
  char buf[MAX_BUFFER] = "";
  char one_thing[SMALL_BUFFER];
  int i;
  int tot_things = 0;
  int size = listSize(list);
  int counts[size];
  void *things[size];
  void *thing;
  LIST_ITERATOR *thing_i = newListIterator(list);

  for(i = 0; i < size; i++) {
    counts[i] = 0;
    things[i] = NULL;
  }

  // first, collect groups of all the things in the list
  ITERATE_LIST(thing, thing_i) {
    // see if we have one with the current name already
    for(i = 0; i < size; i++) {
      // it's a thing with a new name
      if(things[i] == NULL) {
	things[i] = thing;
	counts[i]++;
	tot_things++;
	break;
      }
      // it has the same name as something else
      else if(!strcmp(desc_func(thing), desc_func(things[i]))) {
	counts[i]++;
	break;
      }
    }
  } deleteListIterator(thing_i);


  // now, print everything to the buffer
  for(i = 0; i < tot_things; i++) {
    if(counts[i] == 1)
      sprintf(one_thing, "%s", desc_func(things[i]));
    else if(multi_desc == NULL || !*multi_desc(things[i]))
      sprintf(one_thing, "(%d) %s", counts[i], desc_func(things[i]));
    else
      sprintf(one_thing, multi_desc(things[i]), counts[i]);

    // we're at the last one... we have to print an and
    if(i == tot_things - 1)
      sprintf(buf, "%s%s%s", buf,
	      (i < 1 ? "" : (i == 1 ? " and " : ", and ")),
	      one_thing);
    // regular comma-separated dealy
    else
      sprintf(buf, "%s%s%s", 
	      (i == 0 ? "" : buf), (i == 0 ? "" : ", "), one_thing);
  }

  return strdup(buf);
}


void show_list(CHAR_DATA *ch, LIST *list, void *descriptor, 
	       void *multi_descriptor) {
  const char             *(* desc_func)(void *) = descriptor;
  const char            *(* multi_desc)(void *) = multi_descriptor;

  int i;
  int size = listSize(list);
  int counts[size];
  void *things[size];
  void *thing;
  LIST_ITERATOR *thing_i = newListIterator(list);

  for(i = 0; i < size; i++) {
    counts[i] = 0;
    things[i] = NULL;
  }

  // find a list of all things with unique names
  ITERATE_LIST(thing, thing_i) {
    // see if we have one with the current name already
    for(i = 0; i < size; i++) {
      // it's a thing with a new name
      if(things[i] == NULL) {
	things[i] = thing;
	counts[i]++;
	break;
      }
      // it has the same name as something else
      else if(!strcmp(desc_func(thing), desc_func(things[i]))) {
	counts[i]++;
	break;
      }
    }
  } deleteListIterator(thing_i);


  // print out all of the things
  for(i = 0; i < size; i++) {
    // we've run into the end of the list
    if(things[i] == NULL)
      break;
    else {
      if(counts[i] == 1)
	send_to_char(ch, "{n%s\r\n", desc_func(things[i]));
      else if(multi_desc == NULL || !*multi_desc(things[i]))
	send_to_char(ch, "{n(%d) %s\r\n", counts[i], desc_func(things[i]));
      else {
	char fmt[SMALL_BUFFER];
	sprintf(fmt, "{n%s\r\n", multi_desc(things[i]));
	send_to_char(ch, fmt, counts[i]);
      }
    }
  }
}


LIST *get_unused_items(CHAR_DATA *ch, LIST *list, bool invis_ok) {
  OBJ_DATA *obj;
  LIST *newlist = newList();
  LIST_ITERATOR *obj_i = newListIterator(list);

  ITERATE_LIST(obj, obj_i) {
    if(listSize(objGetUsers(obj)) != 0)
      continue;
    if(!(invis_ok || can_see_obj(ch, obj)))
      continue;

    listPut(newlist, obj);
  } deleteListIterator(obj_i);
  return newlist;
}

LIST *get_used_items(CHAR_DATA *ch, LIST *list, bool invis_ok) {
  OBJ_DATA *obj;
  LIST *newlist = newList();
  LIST_ITERATOR *obj_i = newListIterator(list);

  ITERATE_LIST(obj, obj_i) {
    if(listSize(objGetUsers(obj)) < 1)
      continue;
    if(!(invis_ok || can_see_obj(ch, obj)))
      continue;
    listPut(newlist, obj);
  } deleteListIterator(obj_i);
  return newlist;
}

bool has_obj(CHAR_DATA *ch, const char *prototype) {
  LIST *vis_inv = find_all_objs(ch, charGetInventory(ch), NULL, prototype,TRUE);
  int   ret_val = FALSE;
  if(listSize(vis_inv) > 0)
    ret_val = TRUE;
  deleteList(vis_inv);
  return ret_val;
}

void *identity_func(void *data) {
  return data;
}

LIST *reverse_list(LIST *list) {
  LIST         *newlist = newList();
  LIST_ITERATOR *list_i = newListIterator(list);
  void            *elem = NULL;
  ITERATE_LIST(elem, list_i) {
    listPut(newlist, elem);
  } deleteListIterator(list_i);
  return newlist;
}

bool parse_worldkey(const char *key, char *name, char *locale) {
  *name = *locale = '\0';
  int pos = next_letter_in(key, '@');
  if(pos == -1)
    return FALSE;
  else {
    strncpy(name, key, pos);
    *(name+pos) = '\0';
    strcpy(locale, key+pos+1);
    if(*name == '\0' || *locale == '\0')
      return FALSE;
    return TRUE;
  }
}

bool parse_worldkey_relative(CHAR_DATA *ch, const char *key, char *name, 
			     char *locale) {
  int pos = next_letter_in(key, '@');
  if(pos != -1)
    return parse_worldkey(key, name, locale);
  else {
    strcpy(name, key);
    strcpy(locale, get_key_locale(roomGetClass(charGetRoom(ch))));
    return TRUE;
  }
}

const char *get_key_locale(const char *key) {
  int pos = next_letter_in(key, '@');
  if(pos == -1)
    return "";
  else
    return key+pos+1;
}

const char *get_key_name(const char *key) {
  int pos = next_letter_in(key, '@');
  if(pos == -1)
    return key;
  else {
    static char name[SMALL_BUFFER];
    strcpy(name, key);
    name[pos] = '\0';
    return name;
  }
}

const char *get_fullkey(const char *name, const char *locale) {
  static char full_key[SMALL_BUFFER];
  sprintf(full_key, "%s@%s", name, locale);
  return full_key;
}

const char *get_fullkey_relative(const char *key, const char *locale) {
  int pos = next_letter_in(key, '@');
  if(pos > 0)
    return key;
  else {
    static char full_key[SMALL_BUFFER];
    sprintf(full_key, "%s@%s", key, locale);
    return full_key;
  }
}

const char *get_shortkey(const char *key, const char *to) {
  // are we missing a locale?
  int pos = next_letter_in(key, '@');
  if(pos < 0)
    return key;

  // if 'to' doesn't exist, make sure we return the full thing back.
  // Only relevant for attaching triggers to characters instead of mobiles
  if(!*to)
    return key;

  // is 'to' missing a locale?
  pos = next_letter_in(to, '@');
  if(pos < 0)
    return get_key_name(key);

  // two keys, both with a name and a locale. See if our locales match up
  static char name[SMALL_BUFFER];
  static char locale[SMALL_BUFFER];
  parse_worldkey(key, name, locale);
  if(!strcmp(locale, get_key_locale(to)))
    return name;

  // different locales, return the full key
  return key;
}

bool key_malformed(const char *key) {
  int at_count = 0;
  for(; *key; *key++) {
    if(*key == '@') {
      if(at_count > 0)
	return TRUE;
      at_count++;
    }
    else if(!(*key == '_' || isalpha(*key) || isdigit(*key)))
      return TRUE;
  }
  return FALSE;
}

bool locale_malformed(const char *key) {
  for(; *key; *key++) {
    if(!(isalpha(*key) || isdigit(*key) || *key == '_'))
      return TRUE;
  }
  return FALSE;
}

bool cmd_matches(const char *pattern, const char *cmd) {
  int len = next_letter_in(pattern, '*');
  // we have to match exactly
  if(len == -1)
    return !strcasecmp(pattern, cmd);
  else if(len == 0)
    return TRUE;
  else
    return !strncasecmp(pattern, cmd, len);
}

bool charHasMoreUserGroups(CHAR_DATA *ch1, CHAR_DATA *ch2) {
  return (bitIsAllSet(charGetUserGroups(ch1),
		      bitvectorGetBits(charGetUserGroups(ch2))) &&
	  !bitIsAllSet(charGetUserGroups(ch2),
		       bitvectorGetBits(charGetUserGroups(ch1))));
}

bool canEditZone(ZONE_DATA *zone, CHAR_DATA *ch) {
  return (!charIsNPC(ch) && 
	  (is_keyword(zoneGetEditors(zone), charGetName(ch), FALSE) ||
	   bitIsOneSet(charGetUserGroups(ch), "admin")));
}

bool file_exists(const char *fname) {
  FILE *fl = fopen(fname, "r");
  if(fl == NULL) return FALSE;
  fclose(fl);
  return TRUE;
}

bool dir_exists(const char *dname) {
  DIR *dir = opendir(dname);
  if(dir == NULL) return FALSE;
  closedir(dir);
  return TRUE;
}

void show_prompt(SOCKET_DATA *socket) {
  if(socketGetChar(socket))
    text_to_buffer(socket, custom_prompt(socketGetChar(socket)));
}

const char *custom_prompt(CHAR_DATA *ch) {
  static char prompt[SMALL_BUFFER];
  *prompt = '\0';
  strcat(prompt, "\r\n{nprompt> ");    
  return prompt;
}

//
// Delete an object, room, mobile, etc... from the game. First remove it from
// the gameworld database, and then delete it and its contents. The onus is on
// the builder to make sure deleting a prototype won't screw anything up (e.g.
// people standing about in a room when it's deleted).
bool do_delete(CHAR_DATA *ch, const char *type, void *deleter, const char *arg){
  void  (* delete_func)(void *data) = deleter;
  void      *data = NULL;
  const char *key = get_fullkey_relative(arg, 
		        get_key_locale(roomGetClass(charGetRoom(ch))));
  ZONE_DATA *zone = worldGetZone(gameworld, get_key_locale(key));
  
  if(!arg || !*arg)
    send_to_char(ch, "Which %s did you want to delete?\r\n", type);
  else if(key_malformed(arg))
    send_to_char(ch, "The %s key you entered was malformed.\r\n", type);
  else if(zone == NULL)
    send_to_char(ch, "No such zone exists.\r\n");
  else if(!canEditZone(zone, ch))
    send_to_char(ch, "You are not authorized to edit that zone.\r\n");
  else if((data = worldRemoveType(gameworld, type, key)) == NULL)
    send_to_char(ch, "The %s you tried to delete does not exist.\r\n", type);
  else {
    send_to_char(ch, "You remove the %s %s from the world database.\r\n", 
		 key, type);
    delete_func(data);
    return TRUE;
  }
  return FALSE;
}

//
// generic xxxlist for builders.
void do_list(CHAR_DATA *ch, const char *locale, const char *type, 
	     const char *header, void *informer) {
  if(locale_malformed(locale)) {
    send_to_char(ch, "You supplied a malformed zone name.\r\n");
    return;
  }

  ZONE_DATA *zone = worldGetZone(gameworld, locale);
  if(zone == NULL)
    send_to_char(ch, "No such zone exists.\r\n");
  else {
    const char *(* info_func)(void *) = informer;
    LIST *name_list = zoneGetTypeKeys(zone, type);
    listSortWith(name_list, strcasecmp);
    LIST_ITERATOR *name_i = newListIterator(name_list);
    char            *name = NULL;
    BUFFER           *buf = newBuffer(1);
    void            *data = NULL;
    bprintf(buf, " {wKey %74s \r\n"
"{b--------------------------------------------------------------------------------{n\r\n", header);
    ITERATE_LIST(name, name_i) {
      data = worldGetType(gameworld, type, get_fullkey(name, locale));
      if(data != NULL)
	bprintf(buf, " {c%-20s %57s{n\r\n", name, 
		(info_func ? info_func(zoneGetType(zone, type, name)) : ""));
    } deleteListIterator(name_i);
    deleteListWith(name_list, free);
    page_string(charGetSocket(ch), bufferString(buf));
    deleteBuffer(buf);
  }
}

//
// moves something of the given type with one name to the other name
bool do_rename(CHAR_DATA *ch,const char *type,const char *from,const char *to) {
  // make sure we can edit all of the zones involved
  const char  *locale = get_key_locale(roomGetClass(charGetRoom(ch)));
  void           *tgt = NULL;
  ZONE_DATA     *zone = NULL;

  // make sure everything is formed properly
  if(key_malformed(from) || key_malformed(to))
    send_to_char(ch, "You entered a malformed %s key.\r\n", type);
  // make sure "to" does not already exist, and that we have zone editing privs
  else if((zone = worldGetZone(gameworld, 
	     get_key_locale(get_fullkey_relative(to, locale)))) == NULL)
    send_to_char(ch, "Destination zone does not exist!\r\n");
  else if(!canEditZone(zone, ch))
    send_to_char(ch, "You cannot edit the destination zone.\r\n");
  else if(worldGetType(gameworld, type, get_fullkey_relative(to, locale)))
    send_to_char(ch, "%s already exists!\r\n", to);
  else {
    // make sure "from" exists, and that we have zone editing privs
    if((zone = worldGetZone(gameworld, 
	       get_key_locale(get_fullkey_relative(from, locale)))) == NULL)
      send_to_char(ch, "The originating zone does not exist!\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You do not have editing priviledges for %s!\r\n", from);
    else if( (tgt = worldRemoveType(gameworld, type, 
                    get_fullkey_relative(from, locale))) == NULL)
      send_to_char(ch, "%s %s does not exist.\r\n", from, type);
    else {
      send_to_char(ch, "%s %s renamed to %s.\r\n", from, type, to);
      worldPutType(gameworld, type, get_fullkey_relative(to, locale), tgt);
      worldSaveType(gameworld, type, get_fullkey_relative(to, locale));
      return TRUE;
    }
  }

  // failed!
  return FALSE;
}
