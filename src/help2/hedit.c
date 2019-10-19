//*****************************************************************************
//
// hedit.c
//
// Contains all of the functions necessary to edit helpfiles online. Built 
// into the olc2 module.
//
//*****************************************************************************
#include "../mud.h"
#include "../utils.h"
#include "../storage.h"
#include "../character.h"
#include "../socket.h"
#include "help.h"
//#include "hedit.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../olc2/olc.h"
#include "../editor/editor.h"



//*****************************************************************************
// local functions and data structures
//*****************************************************************************
typedef struct help_olc_data {
  char *old_keywords;
  char     *keywords;
  char      *related;
  char  *user_groups;
  BUFFER       *info;
} HELP_OLC;

HELP_OLC *newHelpOLC(const char *keywords, const char *info, 
		     const char *user_groups, const char *related) {
  HELP_OLC     *data = malloc(sizeof(HELP_OLC));
  data->old_keywords = strdupsafe(keywords);
  data->keywords     = strdupsafe(keywords);
  data->user_groups  = strdupsafe(user_groups);
  data->related      = strdupsafe(related);
  data->info         = newBuffer(1);
  bufferCat(data->info, info);
  return data;
}

void deleteHelpOLC(HELP_OLC *data) {
  if(data->old_keywords) free(data->old_keywords);
  if(data->keywords)     free(data->keywords);
  if(data->user_groups)  free(data->user_groups);
  if(data->related)      free(data->related);
  if(data->info)         deleteBuffer(data->info);
  free(data);
}



//*****************************************************************************
// help file editing
//*****************************************************************************
#define HEDIT_KEYWORDS      1
#define HEDIT_USER_GROUPS   2
#define HEDIT_RELATED       3

void hedit_menu(SOCKET_DATA *sock, HELP_OLC *data) {
  send_to_socket(sock,
		 "{g1) keywords         : {c%s\r\n"
		 "{g2) related          : {c%s\r\n"
		 "{g3) group restriction: {c%s\r\n"
		 "{g4) Info:\r\n"
		 "{c%s",
		 data->keywords,
		 data->related,
		 data->user_groups,
		 bufferString(data->info));
}

int hedit_chooser(SOCKET_DATA *sock, HELP_OLC *data, const char *option) {
  switch(toupper(*option)) {
  case '1':
    send_to_socket(sock, "Enter the keywords that reference this helpfile: ");
    return HEDIT_KEYWORDS;
  case '2':
    send_to_socket(sock, "Enter the names of related helpfiles: ");
    return HEDIT_RELATED;
  case '3':
    send_to_socket(sock, "Which user groups are this helpfile limited to: ");
    return HEDIT_USER_GROUPS;
  case '4':
    text_to_buffer(sock, "Enter Helpfile Information\r\n");
    socketStartEditor(sock, text_editor, data->info);
    return MENU_NOCHOICE;
  default: 
    return MENU_CHOICE_INVALID;
  }
}

bool hedit_parser(SOCKET_DATA *sock, HELP_OLC *data, int choice, 
		  const char *arg) {
  switch(choice) {
  case HEDIT_KEYWORDS:
    free(data->keywords);
    data->keywords = strdupsafe(arg);
    return TRUE;
  case HEDIT_RELATED:
    free(data->related);
    data->related = strdupsafe(arg);
    return TRUE;
  case HEDIT_USER_GROUPS:
    free(data->user_groups);
    data->user_groups = strdupsafe(arg);
    return TRUE;
  default: 
    return FALSE;
  }
}

void save_help_olc(HELP_OLC *data) {
  // first, remove our old helpfile from disc
  LIST    *kwds = parse_keywords(data->old_keywords);
  char *primary = listGet(kwds, 0);
  if(primary && *primary)
    remove_help(primary);
  deleteListWith(kwds, free);

  // now, add our new one
  add_help(data->keywords, bufferString(data->info), data->user_groups,
	   data->related, TRUE);
}



//*****************************************************************************
// commnds
//*****************************************************************************
COMMAND(cmd_hedit) {
  if(!arg || !*arg)
    send_to_char(ch, "Which help file would you like to edit?\r\n");
  else {
    // first, try to find the helpfile
    HELP_DATA    *data = get_help(arg, FALSE);
    HELP_OLC *olc_data = NULL;

    // No such file exists; create a new one
    if(data == NULL)
      olc_data = newHelpOLC(arg, "This is not very helpful.\r\n", "", "");
    else
      olc_data = newHelpOLC(helpGetKeywords(data),
			    helpGetInfo(data),
			    helpGetUserGroups(data),
			    helpGetRelated(data));

    // do our olc
    do_olc(charGetSocket(ch), hedit_menu, hedit_chooser, hedit_parser,
	   NULL, NULL, deleteHelpOLC, save_help_olc, olc_data);
  }
}

COMMAND(cmd_hdelete) {
  if(!arg || !*arg)
    send_to_char(ch, "Which help file would you like to delete?\r\n");
  else {
    // first, try to find the helpfile
    HELP_DATA *data = get_help(arg, FALSE);
    
    // make sure it exists
    if(data == NULL)
      send_to_char(ch, "No helpfile by that name exists.\r\n");
    else {
      send_to_char(ch, "Helpfile for %s removed.\r\n", arg);
      remove_help(arg);
    }
  }
}



//*****************************************************************************
// initialization
//*****************************************************************************
void init_hedit() {
  // add our commands
  add_cmd("hedit",   NULL, cmd_hedit,   "builder", FALSE);
  add_cmd("hdelete", NULL, cmd_hdelete, "builder", FALSE); 
}
