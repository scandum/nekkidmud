//*****************************************************************************
//
// inform.c
//
// These are all of the functions that are used for listing off processed
// information about things (e.g. room descriptions + contents, shopping
// lists, etc...)
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "character.h"
#include "object.h"
#include "world.h"
#include "room.h"
#include "exit.h"
#include "extra_descs.h"
#include "body.h"
#include "races.h"
#include "handler.h"
#include "socket.h"
#include "log.h"
#include "inform.h"
#include "hooks.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "items/items.h"
#include "items/furniture.h"



//*****************************************************************************
// local functions
//*****************************************************************************

//
// Show a character who is all sitting at one piece of furniture
//
void list_one_furniture(CHAR_DATA *ch, OBJ_DATA *furniture) {
  LIST *can_see = find_all_chars(ch,objGetUsers(furniture), "", NULL, TRUE);
  listRemove(can_see, ch);

  char *chars = print_list(can_see, charGetName, charGetMultiName);
  if(*chars) send_to_char(ch, "{n%s %s %s %s%s.\r\n",
			  chars, (listSize(can_see) == 1 ? "is" : "are"),
			  (furnitureGetType(furniture)==FURNITURE_AT?"at":"on"),
			  objGetName(furniture),
			  (charGetFurniture(ch) == furniture ?" with you": ""));
  // everyone was invisible to us... we should still show the furniture though
  else
    send_to_char(ch, "{n%s\r\n", objGetRdesc(furniture));
  deleteList(can_see);
  free(chars);
}


//
// Lists furniture that is assumed to be used. 
//
void list_used_furniture(CHAR_DATA *ch, LIST *furniture) {
  OBJ_DATA *obj;
  LIST_ITERATOR *obj_i = newListIterator(furniture);

  ITERATE_LIST(obj, obj_i)
    // hmmm... how should we handle invisible furniture?
    list_one_furniture(ch, obj);
  deleteListIterator(obj_i);
}


//
// Get a sublist of characters not using furniture. The returned list must
// be deleted after use.
//
LIST *get_nofurniture_chars(CHAR_DATA *ch, LIST *list, 
			    bool invis_ok, bool include_self) {
  LIST *newlist = newList();
  LIST_ITERATOR *char_i = newListIterator(list);
  CHAR_DATA *i = NULL;

  ITERATE_LIST(i, char_i) {
    // don't show ourself
    if(i == ch && !include_self) continue;
    // check for invis and hidden ...
    if(!(invis_ok || can_see_char(ch, i)))
      continue;
    // make sure they're not on furniture
    if(charGetFurniture(i))
      continue;
    listPut(newlist, i);
  };
  deleteListIterator(char_i);
  return newlist;
}


//
// uhhh... "contents" isn't really the right word, since we list both
// objects AND characters. Alas, my lexicon is not as verbose as it
// could be.
//
void list_room_contents(CHAR_DATA *ch, ROOM_DATA *room) {
  LIST *list = NULL;

  list = get_nofurniture_chars(ch, roomGetCharacters(room), FALSE, FALSE);
  show_list(ch, list, charGetRdesc, charGetMultiRdesc);
  deleteList(list);

  // show all of the objects that have people using them
  list = get_used_items(ch, roomGetContents(room), FALSE);
  list_used_furniture(ch, list);
  deleteList(list);

  // show all of the objects that don't have people using them that we can see
  list = get_unused_items(ch, roomGetContents(room), FALSE);
  show_list(ch, list, objGetRdesc, objGetMultiRdesc);
  deleteList(list);
}



//*****************************************************************************
// implementaiton of inform.h
// look_at_xxx and show_xxx functions.
//*****************************************************************************
void look_at_obj(CHAR_DATA *ch, OBJ_DATA *obj) {
  // make our working copy of the description
  bufferClear(charGetLookBuffer(ch));
  bufferCat(charGetLookBuffer(ch), objGetDesc(obj));

  // do all of the preprocessing on the new descriptions
  hookRun("preprocess_obj_desc", hookBuildInfo("obj ch", obj, ch));

  // append anything that might also go onto it
  hookRun("append_obj_desc", hookBuildInfo("obj ch", obj, ch));

  // colorize all of the edescs
  edescTagDesc(charGetLookBuffer(ch), objGetEdescs(obj), "{c", "{n");

  // format the desc, and send it
  bufferFormat(charGetLookBuffer(ch), SCREEN_WIDTH, PARA_INDENT);

  if(bufferLength(charGetLookBuffer(ch)) == 0)
    send_to_char(ch, "{n%s\r\n", NOTHING_SPECIAL);
  else
    send_to_char(ch, "{n%s", bufferString(charGetLookBuffer(ch)));

  hookRun("look_at_obj", hookBuildInfo("obj ch", obj, ch));
  send_to_char(ch, "{n");
}


void look_at_exit(CHAR_DATA *ch, EXIT_DATA *exit) {
  // make the working copy of the description, and fill it up with info
  bufferClear(charGetLookBuffer(ch));
  bufferCat(charGetLookBuffer(ch), exitGetDesc(exit));

  // do all of our preprocessing of the description before we show it
  hookRun("preprocess_exit_desc", hookBuildInfo("ex ch", exit, ch));

  // append anything that might also go onto it
  hookRun("append_exit_desc", hookBuildInfo("ex ch", exit, ch));

  // colorize all of the edescs
  edescTagDesc(charGetLookBuffer(ch), roomGetEdescs(exitGetRoom(exit)), 
	       "{c", "{n");

  // format our description
  bufferFormat(charGetLookBuffer(ch), SCREEN_WIDTH, PARA_INDENT);

  // if the buffer has nothing in it, send a "nothing special" message
  if(bufferLength(charGetLookBuffer(ch)) == 0)
    send_to_char(ch, "{n%s\r\n", NOTHING_SPECIAL);
  else
    send_to_char(ch, "{n%s", bufferString(charGetLookBuffer(ch)));

  hookRun("look_at_exit", hookBuildInfo("ex ch", exit, ch));
  send_to_char(ch, "{n");
}

//
// shows a single exit to a character
void list_one_exit(CHAR_DATA *ch, EXIT_DATA *exit, const char *dir) {
  char   buf[100] = "\0"; // for the room class
  ROOM_DATA *dest = worldGetRoom(gameworld, exitGetToFull(exit));

  if(bitIsOneSet(charGetUserGroups(ch), "builder"))
    snprintf(buf, 100, " [%s]", roomGetClass(dest));

  send_to_char(ch, "{n  %-10s :: %s%s\r\n", dir, buf, 
	       (exitIsClosed(exit) ? 
		// if it's closed, print the exit name
		(*exitGetName(exit) ? exitGetName(exit) : "closed" ) :
		// if it's open, print where it leads to
		roomGetName(dest)));
}


void list_room_exits(CHAR_DATA *ch, ROOM_DATA *room) {
  EXIT_DATA     *exit = NULL;
  ROOM_DATA       *to = NULL;
  int               i = 0;
  LIST       *ex_list = roomGetExitNames(room);
  LIST_ITERATOR *ex_i = newListIterator(ex_list);
  char           *dir = NULL;

  // first, we list all of the normal exit
  for(i = 0; i < NUM_DIRS; i++) {
    if( (exit = roomGetExit(room, dirGetName(i))) != NULL) {
      // make sure the destination exists
      if( (to = worldGetRoom(gameworld, exitGetToFull(exit))) == NULL)
	log_string("ERROR: room %s heads %s to room %s, which does not exist.",
		   roomGetClass(room), dirGetName(i), exitGetTo(exit));
      else if(can_see_exit(ch, exit))
	list_one_exit(ch, exit, dirGetName(i));
    }
  }

  // next, we list all of the special exits
  ITERATE_LIST(dir, ex_i) {
    if(dirGetNum(dir) == DIR_NONE) {
      exit = roomGetExit(room, dir);
      // make sure the destination exists
      if( (to = worldGetRoom(gameworld, exitGetToFull(exit))) == NULL)
	log_string("ERROR: room %s heads %s to room %s, which does not exist.",
		   roomGetClass(room), dir, exitGetTo(exit));
      else if(can_see_exit(ch, exit))
	list_one_exit(ch, exit, dir);
    }
  } deleteListIterator(ex_i);
  deleteListWith(ex_list, free);
}


void look_at_char(CHAR_DATA *ch, CHAR_DATA *vict) {
  bufferClear(charGetLookBuffer(ch));
  bufferCat(charGetLookBuffer(ch), charGetDesc(vict));

  // preprocess our desc before it it sent to the person
  hookRun("preprocess_char_desc", hookBuildInfo("ch ch", vict, ch));

  // append anything that might also go onto it
  hookRun("append_char_desc", hookBuildInfo("ch ch", vict, ch));

  // format and send it
  bufferFormat(charGetLookBuffer(ch), SCREEN_WIDTH, PARA_INDENT);

  if(bufferLength(charGetLookBuffer(ch)) == 0)
    send_to_char(ch, "{n%s\r\n", NOTHING_SPECIAL);
  else
    send_to_char(ch, "{n%s{n", bufferString(charGetLookBuffer(ch)));

  hookRun("look_at_char", hookBuildInfo("ch ch", vict, ch));
}


void look_at_room(CHAR_DATA *ch, ROOM_DATA *room) {
  if(bitIsOneSet(charGetUserGroups(ch), "builder"))
    send_to_char(ch, "{c[%s] [%s] ", roomGetClass(room), 
		 terrainGetName(roomGetTerrain(room)));

  send_to_char(ch, "{c%s\r\n", roomGetName(room));

  // make the working copy of the description, and fill it up with info
  bufferClear(charGetLookBuffer(ch));
  bufferCat(charGetLookBuffer(ch), roomGetDesc(room));

  // do all of our preprocessing of the description before we show it
  hookRun("preprocess_room_desc", hookBuildInfo("rm ch", room, ch));

  // append anything that might also go onto it
  hookRun("append_room_desc", hookBuildInfo("rm ch", room, ch));

  // colorize all of the edescs
  edescTagDesc(charGetLookBuffer(ch), roomGetEdescs(room), "{c", "{n");

  // format our description
  bufferFormat(charGetLookBuffer(ch), SCREEN_WIDTH, PARA_INDENT);

  // do any post-processing we might have
  hookRun("postprocess_room_desc", hookBuildInfo("rm ch", room, ch));

  if(bufferLength(charGetLookBuffer(ch)) == 0)
    send_to_char(ch, "{n%s\r\n", NOTHING_SPECIAL);
  else
    send_to_char(ch, "{n%s", bufferString(charGetLookBuffer(ch)));

  hookRun("look_at_room", hookBuildInfo("rm ch", room, ch));
  send_to_char(ch, "{n");
}



//*****************************************************************************
//
// implementaiton of inform.h
// send_to_xxx functions
//
//*****************************************************************************
void send_outdoors(const char *format, ...) {
  if(format && *format) {
    // form the message
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, MAX_BUFFER, format, args);
    va_end(args);

    // send it out to everyone
    LIST_ITERATOR *list_i = newListIterator(mobile_list);
    CHAR_DATA *ch = NULL;
    ITERATE_LIST(ch, list_i)
      if(charGetRoom(ch) != NULL &&
	 roomGetTerrain(charGetRoom(ch)) != TERRAIN_INDOORS &&
	 roomGetTerrain(charGetRoom(ch)) != TERRAIN_CAVERN)
	text_to_char(ch, buf);
    deleteListIterator(list_i);
  }
}


void text_to_char(CHAR_DATA *ch, const char *txt) {
  //  if (txt && *txt && charGetSocket(ch) && 
  //      socketGetState(charGetSocket(ch)) == STATE_PLAYING) {
  if(txt && *txt && charGetSocket(ch)) {
    text_to_buffer(charGetSocket(ch), txt);
    socketBustPrompt(charGetSocket(ch));
  }

  // if it's a PC or we are not in game, then
  // don't send the mesage to us
  if(!charIsNPC(ch))
    try_log(charGetName(ch), txt);
}


void send_to_char(CHAR_DATA *ch, const char *format, ...) {
  if(charGetSocket(ch) && format && *format) {
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, MAX_BUFFER, format, args);
    va_end(args);
    text_to_char(ch, buf);
    hookRun("char_receive_text", hookBuildInfo("ch str", ch, buf));
    return;
  }
}


void send_around_char(CHAR_DATA *ch, bool hide_nosee, const char *format, ...) {
  static char buf[MAX_BUFFER];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, MAX_BUFFER, format, args);
  va_end(args);

  LIST_ITERATOR *room_i = newListIterator(roomGetCharacters(charGetRoom(ch)));
  CHAR_DATA       *vict = NULL;

  ITERATE_LIST(vict, room_i) {
    if(ch == vict)
      continue;
    if(hide_nosee && !can_see_char(vict, ch))
      continue;
    text_to_char(vict, buf);
  }
  deleteListIterator(room_i);
  return;
}


void send_to_groups(const char *groups, const char *format, ...) {
  static char buf[MAX_BUFFER];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, MAX_BUFFER, format, args);
  va_end(args);

  LIST_ITERATOR *ch_i = newListIterator(mobile_list);
  CHAR_DATA       *ch = NULL;

  ITERATE_LIST(ch, ch_i) {
    if(!charGetSocket(ch) || !bitIsSet(charGetUserGroups(ch), groups))
      continue;
    text_to_char(ch, buf);
  } deleteListIterator(ch_i);
}


void send_to_list(LIST *list, const char *format, ...) {
  if(format && *format) {
    // form the message
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, MAX_BUFFER, format, args);
    va_end(args);

    // send it out to everyone
    LIST_ITERATOR *list_i = newListIterator(list);
    CHAR_DATA *ch = NULL;
    ITERATE_LIST(ch, list_i)
      text_to_char(ch, buf);
    deleteListIterator(list_i);
  }
};



//*****************************************************************************
//
// implementation of inform.h
// game commands
//
//*****************************************************************************

//
// cmd_more skips onto the next page in the socket's read buffer
//
COMMAND(cmd_more) {
  if(charGetSocket(ch))
    page_continue(charGetSocket(ch));
}


//
// cmd_back goes back to the previous page in the socket's read buffer
//
COMMAND(cmd_back) {
  if(charGetSocket(ch))
    page_back(charGetSocket(ch));
}


//
// lists all of the commands available to a character via some group. If the
// character is not part of the group, s/he cannot see the commands
COMMAND(cmd_groupcmds) {
  if(!*arg)
    send_to_char(ch, "Which group did you want to commands for: %s\r\n",
		 bitvectorGetBits(charGetUserGroups(ch)));
  else if(!bitIsOneSet(charGetUserGroups(ch), arg))
    send_to_char(ch, "You are not a member of user group, %s.\r\n", arg);
  else
    show_commands(ch, arg);
}


//
// cmd_look lets a character get information about a specific thing. Objects,
// characters, extra descriptions, and just about anything else that can exist
// in the MUD can be looked at.
//   usage: look [at] <target> [[<on> <thing>] [<in> <thing>]]
//
//   examples:
//     look bob                      look at bob
//     look at 2.woman               look at the 2nd woman
//     look at sword in sheath       look at the sword in the sheath
//     look at earrings on sue       look at the earrings sue is wearing
COMMAND(cmd_look) {
  if(!arg || !*arg)
    look_at_room(ch, charGetRoom(ch));
  else {
    int found_type = FOUND_NONE;
    void *found = generic_find(ch, arg, FIND_TYPE_ALL, FIND_SCOPE_IMMEDIATE,
			       FALSE, &found_type);

    // nothing!
    if(found == NULL)
      send_to_char(ch, "What did you want to look at?\r\n");

    // is it an extra description?
    else if(found_type == FOUND_EDESC) {
      EDESC_SET *set = edescGetSet(found);
      BUFFER  *edesc = bufferCopy(edescGetDescBuffer(found));
      edescTagDesc(edesc, set, "{c", "{n");
      bufferFormat(edesc, SCREEN_WIDTH, PARA_INDENT);
      send_to_char(ch, "%s", bufferString(edesc));
      deleteBuffer(edesc);
    }

    // is it an item?
    else if(found_type == FOUND_OBJ || found_type == FOUND_IN_OBJ)
      look_at_obj(ch, found);

    // is it another character?
    else if(found_type == FOUND_CHAR)
      look_at_char(ch, found);

    // is it an exit?
    else if(found_type == FOUND_EXIT)
      look_at_exit(ch, found);

    // couldn't find anything. too bad!
    else
      send_to_char(ch, "What did you want to at?\r\n");

  }
}


//
// show a list of all commands available to the character
COMMAND(cmd_commands) {
  if(!*arg)
    show_commands(ch, bitvectorGetBits(charGetUserGroups(ch)));
  else if(!bitIsAllSet(charGetUserGroups(ch), arg))
    send_to_char(ch, "You are not a member of all user groups: %s.\r\n", arg);
  else
    show_commands(ch, arg);
}



//*****************************************************************************
// below this line are all of the subfunctions related to the message() 
// function
//*****************************************************************************

//
// Send a message out
//
// Converts the following symbols:
//  $n = ch name
//  $N = vict name
//  $m = him/her of char
//  $M = him/her of vict
//  $s = his/hers of char
//  $S = his/hers of vict
//  $e = he/she of char
//  $E = he/she of vict
//
//  $o = obj name
//  $O = vobj name
//  $a = a/an of obj
//  $A = a/an of vobj
void send_message(CHAR_DATA *to, 
		  const char *str,
		  CHAR_DATA *ch, CHAR_DATA *vict,
		  OBJ_DATA *obj, OBJ_DATA *vobj) {
  static char buf[MAX_BUFFER];
  int i, j;
  *buf = '\0';

  // if there's nothing to send the message to, don't go through all
  // the work it takes to parse the string
  if(charGetSocket(to) == NULL)
    return;

  for(i = 0, j = 0; str[i] != '\0'; i++) {
    if(str[i] != '$') {
      buf[j] = str[i];
      j++;
    }
    else {
      i++;

      switch(str[i]) {
      case 'n':
	if(!ch) break;
	sprintf(buf+j, see_char_as(to, ch));
	while(buf[j] != '\0') j++;
	break;
      case 'N':
	if(!vict) break;
	sprintf(buf+j, see_char_as(to, vict));
	while(buf[j] != '\0') j++;
	break;
      case 'm':
	if(!ch) break;
	sprintf(buf+j, (can_see_char(to, ch) ? HIMHER(ch) : SOMEONE));
	while(buf[j] != '\0') j++;
	break;
      case 'M':
	if(!vict) break;
	sprintf(buf+j, (can_see_char(to, vict) ? HIMHER(vict) : SOMEONE));
	while(buf[j] != '\0') j++;
	break;
      case 's':
	if(!ch) break;
	sprintf(buf+j, (can_see_char(to, ch) ? HISHER(ch) :SOMEONE"'s"));
	while(buf[j] != '\0') j++;
	break;
      case 'S':
	if(!vict) break;
	sprintf(buf+j, (can_see_char(to, vict) ? HISHER(vict) :SOMEONE"'s"));
	while(buf[j] != '\0') j++;
	break;
      case 'e':
	if(!ch) break;
	sprintf(buf+j, (can_see_char(to, ch) ? HESHE(ch) : SOMEONE));
	while(buf[j] != '\0') j++;
	break;
      case 'E':
	if(!vict) break;
	sprintf(buf+j, (can_see_char(to, vict) ? HESHE(vict) : SOMEONE));
	while(buf[j] != '\0') j++;
	break;
      case 'o':
	if(!obj) break;
	sprintf(buf+j, see_obj_as(to, obj));
	while(buf[j] != '\0') j++;
	break;
      case 'O':
	if(!vobj) break;
	sprintf(buf+j, see_obj_as(to, vobj));
	while(buf[j] != '\0') j++;
	break;
      case 'a':
	if(!obj) break;
	sprintf(buf+j, AN(see_obj_as(to, obj)));
	while(buf[j] != '\0') j++;
	break;
      case 'A':
	if(!vobj) break;
	sprintf(buf+j, AN(see_obj_as(to, vobj)));
	while(buf[j] != '\0') j++;
	break;
      case '$':
	buf[j] = '$';
	j++;
	break;
      default:
	// do nothing
	break;
      }
    }
  }

  //  buf[0] = toupper(buf[0]);
  sprintf(buf+j, "{n\r\n");
  text_to_char(to, buf);
}


void message(CHAR_DATA *ch,  CHAR_DATA *vict,
	     OBJ_DATA  *obj, OBJ_DATA  *vobj,
	     int hide_nosee, bitvector_t range, 
	     const char *mssg) {
  if(!mssg || !*mssg)
    return;

  // what's our scope?
  if(IS_SET(range, TO_VICT) && vict &&
     (!hide_nosee ||
      // make sure the vict can the character, or the
      // object if there is no character
      ((!ch || can_see_char(vict, ch)) &&
       (ch  || (!obj || can_see_obj(vict, obj))))))
    send_message(vict, mssg, ch, vict, obj, vobj);

  // characters can always see themselves. No need to do checks here
  if(IS_SET(range, TO_CHAR) && ch)
    send_message(ch, mssg, ch, vict, obj, vobj);

  LIST *recipients = NULL;
  // check if the scope of this message is everyone in the world
  if(IS_SET(range, TO_WORLD))
    recipients = mobile_list;
  else if(IS_SET(range, TO_ROOM) && charGetRoom(ch) != NULL)
    recipients = roomGetCharacters(charGetRoom(ch));

  // if we have a list to send the message to, do it
  if(recipients != NULL) {
    LIST_ITERATOR *rec_i = newListIterator(recipients);
    CHAR_DATA *rec = NULL;

    // go through everyone in the list
    ITERATE_LIST(rec, rec_i) {
      // if we wanted to send to ch or vict, we would have already...
      if(rec == vict || rec == ch)
	continue;
      // skip by people who are in the game but not in the world yet
      if(charGetRoom(rec) == NULL)
	continue;
      if(rec == ch ||
	 (!hide_nosee ||
	  // make sure the vict can see the character, or the
	  // object if there is no character
	  ((!ch || can_see_char(rec, ch)) &&
	   (ch  || (!obj || can_see_obj(rec, obj))))))
      send_message(rec, mssg, ch, vict, obj, vobj);
    } deleteListIterator(rec_i);
  }
}

void mssgprintf(CHAR_DATA *ch, CHAR_DATA *vict, 
		OBJ_DATA *obj, OBJ_DATA  *vobj,
		int hide_nosee, bitvector_t range, const char *fmt, ...) {
  if(fmt && *fmt) {
    // form the message
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, MAX_BUFFER, fmt, args);
    va_end(args);
    message(ch, vict, obj, vobj, hide_nosee, range, buf);
  }
}



//*****************************************************************************
// hooks
//*****************************************************************************

//
// appends all of our exit extra descriptions to the room description.
void exit_append_room_hook(BUFFER *buf, ROOM_DATA *room, CHAR_DATA *ch) {
  LIST       *exnames = roomGetExitNames(room);
  LIST       *ex_same = newList(); // leads to room w/ same name
  LIST       *ex_diff = newList(); // leads to room w/ diff name
  LIST     *ex_closed = newList(); // there is a closed door blocking us
  LIST_ITERATOR *ex_i = newListIterator(exnames);
  char            *ex = NULL;

  // figure out our exits that lead to same-room-name 
  // or different-room-name destinations.
  ITERATE_LIST(ex, ex_i) {
    EXIT_DATA *exit = roomGetExit(room, ex);
    ROOM_DATA *dest = worldGetRoom(gameworld, exitGetToFull(exit));
    if(dest && can_see_exit(ch, exit) && dirGetNum(ex) != DIR_NONE) {
      if(exitIsClosed(exit))
	listPut(ex_closed, ex);
      else if(!strcasecmp(roomGetName(room), roomGetName(dest)))
	listPush(ex_same, ex);
      else
	listQueue(ex_diff, ex);
    }
  } deleteListIterator(ex_i);

  // append info for dirs that are blocked by doors
  ex_i = newListIterator(ex_closed);
  ITERATE_LIST(ex, ex_i) {
    EXIT_DATA *exit = roomGetExit(room, ex);
    bprintf(buf, " %s%s, you see %s.",
	    (dirGetNum(ex) == DIR_NONE ? "At the exit " : ""), ex,
	    (*exitGetName(exit) ? exitGetName(exit) : "a door"));
  } deleteListIterator(ex_i);

  // append info for dirs that exit to other room names
  ex_i = newListIterator(ex_diff);
  ITERATE_LIST(ex, ex_i) {
    ROOM_DATA *dest = worldGetRoom(gameworld, exitGetToFull(roomGetExit(room, ex)));
    bprintf(buf, " Continuing %s would take you to %s.", ex, roomGetName(dest));
  } deleteListIterator(ex_i);

  // and now print stuff for exits that go to rooms with the name name
  if(listSize(ex_same) > 0) {
    // if we just have a couple exits, list them off
    if(listSize(ex_same) <= 3) {
      char *list = print_list(ex_same, identity_func, NULL);
      bprintf(buf, " %s continues %s.", roomGetName(room), list);
      free(list);
    }
    // else display in bulk
    else {
      bprintf(buf, " All %sdiretions continue to %s.", 
	      (listSize(ex_same) == listSize(exnames) ? "" : "other "),
	      roomGetName(room));
    }
  }

  // clean up our garbage
  deleteList(ex_diff);
  deleteList(ex_same);
  deleteList(ex_closed);
  deleteListWith(exnames, free);
}

void exit_append_hook(const char *info) {
  // before anything, figure out some basic information like our dir and dest
  EXIT_DATA     *exit = NULL;
  CHAR_DATA       *ch = NULL;
  hookParseInfo(info, &exit, &ch);

  BUFFER         *buf = charGetLookBuffer(ch);
  ROOM_DATA     *room = exitGetRoom(exit);
  ROOM_DATA     *dest = worldGetRoom(gameworld, exitGetToFull(exit));
  LIST       *exnames = roomGetExitNames(room);
  LIST_ITERATOR *ex_i = newListIterator(exnames);
  char            *ex = NULL;
  char           *dir = NULL;

  // figure out which direction we came from
  ITERATE_LIST(ex, ex_i) {
    if(roomGetExit(room, ex) == exit) {
      dir = strdup(ex);
      break;
    }
  } deleteListIterator(ex_i);
  deleteListWith(exnames, free);

  // tell us where it would take us
  if(dest && !*exitGetDesc(exit) && !exitIsClosed(exit)) {
    if(!strcasecmp(roomGetName(dest), roomGetName(room)))
      bprintf(buf, " %s continues %s.", roomGetName(dest), dir);
    else 
      bprintf(buf, " Continuing %s would take you to %s.", dir, 
	      roomGetName(dest));
  }

  // we have a door ... gotta print its status
  if(exitIsClosable(exit)) {
    bprintf(buf, " %s%s, you see %s which is currently %s.",
	    (dirGetNum(dir) == DIR_NONE ? "At the exit " : ""), dir,
	    (*exitGetName(exit) ? exitGetName(exit) : "a door"),
	    (exitIsClosed(exit) ? "closed" : "open"));
  }

  // garbage collection
  if(dir) free(dir);
}

void exit_look_hook(const char *info) {
  EXIT_DATA *exit = NULL;
  CHAR_DATA   *ch = NULL;
  hookParseInfo(info, &exit, &ch);
  // the door is not closed, list off the people we can see as well
  if(!exitIsClosed(exit)) {
    ROOM_DATA *room = worldGetRoom(gameworld, exitGetToFull(exit));
    if(room != NULL)
      list_room_contents(ch, room);
  }
}

void room_look_hook(const char *info) {
  ROOM_DATA *room = NULL;
  CHAR_DATA   *ch = NULL;
  hookParseInfo(info, &room, &ch);
  list_room_exits(ch, room);
  list_room_contents(ch, room);
}



//*****************************************************************************
// initialization of inform.h
//*****************************************************************************
void init_inform(void) {
  // attach hooks
  hookAdd("append_exit_desc", exit_append_hook);
  // enable if you want exits to append to the end of room descs
  //  hookAdd("append_room_desc", exit_append_room_hook);
  //  hookAdd("look_at_exit", exit_look_hook);
  //  hookAdd("look_at_room", room_look_hook);
}
