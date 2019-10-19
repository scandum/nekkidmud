//*****************************************************************************
//
// accedit.c
//
// Accedit allows admins to edit player accounts online. Passwords and
// character lists can be edited.
//
//*****************************************************************************
#include "../mud.h"
#include "../utils.h"
#include "../account.h"
#include "../socket.h"
#include "../character.h"
#include "../save.h"

#include "olc.h"



//*****************************************************************************
// account editing
//*****************************************************************************
#define ACCEDIT_NEW_CHAR      1
#define ACCEDIT_DELETE_CHAR   2
#define ACCEDIT_PASSWORD      3



void accedit_menu(SOCKET_DATA *sock, ACCOUNT_DATA *acct) {
  LIST_ITERATOR *ch_i = newListIterator(accountGetChars(acct));
  char            *ch = NULL;
  send_to_socket(sock, "{g[{c%s{g]\r\n", accountGetName(acct));
  send_to_socket(sock, "Character list:{c\r\n");
  ITERATE_LIST(ch, ch_i) {
    send_to_socket(sock, "  %s\r\n", ch);
  } deleteListIterator(ch_i);
  send_to_socket(sock, 
		 "\r\n"
		 "{cN{n) Add new character\r\n"
		 "{cD{n) Delete character from account\r\n"
		 "{cP{n) Change password\r\n");
}

int accedit_chooser(SOCKET_DATA *sock, ACCOUNT_DATA *acct, const char *option) {
  switch(toupper(*option)) {
  case 'N':
    text_to_buffer(sock, "Enter character's name: ");
    return ACCEDIT_NEW_CHAR;
  case 'D':
    text_to_buffer(sock, "Enter character's name: ");
    return ACCEDIT_DELETE_CHAR;
  case 'P':
    text_to_buffer(sock, "Enter new password: ");
    return ACCEDIT_PASSWORD;
  default:
    return MENU_CHOICE_INVALID;
  }
}

bool accedit_parser(SOCKET_DATA *sock, ACCOUNT_DATA *acct, int choice, 
		  const char *arg) {
  switch(choice) {
  case ACCEDIT_NEW_CHAR:
    if(!*arg)
      return TRUE;
    else if(!player_exists(arg))
      return FALSE;
    else {
      listPutWith(accountGetChars(acct), strdup(arg), strcasecmp);
      return TRUE;
    }
  case ACCEDIT_DELETE_CHAR: {
    char *name = listRemoveWith(accountGetChars(acct), arg, strcasecmp);
    if(name != NULL) free(name);
    return TRUE;
  }
  case ACCEDIT_PASSWORD:
    // make sure the password meets our length requirements
    if(strlen(arg) < 4 || strlen(arg) > 12)
      return FALSE;
    else {
      accountSetPassword(acct, crypt(arg, accountGetName(acct)));
      return TRUE;
    }
  default:
    return FALSE;
  }
}

COMMAND(cmd_accedit) {
  if(!arg || !*arg)
    send_to_char(ch, "You must supply an account name, first!\r\n");
  else {
    ACCOUNT_DATA *acct = get_account(arg);
    if(acct == NULL)
      send_to_char(ch, "Account '%s' does not exist!\r\n", arg);
    else {
      do_olc(charGetSocket(ch), accedit_menu, accedit_chooser, accedit_parser,
	     NULL, NULL, unreference_account, save_account, acct);
    }
  }
}
