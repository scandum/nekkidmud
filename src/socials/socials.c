//*****************************************************************************
//
// socials.c
//
// socials are commonly used emotes (e.g. smiling, grinning, laughing). Instead
// making people have to write out an entire emote every time they would like
// to express such an emote, they can simply use one of these simple commands
// to perform a pre-made emote.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../storage.h"
#include "../handler.h"     
#include "../inform.h"
#include "../character.h"

#include "socedit.h"
#include "socials.h"



//*****************************************************************************
//
// local datastructures, defines, and functions
//
//*****************************************************************************

// the file where all of the socials are stored
#define SOCIALS_FILE "../lib/misc/socials"

// the table we store all of the socials in
HASHTABLE *social_table = NULL;


//
// save all of the socials to disk
//
void save_socials() {
  STORAGE_SET       *set = new_storage_set();
  LIST         *soc_list = newList();

  // iterate across the social table and save all of the unique socials
  HASH_ITERATOR  *hash_i = newHashIterator(social_table);
  const char        *cmd = NULL;
  SOCIAL_DATA      *data = NULL;

  ITERATE_HASH(cmd, data, hash_i)
    listPut(soc_list, data);
  deleteHashIterator(hash_i);

  store_list(set, "socials", gen_store_list(soc_list, socialStore));
  deleteList(soc_list);

  // write the set
  storage_write(set, SOCIALS_FILE);
  
  // close the set
  storage_close(set);
}



//*****************************************************************************
//
// Implementation of the social datastructure
//
//*****************************************************************************
struct social_data {
  char *cmds;          // the list of commands we fire off of
  char *to_char_notgt; // the message to us when there's no target
  char *to_room_notgt; // the message to the room when there's no target
  char *to_char_self;  // the message to ourself when tgt == us
  char *to_room_self;  // the message to the room when tgt == us
  char *to_char_tgt;   // the message to us when there's a target
  char *to_vict_tgt;   // the message to the target
  char *to_room_tgt;   // the message to the room when there's a target

  int min_pos;         // the minimum position we can perform this from
  int max_pos;         // and the maximum
};


SOCIAL_DATA *newSocial(const char *cmds,
		       const char *to_char_notgt,
		       const char *to_room_notgt,
		       const char *to_char_self,
		       const char *to_room_self,
		       const char *to_char_tgt,
		       const char *to_vict_tgt,
		       const char *to_room_tgt,
		       int min_pos, int max_pos) {
  SOCIAL_DATA *data = malloc(sizeof(SOCIAL_DATA));
  data->cmds          = strdupsafe(cmds);
  data->to_char_notgt = strdupsafe(to_char_notgt);
  data->to_room_notgt = strdupsafe(to_room_notgt);
  data->to_char_self  = strdupsafe(to_char_self);
  data->to_room_self  = strdupsafe(to_room_self);
  data->to_char_tgt   = strdupsafe(to_char_tgt);
  data->to_vict_tgt   = strdupsafe(to_vict_tgt);
  data->to_room_tgt   = strdupsafe(to_room_tgt);
  data->min_pos       = min_pos;
  data->max_pos       = max_pos;
  return data;
}


void deleteSocial(SOCIAL_DATA *data) {
  if(data->cmds)          free(data->cmds);
  if(data->to_char_notgt) free(data->to_char_notgt);
  if(data->to_room_notgt) free(data->to_room_notgt);
  if(data->to_char_self)  free(data->to_char_self);
  if(data->to_room_self)  free(data->to_room_self);
  if(data->to_char_tgt)   free(data->to_char_tgt);
  if(data->to_vict_tgt)   free(data->to_vict_tgt);
  if(data->to_room_tgt)   free(data->to_room_tgt);
  free(data);
}


STORAGE_SET *socialStore(SOCIAL_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "cmds",          data->cmds);
  store_string(set, "to_char_notgt", data->to_char_notgt);
  store_string(set, "to_room_notgt", data->to_room_notgt);
  store_string(set, "to_char_self",  data->to_char_self);
  store_string(set, "to_room_self",  data->to_room_self);
  store_string(set, "to_char_tgt",   data->to_char_tgt);
  store_string(set, "to_vict_tgt",   data->to_vict_tgt);
  store_string(set, "to_room_tgt",   data->to_room_tgt);
  store_string(set, "min_pos",       posGetName(data->min_pos));
  store_string(set, "max_pos",       posGetName(data->max_pos));
  return set;
}


SOCIAL_DATA *socialRead(STORAGE_SET *set) {
  return newSocial(read_string(set, "cmds"),
		   read_string(set, "to_char_notgt"),
		   read_string(set, "to_room_notgt"),
		   read_string(set, "to_char_self"),
		   read_string(set, "to_room_self"),
		   read_string(set, "to_char_tgt"),
		   read_string(set, "to_vict_tgt"),
		   read_string(set, "to_room_tgt"),
		   posGetNum(read_string(set, "min_pos")),
		   posGetNum(read_string(set, "max_pos")));
}


void socialCopyTo(SOCIAL_DATA *from, SOCIAL_DATA *to) {
  // free all of the strings
  if(to->cmds)          free(to->cmds);
  if(to->to_char_notgt) free(to->to_char_notgt);
  if(to->to_room_notgt) free(to->to_room_notgt);
  if(to->to_char_self)  free(to->to_char_self);
  if(to->to_room_self)  free(to->to_room_self);
  if(to->to_char_tgt)   free(to->to_char_tgt);
  if(to->to_vict_tgt)   free(to->to_vict_tgt);
  if(to->to_room_tgt)   free(to->to_room_tgt);

  // copy over all of the new descs and commands
  to->cmds          = strdupsafe(from->cmds);
  to->to_char_notgt = strdupsafe(from->to_char_notgt);
  to->to_room_notgt = strdupsafe(from->to_room_notgt);
  to->to_char_self  = strdupsafe(from->to_char_self);
  to->to_room_self  = strdupsafe(from->to_room_self);
  to->to_char_tgt   = strdupsafe(from->to_char_tgt);
  to->to_vict_tgt   = strdupsafe(from->to_vict_tgt);
  to->to_room_tgt   = strdupsafe(from->to_room_tgt);
  to->min_pos       = from->min_pos;
  to->max_pos       = from->max_pos;
}


SOCIAL_DATA *socialCopy(SOCIAL_DATA *social) {
  SOCIAL_DATA *new_soc = newSocial("", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
				   POS_STANDING, POS_STANDING);
  socialCopyTo(social, new_soc);
  return new_soc;
}

const char *socialGetCmds(SOCIAL_DATA *social) {
  return social->cmds;
}

const char *socialGetCharNotgt(SOCIAL_DATA *social) {
  return social->to_char_notgt;
}

const char *socialGetRoomNotgt(SOCIAL_DATA *social) {
  return social->to_room_notgt;
}

const char *socialGetCharSelf (SOCIAL_DATA *social) {
  return social->to_char_self;
}

const char *socialGetRoomSelf (SOCIAL_DATA *social) {
  return social->to_room_self;
}

const char *socialGetCharTgt(SOCIAL_DATA *social) {
  return social->to_char_tgt;
}

const char *socialGetVictTgt(SOCIAL_DATA *social) {
  return social->to_vict_tgt;
}

const char *socialGetRoomTgt(SOCIAL_DATA *social) {
  return social->to_room_tgt;
}

int socialGetMinPos(SOCIAL_DATA *social) {
  return social->min_pos;
}

int socialGetMaxPos(SOCIAL_DATA *social) {
  return social->max_pos;
}

void socialSetCharNotgt(SOCIAL_DATA *social, const char *mssg) {
  if(social->to_char_notgt) free(social->to_char_notgt);
  social->to_char_notgt   = strdupsafe(mssg);
}

void socialSetRoomNotgt(SOCIAL_DATA *social, const char *mssg) {
  if(social->to_room_notgt) free(social->to_room_notgt);
  social->to_room_notgt   = strdupsafe(mssg);
}

void socialSetCharSelf(SOCIAL_DATA *social, const char *mssg) {
  if(social->to_char_self) free(social->to_char_self);
  social->to_char_self   = strdupsafe(mssg);
}

void socialSetRoomSelf(SOCIAL_DATA *social, const char *mssg) {
  if(social->to_room_self) free(social->to_room_self);
  social->to_room_self   = strdupsafe(mssg);
}

void socialSetCharTgt(SOCIAL_DATA *social, const char *mssg) {
  if(social->to_char_tgt) free(social->to_char_tgt);
  social->to_char_tgt   = strdupsafe(mssg);
}

void socialSetVictTgt(SOCIAL_DATA *social, const char *mssg) {
  if(social->to_vict_tgt) free(social->to_vict_tgt);
  social->to_vict_tgt   = strdupsafe(mssg);
}

void socialSetRoomTgt(SOCIAL_DATA *social, const char *mssg) {
  if(social->to_room_tgt) free(social->to_room_tgt);
  social->to_room_tgt   = strdupsafe(mssg);
}

void socialSetMinPos(SOCIAL_DATA *social, int pos) {
  social->min_pos = pos;
}

void socialSetMaxPos(SOCIAL_DATA *social, int pos) {
  social->max_pos = pos;
}


//
// Link one social to another
//   usage: soclink [new command] [old command]
//
COMMAND(cmd_soclink) {
  char new_soc[SMALL_BUFFER];

  if(!arg || !*arg) {
    send_to_char(ch, "Link which social to which?\r\n");
    return;
  }

  arg = one_arg(arg, new_soc);

  if(!*new_soc || !*arg) {
    send_to_char(ch, "You must provide a new command and an old social "
		 "to link it to.\r\n");
    return;
  }

  // find the social we're trying to link to
  SOCIAL_DATA *social = get_social(arg);

  if(social == NULL) {
    send_to_char(ch, "No social exists for %s.\r\n", arg);
    return;
  }

  // perform the link
  link_social(new_soc, arg);
  send_to_char(ch, "%s linked to %s.\r\n", new_soc, arg);
}


//
// Unlink a social
//   usage: socunlink [command]
//
COMMAND(cmd_socunlink) {
  if(!arg || !*arg) {
    send_to_char(ch, "Unlink which social?\r\n");
    return;
  }

  // find the social we're trying to link to
  SOCIAL_DATA *social = get_social(arg);

  if(social == NULL) {
    send_to_char(ch, "No social exists for %s.\r\n", arg);
    return;
  }

  // perform the unlinking
  unlink_social(arg);
  send_to_char(ch, "The %s social was unlinked.\r\n", arg);
}


//
// List all of the socials to the character
//
COMMAND(cmd_socials) {
  HASH_ITERATOR  *soc_i = newHashIterator(social_table);
  const char       *soc = NULL;
  SOCIAL_DATA     *data = NULL;
  LIST        *soc_list = newList();
  int count             = 0;

  // go through the hashtable and pull out each key
  ITERATE_HASH(soc, data, soc_i)
    listPut(soc_list, strdup(soc));
  deleteHashIterator(soc_i);

  // now sort them, and pop them out one at a time
  listSortWith(soc_list, strcmp);

  // and pop them out one at a time to print
  while( (soc = listPop(soc_list)) != NULL) {
    count++;
    send_to_char(ch, "%-20s%s", soc, (count % 4 == 0 ? "\r\n" : ""));    
  }
  deleteListWith(soc_list, free);

  if(count == 0 || count % 4 != 0)
    send_to_char(ch, "\r\n");
}



//
// One generic command for handling socials. Does table lookup on all of
// the existing socials and executes the proper one.
//
COMMAND(cmd_social) {
  // look up the social
  SOCIAL_DATA *data = hashGet(social_table, cmd);

  // does the social exist? Do we have a problem? DO WE?
  if(data != NULL) {
    // find the target
    CHAR_DATA *tgt = NULL;
    if(*arg) {
      int tgt_type = FOUND_NONE;
      tgt = generic_find(ch, arg, FIND_TYPE_CHAR, FIND_SCOPE_IMMEDIATE,
			 FALSE, &tgt_type);
    }

    // see if we were trying to find a target but couldn't
    if(*arg && !tgt)
      send_to_char(ch, "Who were you looking for?\r\n");
    // no target was supplied
    else if(!tgt) {
      if(*data->to_char_notgt)
	message(ch, NULL, NULL, NULL, TRUE, TO_CHAR, data->to_char_notgt);
      if(*data->to_room_notgt)
	message(ch, NULL, NULL, NULL, TRUE, TO_ROOM, data->to_room_notgt);
    }

    // a target was supplied, and it was us
    else if(ch == tgt) {
      if(*data->to_char_self)
	message(ch, NULL, NULL, NULL, TRUE, TO_CHAR, data->to_char_self);
      else if(*data->to_char_notgt)
	message(ch, NULL, NULL, NULL, TRUE, TO_CHAR, data->to_char_notgt);
      if(*data->to_room_self)
	message(ch, NULL, NULL, NULL, TRUE, TO_ROOM, data->to_room_self);
      else if(*data->to_room_notgt)
	message(ch, NULL, NULL, NULL, TRUE, TO_ROOM, data->to_room_notgt);
    }

    // a target was supplied and it was not us
    else {
      if(*data->to_char_tgt)
	message(ch, tgt, NULL, NULL, TRUE, TO_CHAR, data->to_char_tgt);
      if(*data->to_vict_tgt)
	message(ch, tgt, NULL, NULL, TRUE, TO_VICT, data->to_vict_tgt);
      if(*data->to_room_tgt)
	message(ch, tgt, NULL, NULL, TRUE, TO_ROOM, data->to_room_tgt);
    }
  }
  else
    log_string("ERROR: %s tried social, %s, but no such social exists!",
	charGetName(ch), cmd);
}



//*****************************************************************************
//
// implementation of socials.h
//
//*****************************************************************************

// I feel dirty doing this, but it's small and it saves us lots of work. So,
// well, we'll just have to live with it ;)
//
// init_socials calls add_social to add in all of the socials it loads up.
// But add_social calls save_socials each time that a new social is added.
// Thus, when we are booting up, we're making so so so many writes to disk
// that are un-needed. Here, we just have a little variable that tells whether
// we're booting up or if we're adding a new social in-game. If we're just
// booting up, then add_socials won't save any changes.
bool in_social_init = TRUE;

void init_socials() {
  // create the social table
  social_table = newHashtable();

  // open up the storage set
  STORAGE_SET       *set = storage_read(SOCIALS_FILE);
  STORAGE_SET_LIST *list = read_list(set, "socials");
  STORAGE_SET    *social = NULL;

  // parse all of the socials
  while( (social = storage_list_next(list)) != NULL)
    add_social(socialRead(social));
  
  // close the storage set
  storage_close(set);

  // add all of the socials to the command table
  HASH_ITERATOR *hash_i = newHashIterator(social_table);
  const char       *cmd = NULL;
  SOCIAL_DATA     *data = NULL;

  ITERATE_HASH(cmd, data, hash_i) {
    add_cmd(cmd, NULL, cmd_social, "player", FALSE);
    if(data->min_pos == POS_SITTING)
      add_cmd_check(cmd, chk_conscious);
    else if(data->min_pos == POS_STANDING)
      add_cmd_check(cmd, chk_can_move);
    else if(data->max_pos == POS_STANDING)
      add_cmd_check(cmd, chk_grounded);
  } deleteHashIterator(hash_i);

  // link/unlink commands for the admins
  add_cmd("soclink",   NULL, cmd_soclink,   "builder", FALSE);
  add_cmd("socunlink", NULL, cmd_socunlink, "builder", FALSE);
  add_cmd("socials",   NULL, cmd_socials,   "player",  FALSE);

  // let add_social know it can start saving again
  in_social_init = FALSE;

  init_socedit();
}


SOCIAL_DATA *get_social(const char *cmd) {
  return hashGet(social_table, cmd);
}


void add_social(SOCIAL_DATA *social) {
  // for each of our keywords, go through and 
  // unlink all of the current socials and link the new one
  LIST       *cmd_list = parse_keywords(social->cmds);
  LIST_ITERATOR *cmd_i = newListIterator(cmd_list);
  char            *cmd = NULL;

  ITERATE_LIST(cmd, cmd_i) {
    unlink_social(cmd);
    hashPut(social_table, cmd, social);
    // add the new command to the game
    add_cmd(cmd, NULL, cmd_social, "player", FALSE);
    if(social->min_pos == POS_SITTING)
      add_cmd_check(cmd, chk_conscious);
    else if(social->min_pos == POS_STANDING)
      add_cmd_check(cmd, chk_can_move);
    else if(social->max_pos == POS_STANDING)
      add_cmd_check(cmd, chk_grounded);
  } deleteListIterator(cmd_i);

  // garbage collection
  deleteListWith(cmd_list, free);

  // save changes
  if(!in_social_init)
    save_socials();
}


void link_social(const char *new_cmd, const char *old_cmd) {
  // check for old_cmd
  SOCIAL_DATA *data = hashGet(social_table, old_cmd);

  // link new_cmd to old_cmd
  if(data != NULL) {
    // first, remove the current new_cmd social, if it exists
    unlink_social(new_cmd);

    // add the new keyword to the social
    add_keyword(&(data->cmds), new_cmd);
    
    // add the new command to the social table
    hashPut(social_table, new_cmd, data);
    
    // add the new command to the game
    add_cmd(new_cmd, NULL, cmd_social, "player", FALSE);
    if(data->min_pos == POS_SITTING)
      add_cmd_check(new_cmd, chk_conscious);
    else if(data->min_pos == POS_STANDING)
      add_cmd_check(new_cmd, chk_can_move);
    else if(data->max_pos == POS_STANDING)
      add_cmd_check(new_cmd, chk_grounded);
  }

  // save changes
  if(!in_social_init)
    save_socials();
}


void unlink_social(const char *cmd) {
  // remove the command
  SOCIAL_DATA *data = hashRemove(social_table, cmd);

  // unlink the social
  if(data != NULL) {
    remove_keyword(data->cmds, cmd);

    // remove the command from the command table
    CMD_DATA *cdata = remove_cmd(cmd);
    if(cdata != NULL)
      deleteCmd(cdata);

    // if no links are left, delete the social
    if(!*data->cmds)
      deleteSocial(data);

    // save changes
    if(!in_social_init)
      save_socials();
  }
}
