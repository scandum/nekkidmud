//*****************************************************************************
//
// socedit.c
//
// socedit is a set of tools for editing socials online. Socedit requires that
// olc2 be installed.
//
//*****************************************************************************

#include "../mud.h"
#include "../character.h"
#include "../socket.h"
#include "../olc2/olc.h"

#include "socials.h"



//*****************************************************************************
// the functions needed by olc2
//*****************************************************************************
#define SOCEDIT_CHAR_NOTGT      1
#define SOCEDIT_ROOM_NOTGT      2
#define SOCEDIT_CHAR_SELF       3
#define SOCEDIT_ROOM_SELF       4
#define SOCEDIT_CHAR_TGT        5
#define SOCEDIT_VICT_TGT        6
#define SOCEDIT_ROOM_TGT        7
#define SOCEDIT_MIN_POS         8
#define SOCEDIT_MAX_POS         9

void socedit_menu(SOCKET_DATA *sock, SOCIAL_DATA *social) {
  send_to_socket(sock,
		 "{y[{c%s{y]\r\n"
		 "{g1) to char notgt: {c%s\r\n"
		 "{g2) to room notgt: {c%s\r\n"
		 "{g3) to char self : {c%s\r\n"
		 "{g4) to room self : {c%s\r\n"
		 "{g5) to char tgt  : {c%s\r\n"
		 "{g6) to vict tgt  : {c%s\r\n"
		 "{g7) to room tgt  : {c%s\r\n"
		 "{g8) minimum pos  : {c%s\r\n"
		 "{g9) maximum pos  : {c%s\r\n"
		 "\r\n"
		 "{gTo assocciate/unassociate commands, use soclink and socunlink\r\n",
		 socialGetCmds(social),
		 socialGetCharNotgt(social),
		 socialGetRoomNotgt(social),
		 socialGetCharSelf(social),
		 socialGetRoomSelf(social),
		 socialGetCharTgt(social),
		 socialGetVictTgt(social),
		 socialGetRoomTgt(social),
		 posGetName(socialGetMinPos(social)),
		 posGetName(socialGetMaxPos(social))
		 );
}

int  socedit_chooser(SOCKET_DATA *sock, SOCIAL_DATA *social,const char *option){
  switch(toupper(*option)) {
  case '1':
    send_to_socket(sock, 
		   "The message to character when no target is supplied : ");
    return SOCEDIT_CHAR_NOTGT;
  case '2':
    send_to_socket(sock, 
		   "The message to room when no target is supplied : ");
    return SOCEDIT_ROOM_NOTGT;
  case '3':
    send_to_socket(sock, 
		   "The message to character when target is self : ");
    return SOCEDIT_CHAR_SELF;
  case '4':
    send_to_socket(sock, 
		   "The message to room when target is self : ");
    return SOCEDIT_ROOM_SELF;
  case '5':
    send_to_socket(sock, 
		   "The message to character when a target is found : ");
    return SOCEDIT_CHAR_TGT;
  case '6':
    send_to_socket(sock, 
		   "The message to target when a target is found : ");
    return SOCEDIT_VICT_TGT;
  case '7':
    send_to_socket(sock, 
		   "The message to room when a target is found : ");
    return SOCEDIT_ROOM_TGT;
  case '8':
    olc_display_table(sock, posGetName, NUM_POSITIONS, 2);
    text_to_buffer(sock, "Pick a minimum position: ");
    return SOCEDIT_MIN_POS;
  case '9':
    olc_display_table(sock, posGetName, NUM_POSITIONS, 2);
    text_to_buffer(sock, "Pick a maximum position: ");
    return SOCEDIT_MAX_POS;
  default: 
    return MENU_CHOICE_INVALID;
  }
}

bool socedit_parser(SOCKET_DATA *sock, SOCIAL_DATA *social, int choice, 
		    const char *arg){
  switch(choice) {
  case SOCEDIT_CHAR_NOTGT:
    socialSetCharNotgt(social, arg);
    return TRUE;
  case SOCEDIT_ROOM_NOTGT:
    socialSetRoomNotgt(social, arg);
    return TRUE;
  case SOCEDIT_CHAR_SELF:
    socialSetCharSelf(social, arg);
    return TRUE;
  case SOCEDIT_ROOM_SELF:
    socialSetRoomSelf(social, arg);
    return TRUE;
  case SOCEDIT_CHAR_TGT:
    socialSetCharTgt(social, arg);
    return TRUE;
  case SOCEDIT_VICT_TGT:
    socialSetVictTgt(social, arg);
    return TRUE;
  case SOCEDIT_ROOM_TGT:
    socialSetRoomTgt(social, arg);
    return TRUE;
  case SOCEDIT_MIN_POS: {
    int val = atoi(arg);
    if(!isdigit(*arg) || val < 0 || val >= NUM_POSITIONS)
      return FALSE;
    socialSetMinPos(social, val);
    return TRUE;
  }
  case SOCEDIT_MAX_POS: {
    int val = atoi(arg);
    if(!isdigit(*arg) || val < 0 || val >= NUM_POSITIONS)
      return FALSE;
    socialSetMaxPos(social, val);
    return TRUE;
  }
  default: 
    return FALSE;
  }
}



//*****************************************************************************
// commands for entering socedit
//*****************************************************************************
void save_social(SOCIAL_DATA *social) {
  save_socials();
}

COMMAND(cmd_socedit) {
  SOCIAL_DATA *social;

  if(!arg || !*arg)
    send_to_char(ch, "Which social are you trying to edit?\r\n");
  else {
    // strip down to one argument
    one_arg(arg, arg);

    // find the social
    social = get_social(arg);

    // make sure we're not trying to edit a command
    if(social == NULL && cmd_exists(arg))
      send_to_char(ch, "But that is already a command!\r\n");
    else {
      // make a new one
      if(social == NULL) {
	social = newSocial(arg, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			   POS_STANDING, POS_STANDING);
	add_social(social);
      }

      // set up the OLC
      do_olc(charGetSocket(ch), socedit_menu, socedit_chooser, socedit_parser,
	     socialCopy, socialCopyTo, deleteSocial, save_social, social);
    }
  }
}



//*****************************************************************************
// implementation of socedit.h
//*****************************************************************************
void init_socedit(void) {
  add_cmd("socedit", NULL, cmd_socedit, "builder", TRUE);
}
