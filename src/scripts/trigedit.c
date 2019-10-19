//*****************************************************************************
//
// trigedit.c
//
// contains the functions neccessary for using OLC on triggers.
//
//*****************************************************************************
#include "../mud.h"
#include "../socket.h"
#include "../character.h"
#include "../utils.h"
#include "../world.h"
#include "scripts.h"
#include "trighooks.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../editor/editor.h"
#include "script_editor.h"

#ifdef MODULE_OLC2
#include "../olc2/olc.h"



//*****************************************************************************
// trigger lists
//*****************************************************************************
#define TRIGLIST_NEW     1
#define TRIGLIST_DELETE  2

void trigger_list_menu(SOCKET_DATA *sock, LIST *triggers) {
  if(listSize(triggers) > 0) {
    send_to_socket(sock, "{wCurrent triggers:\r\n");
    LIST_ITERATOR *trig_i = newListIterator(triggers);
    char            *trig = NULL;
    ITERATE_LIST(trig, trig_i) {
      send_to_socket(sock, "  {g%s\r\n", trig);
    } deleteListIterator(trig_i);
  }

  send_to_socket(sock, 
		 "\r\n"
		 "  {gN) Add new trigger\r\n"
		 "  {gD) Delete trigger\r\n");
}

int trigger_list_chooser(SOCKET_DATA *sock, LIST *triggers, const char *option){
  switch(toupper(*option)) {
  case 'N':
    send_to_socket(sock, "Enter key for trigger to attach: ");
    return TRIGLIST_NEW;
  case 'D':
    send_to_socket(sock, "Enter key for trigger to delete: ");
    return TRIGLIST_DELETE;
  default: 
    return MENU_CHOICE_INVALID;
  }
}

bool trigger_list_parser(SOCKET_DATA *sock, LIST *triggers, int choice,
		    const char *arg) {
  switch(choice) {
  case TRIGLIST_NEW:
    if(!listGetWith(triggers, arg, strcasecmp))
      listPutWith(triggers, strdup(arg), strcasecmp);
    return TRUE;
  case TRIGLIST_DELETE: {
    char *found = listRemoveWith(triggers, arg, strcasecmp);
    if(found) free(found);
    return TRUE;
  }
  default: return FALSE;
  }
}



//*****************************************************************************
// triggers
//*****************************************************************************
#define TEDIT_NAME       1
#define TEDIT_TYPE       2



//*****************************************************************************
// tedit OLC functions
//*****************************************************************************

void tedit_menu(SOCKET_DATA *sock, TRIGGER_DATA *trigger) {
  // the display line for our trigger type
  BUFFER *ttype_line = newBuffer(1);
  if(!*triggerGetType(trigger))
    bufferCat(ttype_line, "<NONE>");
  else {
    bufferCat(ttype_line, triggerGetType(trigger));

    // is it a valid type?
    if(hashIn(get_tedit_opts(), triggerGetType(trigger)))
      bprintf(ttype_line, "   (%s)", (char *)hashGet(get_tedit_opts(), 
						   triggerGetType(trigger)));
    else
      bprintf(ttype_line, "   {r* unknown trigger type");
  }

  send_to_socket(sock,
		 "{g[{c%s{g]\r\n"
		 "{g1) Name        : {c%s\r\n"
		 "{g2) Trigger type: {c%s\r\n"
		 "{g3) Script Code\r\n",
		 triggerGetKey(trigger),
		 triggerGetName(trigger),
		 bufferString(ttype_line));
  script_display(sock, triggerGetCode(trigger), FALSE);

  // garbage collection
  deleteBuffer(ttype_line);
}

int tedit_chooser(SOCKET_DATA *sock, TRIGGER_DATA *trigger, const char *option){
  switch(toupper(*option)) {
  case '1':
    send_to_socket(sock, "Enter trigger name: ");
    return TEDIT_NAME;
  case '2':
    send_to_socket(sock, 
		   "{w%-18s %-20s%-18s %-22s\r\n", 
		   "Type", "Usable By", "Type", "Usable By");
    send_to_socket(sock, "{y------------------------------------------------------------------------------{g\r\n");

    // get our keys, sort them alphabetically, and then list them all
    HASHTABLE        *opts = get_tedit_opts();
    LIST             *keys = hashCollect(opts);
    listSortWith(keys, strcasecmp);
    LIST_ITERATOR   *key_i = newListIterator(keys);
    const char        *key = NULL;
    int             parity = 0;

    // display our keys, one at a time
    ITERATE_LIST(key, key_i) {
      send_to_socket(sock, "%-18s %-20s", key, (char *)hashGet(opts, key));
      parity = (parity + 1) % 2;
      if(parity == 0)
	send_to_socket(sock, "\r\n");
    } deleteListIterator(key_i);

    // need a newline
    if(parity == 1)
      send_to_socket(sock, "\r\n");

    send_to_socket(sock, "\r\nEnter trigger type: ");

    // garbage collection
    deleteListWith(keys, free);

    return TEDIT_TYPE;
  case '3':
    socketStartEditor(sock, script_editor, triggerGetCodeBuffer(trigger));
    return MENU_NOCHOICE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool tedit_parser(SOCKET_DATA *sock, TRIGGER_DATA *trigger, int choice,
		  const char *arg) {
  switch(choice) {
  case TEDIT_NAME:
    triggerSetName(trigger, arg);
    return TRUE;

  case TEDIT_TYPE: {
    triggerSetType(trigger, arg);
    /*
    int num = atoi(arg);
    if(num < 0 || num >= num_trig_types())
      return FALSE;
    else {
      triggerSetType(trigger, triggerTypeGetName(num));
      return TRUE;
    }
    */
    return TRUE;
  }

  default: 
    return FALSE;
  }
}

void save_trigger(TRIGGER_DATA *trigger) {
  format_script_buffer(triggerGetCodeBuffer(trigger));
  worldSaveType(gameworld, "trigger", triggerGetKey(trigger));
}

COMMAND(cmd_tedit) {
  ZONE_DATA       *zone = NULL;
  TRIGGER_DATA *trigger = NULL;

  // we need a key
  if(!arg || !*arg)
    send_to_char(ch,"Please supply the key of a trigger you wish to edit.\r\n");
  else if(key_malformed(arg))
    send_to_char(ch, "You entered a malformed trigger key.\r\n");
  else {
    char name[SMALL_BUFFER], locale[SMALL_BUFFER];
    if(!parse_worldkey_relative(ch, arg, name, locale))
      send_to_char(ch, "Which trigger are you trying to edit?\r\n");
    else if( (zone = worldGetZone(gameworld, locale)) == NULL)
      send_to_char(ch, "No such zone exists.\r\n");
    else if(!canEditZone(zone, ch))
      send_to_char(ch, "You are not authorized to edit that zone.\r\n");
    else {
      // pull up the script
      trigger = worldGetType(gameworld, "trigger", get_fullkey(name, locale));
      if(trigger == NULL) {
	trigger = newTrigger();
	triggerSetName(trigger, "An Unfinished Trigger");
	triggerSetCode(trigger, "# trigger code goes here\n"
		                "# make sure to comment it with pounds (#)\n");
	worldPutType(gameworld, "trigger", get_fullkey(name, locale), trigger);
      }

      do_olc(charGetSocket(ch), tedit_menu, tedit_chooser, tedit_parser,
	     triggerCopy, triggerCopyTo, deleteTrigger, save_trigger, trigger);
    }
  }
}

#endif // MODULE_OLC2
