//*****************************************************************************
//
// bedit.c
//
// bedit is the OLC for bitvector editing. Allows people to turn on/off bits for
// a bitvector.
//
//*****************************************************************************

#include "../mud.h"
#include "../socket.h"
#include "../bitvector.h"

#include "olc.h"



//*****************************************************************************
// bitvector editing functions
//*****************************************************************************
void bedit_menu   (SOCKET_DATA *sock, BITVECTOR *vector) {
  LIST *bits = bitvectorListBits(vector);
  send_to_socket(sock, "{wCurrent bits: {c%s\r\n", bitvectorGetBits(vector));
  olc_display_list(sock, bits, 3);
  deleteListWith(bits, free);
}

int  bedit_chooser(SOCKET_DATA *sock, BITVECTOR *vector, const char *option) {
  if(!isdigit(*option)) 
    return MENU_CHOICE_INVALID;
  else {
    int choice = atoi(option);
    if(choice < 0 || choice >= bitvectorSize(vector))
      return MENU_CHOICE_INVALID;
    else {
      LIST *bits = bitvectorListBits(vector);
      char  *bit = listGet(bits, choice);
      bitToggle(vector, bit);
      deleteListWith(bits, free);
      return MENU_NOCHOICE;
    }
  }
}

bool bedit_parser (SOCKET_DATA *sock, BITVECTOR *vector, int choice, 
		   const char *arg) {
  // no parser... everything is done in the chooser
  return FALSE;
}
