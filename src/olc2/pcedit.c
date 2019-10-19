//*****************************************************************************
//
// pcedit.c
//
// These are the functions for editing player files. Can be used to change
// descriptions and other facets of a character.
//
//*****************************************************************************
#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../character.h"
#include "../races.h"
#include "../handler.h"
#include "../save.h"

#include "olc.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../editor/editor.h"



//*****************************************************************************
// mobile editing
//*****************************************************************************
#define PCEDIT_NAME          1
#define PCEDIT_MULTI_NAME    2
#define PCEDIT_KEYWORDS      3
#define PCEDIT_RDESC         4
#define PCEDIT_MULTI_RDESC   5
#define PCEDIT_RACE          6
#define PCEDIT_SEX           7
#define PCEDIT_USER_GROUPS   8



void pcedit_menu(SOCKET_DATA *sock, CHAR_DATA *mob) {
  send_to_socket(sock,
		 "{g1) Name\r\n"
		 "{c%s\r\n"
		 "{g2) Name for multiple occurences\r\n"
		 "{c%s\r\n"
		 "{g3) Keywords\r\n"
		 "{c%s\r\n"
		 "{g4) Room description\r\n"
		 "{c%s\r\n"
		 "{g5) Room description for multiple occurences\r\n"
		 "{c%s\r\n"
		 "{g6) Description\r\n"
		 "{c%s\r\n"
		 "{gU) User Groups   {c%s{n\r\n"
		 "{gR) Change Race   {y[{c%8s{y]\r\n"
		 "{gG) Change Gender {y[{c%8s{y]\r\n",
		 charGetName(mob),
		 charGetMultiName(mob),
		 charGetKeywords(mob),
		 charGetRdesc(mob),
		 charGetMultiRdesc(mob),
		 charGetDesc(mob),
		 bitvectorGetBits(charGetUserGroups(mob)),
		 charGetRace(mob),
		 sexGetName(charGetSex(mob))
		 );
}

int  pcedit_chooser(SOCKET_DATA *sock, CHAR_DATA *mob, const char *option) {
  switch(toupper(*option)) {
  case '1':
    text_to_buffer(sock, "Enter name: ");
    return PCEDIT_NAME;
  case '2':
    text_to_buffer(sock, "Enter name for multiple occurences: ");
    return PCEDIT_MULTI_NAME;
  case '3':
    text_to_buffer(sock, "Enter keywords: ");
    return PCEDIT_KEYWORDS;
  case '4':
    text_to_buffer(sock, "Enter room description: ");
    return PCEDIT_RDESC;
  case '5':
    text_to_buffer(sock, "Enter room description for multiple occurences: ");
    return PCEDIT_MULTI_RDESC;
  case '6':
    text_to_buffer(sock, "Enter description\r\n");
    socketStartEditor(sock, text_editor, charGetDescBuffer(mob));
    return MENU_NOCHOICE;
  case 'R':
    send_to_socket(sock, "%s\r\n\r\n", raceGetList(FALSE));
    text_to_buffer(sock, "Please select a race: ");
    return PCEDIT_RACE;
  case 'G':
    olc_display_table(sock, sexGetName, NUM_SEXES, 1);
    text_to_buffer(sock, "Pick a gender: ");
    return PCEDIT_SEX;
  case 'U':
    text_to_buffer(sock, "Enter comma-separated list of user groups: ");
    return PCEDIT_USER_GROUPS;
  default: return MENU_CHOICE_INVALID;
  }
}

bool pcedit_parser(SOCKET_DATA *sock, CHAR_DATA *mob, int choice, 
		  const char *arg) {
  switch(choice) {
  case PCEDIT_NAME:
    charSetName(mob, arg);
    return TRUE;
  case PCEDIT_MULTI_NAME:
    charSetMultiName(mob, arg);
    return TRUE;
  case PCEDIT_KEYWORDS:
    charSetKeywords(mob, arg);
    return TRUE;
  case PCEDIT_RDESC:
    charSetRdesc(mob, arg);
    return TRUE;
  case PCEDIT_MULTI_RDESC:
    charSetMultiRdesc(mob, arg);
    return TRUE;
  case PCEDIT_USER_GROUPS:
    bitClear(charGetUserGroups(mob));
    bitToggle(charGetUserGroups(mob), arg);
    return TRUE;
  case PCEDIT_RACE:
    if(!isRace(arg))
      return FALSE;
    charSetRace(mob, arg);
    return TRUE;
  case PCEDIT_SEX: {
    int val = atoi(arg);
    if(!isdigit(*arg) || val < 0 || val >= NUM_SEXES)
      return FALSE;
    charSetSex(mob, val);
    return TRUE;
  }
  default: return FALSE;
  }
}

COMMAND(cmd_pcedit) {
  if(!arg || !*arg)
    send_to_char(ch, "You must supply the player's name, first!\r\n");
  else {
    CHAR_DATA *player = get_player(arg);
    if(player == NULL)
      send_to_char(ch, "Player '%s' does not exist!\r\n", arg);
    else {
      do_olc(charGetSocket(ch), pcedit_menu, pcedit_chooser, pcedit_parser,
	     NULL, NULL, unreference_player, save_player, player);
    }
  }
}
