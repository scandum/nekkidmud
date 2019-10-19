/*
 * This file handles command interpreting
 */
#include <sys/types.h>
#include <stdio.h>

/* include main header file */
#include "mud.h"
#include "utils.h"
#include "character.h"
#include "socket.h"
#include "room.h"
#include "commands.h"
#include "action.h"
#include "hooks.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/scripts.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_FACULTY
#include "faculty/faculty.h"
#endif
#ifdef MODULE_ALIAS
#include "alias/alias.h"
#endif



//*****************************************************************************
// local variables and functions
//*****************************************************************************
NEAR_MAP *cmd_table = NULL;

void init_commands() {
  cmd_table = newNearMap();

  //***************************************************************************
  // This is for core functions ONLY! If you have a module that adds new
  // functions to the MUD, they should be added in the init_xxx() function
  // associated with your module.
  //***************************************************************************
  add_cmd("back",       NULL, cmd_back,        "player", FALSE);
  add_cmd("commands",   NULL, cmd_commands,    "player", FALSE);
  //add_cmd("compress",   NULL, cmd_compress,    "player", FALSE);
  //add_cmd("groupcmds",  NULL, cmd_groupcmds,   "player", FALSE);
  add_cmd("look",       "l",  cmd_look,        "player", FALSE);
  add_cmd("more",       NULL, cmd_more,        "player", FALSE);
  add_cmd_check("look",        chk_conscious);
}

bool cmd_exists(const char *cmd) {
  return nearMapKeyExists(cmd_table, cmd);
}

CMD_DATA *remove_cmd(const char *cmd) {
  return nearMapRemove(cmd_table, cmd);
}

void add_cmd(const char *cmd, const char *sort_by, COMMAND(func),
	     const char *user_group, bool interrupts) {
  // do we already have an existing command we're just updating?
  CMD_DATA *data = remove_cmd(cmd);

  // is it something that needs to be updated or erased?
  if(data != NULL && !cmdHasFunc(data))
    cmdUpdate(data, func, user_group, interrupts);
  else {
    if(data != NULL)
      deleteCmd(data);
    data = newCmd(cmd, func, user_group, interrupts);
  }

  // add in the new command
  nearMapPut(cmd_table, cmd, sort_by, data);
}

void add_py_cmd(const char *cmd, const char *sort_by, void *pyfunc,
	     const char *user_group, bool interrupts) {
  // do we already have an existing command we're just updating?
  CMD_DATA *data = remove_cmd(cmd);

  // is this something that needs to be updated or erased?
  if(data != NULL && !cmdHasFunc(data))
    cmdPyUpdate(data, pyfunc, user_group, interrupts);
  else {
    if(data != NULL)
      deleteCmd(data);
    data = newPyCmd(cmd, pyfunc, user_group, interrupts);
  }

  // add in the new command
  nearMapPut(cmd_table, cmd, sort_by, data);
}

void add_cmd_check(const char *cmd, CMD_CHK(func)) {
  CMD_DATA *data = nearMapGet(cmd_table, cmd, FALSE);

  // make a temp container for just the checks
  if(data == NULL) {
    data = newCmd(cmd, NULL, "", FALSE);
    nearMapPut(cmd_table, cmd, NULL, data);
  }
    
  cmdAddCheck(data, func);
}

void add_py_cmd_check(const char *cmd, void *pyfunc) {
  CMD_DATA *data = nearMapGet(cmd_table, cmd, FALSE);

  // make a temp container just for the checks
  if(data == NULL) {
    data = newCmd(cmd, NULL, "", FALSE);
    nearMapPut(cmd_table, cmd, NULL, data);
  }

  cmdAddPyCheck(data, pyfunc);
}

// show the character all of the commands in the specified group(s).
void show_commands(CHAR_DATA *ch, const char *user_groups) {
  BUFFER           *buf = newBuffer(MAX_BUFFER);
  NEAR_ITERATOR *near_i = newNearIterator(cmd_table);
  const char    *abbrev = NULL;
  CMD_DATA         *cmd = NULL;
  int               col = 0;

  // go over all of our buckets
  ITERATE_NEARMAP(abbrev, cmd, near_i) {
    if(cmdHasFunc(cmd) && is_keyword(user_groups, cmdGetUserGroup(cmd), FALSE)){
      bprintf(buf, "%-13.13s", cmdGetName(cmd));
      if (!(++col % 6))
	bufferCat(buf, "\r\n");
    }
  } deleteNearIterator(near_i);

  // do room commands as well
  if(roomHasCmds(charGetRoom(ch))) {
    near_i = newNearIterator(roomGetCmdTable(charGetRoom(ch)));
    abbrev = NULL;
    cmd    = NULL;
    bufferCat(buf, "{c");
    ITERATE_NEARMAP(abbrev, cmd, near_i) {
      if(cmdHasFunc(cmd) && is_keyword(user_groups,cmdGetUserGroup(cmd),FALSE)){
	bprintf(buf, "%-13.13s", cmdGetName(cmd));
	if (!(++col % 6))
	  bufferCat(buf, "\r\n");
      }
    } deleteNearIterator(near_i);
    bufferCat(buf, "{n");
  }

  // tag on our last newline if neccessary, and show commands
  if (col % 6) bprintf(buf, "\r\n");
  text_to_char(ch, bufferString(buf));
  deleteBuffer(buf);
}

//
// return whether the command is usable by the character
bool is_usable_cmd(CHAR_DATA *ch, CMD_DATA *cmd) {
  // this is a check, not a command
  if(*cmdGetUserGroup(cmd) == '\0')
    return FALSE;
  return is_keyword(bitvectorGetBits(charGetUserGroups(ch)),
		    cmdGetUserGroup(cmd), FALSE);
}

//
// checks are commands without a specific user group, added to finer-grain 
// command tables (e.g., characters, to rooms, to zones, to the world). This
// is for e.g., allowing for the addition of checks before leaving a room
// (maybe you have to move a boulder out of the way first, etc...)
CMD_DATA *find_check(CHAR_DATA *ch, NEAR_MAP *table, const char *name) {
  CMD_DATA *check = nearMapGet(table, name, FALSE);
  /*
  if(check == NULL) || *cmdGetUserGroup(check) != '\0')
    return NULL;
  */
  return check;
}

//
// tries to find the relevant command, usable by the character
CMD_DATA *find_cmd(CHAR_DATA *ch, NEAR_MAP *table, const char *name, 
		   bool abbrev_ok) {
  if(abbrev_ok == FALSE) {
    CMD_DATA *cmd = nearMapGet(table, name, FALSE);
    // no command exists
    if(cmd == NULL || !is_usable_cmd(ch, cmd))
      return NULL;
    return cmd;
  }
  else {
    // try to look up the possible commands
    LIST *cmdname_list = nearMapGetAllMatches(table, name);
    CMD_DATA  *cmd = NULL;
    if(cmdname_list != NULL) {
      LIST_ITERATOR *cmdname_i = newListIterator(cmdname_list);
      const char      *cmdname = NULL;
      ITERATE_LIST(cmdname, cmdname_i) {
	cmd = nearMapGet(table, cmdname, FALSE);
	if(is_usable_cmd(ch, cmd))
	  break;
	else
	  cmd = NULL;
      } deleteListIterator(cmdname_i);
      deleteListWith(cmdname_list, free);
    }
    return cmd;
  }
}

// tries to pull a usable command from the near-table and use it. Returns
// TRUE if a usable command was found (even if it failed) and false otherwise.
bool try_use_cmd_table(CHAR_DATA *ch, NEAR_MAP *table, const char *command, 
		       char *arg, bool abbrev_ok) {
  if(abbrev_ok == FALSE) {
    CMD_DATA *cmd = nearMapGet(table, command, FALSE);
    if(cmd == NULL)
      return FALSE;
    else if(!*cmdGetUserGroup(cmd) || 
	    is_keyword(bitvectorGetBits(charGetUserGroups(ch)),
		       cmdGetUserGroup(cmd), FALSE)) {
      if(charTryCmd(ch, cmd, arg) == -1)
	return FALSE;
      return TRUE;
    }
    else
      return FALSE;
  }
  else {
    // try to look up the possible commands
    LIST *cmdname_list = nearMapGetAllMatches(table, command);
    bool           ret = FALSE;
    if(cmdname_list != NULL) {
      LIST_ITERATOR *cmdname_i = newListIterator(cmdname_list);
      CMD_DATA            *cmd = NULL;
      const char      *cmdname = NULL;
      ITERATE_LIST(cmdname, cmdname_i) {
	cmd = nearMapGet(table, cmdname, FALSE);
	if(!*cmdGetUserGroup(cmd) ||
	   is_keyword(bitvectorGetBits(charGetUserGroups(ch)), 
		      cmdGetUserGroup(cmd), FALSE)) {
	  if(charTryCmd(ch, cmd, arg) != -1)
	    ret = TRUE;
	  break;
	}
      } deleteListIterator(cmdname_i);
      deleteListWith(cmdname_list, free);
    }
    return ret;
  }
}

void handle_cmd_input(SOCKET_DATA *dsock, char *arg) {
  CHAR_DATA *ch;
  if ((ch = socketGetChar(dsock)) == NULL)
    return;
  do_cmd(ch, arg, TRUE);
}

void do_cmd(CHAR_DATA *ch, char *arg, bool aliases_ok)  {
  char command[MAX_BUFFER];

  // make sure we've got a command to try
  if(!arg || !*arg)
    return;

  // if we are leading with a non-character, we are trying to do a short-form
  // command (e.g. ' for say, " for gossip). Just take the first character
  // and use the rest as the arg
  if(isalpha(*arg) || isdigit(*arg))
    arg = one_arg(arg, command);
  else {
    *command     = *arg;
    *(command+1) = '\0';
    arg++;
    // and skip all spaces
    while(isspace(*arg))
      arg++;
  }

#ifdef MODULE_ALIAS
  if(aliases_ok && try_alias(ch, command, arg))
    return;
#endif

  // check to see if it's a faculty command
#ifdef MODULE_FACULTY
  if(try_use_faculty(ch, command))
    return;
#endif

  // figure out what tables we need to look over
  LIST *cmd_tables = newList();
  // item-specific commands here? <---
  // character-specific commands here? <---
  if(charGetRoom(ch) && roomHasCmds(charGetRoom(ch)))
    listQueue(cmd_tables, roomGetCmdTable(charGetRoom(ch)));
  // zone-specific commands here? <---
  listQueue(cmd_tables, cmd_table);

  // go through each table in the list, in order. Check if the command exists
  // in any of the tables. Only allow abbreviations in the last table (the
  // game commands). Before a command is executed, run checks that might be
  // included on any of the previous tables
  int  i, j, ret;
  bool found = FALSE;
  for(i = 0; i < listSize(cmd_tables); i++) {
    NEAR_MAP *table = listGet(cmd_tables, i);
    CMD_DATA   *cmd = find_cmd(ch, table, command, 
			       (i == listSize(cmd_tables)-1 ? TRUE : FALSE));
    if(cmd != NULL) {
      // first, run checks on all our previous tables
      for(j = 0; j < i; j++) {
	CMD_DATA *check = find_check(ch,listGet(cmd_tables,j),cmdGetName(cmd));
	if(check != NULL) {
	  // run the check
	  if((ret = charTryCmd(ch, check, arg)) != -1) {
	    if(ret == TRUE)
	      hookRun("command", hookBuildInfo("ch str str",
					       ch,cmdGetName(cmd),arg));
 	    found = TRUE;
	    break;
	  }
	}
      }
    }

    if(found == TRUE)
      break;
    else if(cmd != NULL) {
      // execute the command
      if((ret = charTryCmd(ch, cmd, arg)) != -1) {
	if(ret == TRUE)
	  hookRun("command",hookBuildInfo("ch str str",ch,cmdGetName(cmd),arg));
	found = TRUE;
	break;
      }
    }
  }

  // nothing usable was found
  if(found == FALSE)
    text_to_char(ch, "No such command.\r\n");

  // garbage collection
  deleteList(cmd_tables);

  /*
  // try the command
  if(!charGetRoom(ch) || 
     !try_use_cmd_table(ch,roomGetCmdTable(charGetRoom(ch)),command,arg,FALSE))
    if(!try_use_cmd_table(ch, cmd_table, command, arg, TRUE))
      text_to_char(ch, "No such command.\r\n");
  */
}
