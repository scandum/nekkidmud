//*****************************************************************************
//
// esedit.c
//
// Contains all of the functions for doing the olc editing of extra description
// sets. Used by rooms, objects (and mobiles?)
//
//*****************************************************************************

#include "../mud.h"
#include "../socket.h"
#include "../extra_descs.h"
#include "../editor/editor.h"

#include "olc.h"



//*****************************************************************************
// functions for editing a single extra description
//*****************************************************************************
#define EDEDIT_KEYWORD     1

void edesc_menu(SOCKET_DATA *sock, EDESC_DATA *edesc) {
  send_to_socket(sock,
		 "{g1) Keywords\r\n"
		 "{c%s\r\n"
		 "{g2) Description\r\n"
		 "{c%s\r\n",
		 edescGetKeywords(edesc), 
		 edescSetGetDesc(edesc)
		 );
}

int  edesc_chooser(SOCKET_DATA *sock, EDESC_DATA *edesc, const char *option) {
  switch(toupper(*option)) {
  case '1':
    text_to_buffer(sock, "Enter a new list of keywords: ");
    return EDEDIT_KEYWORD;
  case '2':
    socketStartEditor(sock, text_editor, edescGetDescBuffer(edesc));
    return MENU_NOCHOICE;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool edesc_parser(SOCKET_DATA *sock, EDESC_DATA *edesc, int choice, 
		  const char *arg) {
  switch(choice) {
  case EDEDIT_KEYWORD:
    edescSetKeywords(edesc, arg);
    return TRUE;
  default:
    return FALSE;
  }
}



//*****************************************************************************
// functions for editing an extra description set
//*****************************************************************************
#define ESEDIT_DELETE     1
#define ESEDIT_EDIT       2

void edesc_set_menu(SOCKET_DATA *sock, EDESC_SET *set) {
  int i, entries = edescGetSetSize(set);

  send_to_socket(sock,
		 "{gEntries: {c%d\r\n", entries);
  // print out each one
  for(i = 0; i < entries; i++)
    send_to_socket(sock, "  {g%2d) {c%s\r\n", i, 
		   edescGetKeywords(edescSetGetNum(set, i)));
  send_to_socket(sock,
		 "\r\n"
		 "{gE) edit entry\r\n"
		 "{gN) new entry\r\n"
		 "{gD) delete entry\r\n"
		 );
}

int  edesc_set_chooser(SOCKET_DATA *sock, EDESC_SET *set, const char *option) {
  switch(toupper(*option)) {
  case 'E':
    text_to_buffer(sock, "Enter the number of the edesc to edit (-1 for none): ");
    return ESEDIT_EDIT;
  case 'N': {
    // create a new edesc
    EDESC_DATA *edesc = newEdesc("", "");
    edescSetPut(set, edesc);
    do_olc(sock, edesc_menu, edesc_chooser, edesc_parser, NULL, NULL, NULL,
	   NULL, edesc);
    return MENU_NOCHOICE;
  }
  case 'D':
    text_to_buffer(sock, "Enter the number of the edesc to delete: ");
    return ESEDIT_DELETE;
  default:
    return MENU_CHOICE_INVALID;
  }
}


bool edesc_set_parser(SOCKET_DATA *sock, EDESC_SET *set, int choice,
		       const char *arg) {
  switch(choice) {
  case ESEDIT_EDIT: {
    int num = atoi(arg);
    if(num == NOTHING)
      return TRUE;
    else {
      EDESC_DATA *edesc = edescSetGetNum(set, num);
      if(edesc == NULL)
	return FALSE;
      do_olc(sock, edesc_menu, edesc_chooser, edesc_parser, NULL, NULL, NULL,
	     NULL, edesc);
      return TRUE;
    }
  }
  case ESEDIT_DELETE: {
    EDESC_DATA *edesc = edescSetGetNum(set, atoi(arg));
    if(edesc) {
      removeEdesc(set, edesc);
      deleteEdesc(edesc);
    }
    return TRUE;
  }
  default:
    return FALSE;
  }
}
