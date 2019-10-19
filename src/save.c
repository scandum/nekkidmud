//*****************************************************************************
//
// save.c
//
// contains all of the functions for the saving of characters and accounts.
// makes sure these things go into the right directories.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "socket.h"
#include "character.h"
#include "account.h"
#include "world.h"
#include "handler.h"
#include "body.h"
#include "object.h"
#include "room.h"
#include "storage.h"
#include "save.h"



//*****************************************************************************
// local data structures and variables
//*****************************************************************************

// tables of accounts and players currently referenced
HASHTABLE *account_table = NULL;
HASHTABLE *player_table  = NULL;

typedef struct {
  int refcnt;
  void *data;
} SAVE_REF_DATA;

SAVE_REF_DATA *newSaveRefData(void *data) {
  SAVE_REF_DATA *ref_data = malloc(sizeof(SAVE_REF_DATA));
  ref_data->refcnt = 1;
  ref_data->data   = data;
  return ref_data;
}

void deleteSaveRefData(SAVE_REF_DATA *ref_data) {
  free(ref_data);
}



//*****************************************************************************
// local functions
//*****************************************************************************

//
// Just a general function for finding the filename assocciated with the
// given information about a player.
#define FILETYPE_PFILE        0
#define FILETYPE_OFILE        1
#define FILETYPE_ACCOUNT      2
const char *get_save_filename(const char *name, int filetype) {
  static char buf[SMALL_BUFFER];
  static char pname[SMALL_BUFFER];
  int i, size;

  *buf = *pname = '\0';

  *pname = toupper(*name);
  size = strlen(name);
  for (i = 1; i < size; i++)
    pname[i] = tolower(*(name+i));
  pname[i] = '\0';

  switch(filetype) {
  case FILETYPE_ACCOUNT:
    sprintf(buf, "../lib/accounts/%c/%s.acct", *pname, pname);
    break;
  case FILETYPE_PFILE:
    sprintf(buf, "../lib/players/pfiles/%c/%s.pfile", *pname, pname);
    break;
  case FILETYPE_OFILE:
    sprintf(buf, "../lib/players/objfiles/%c/%s.ofile", *pname, pname);
    break;
  default: 
    log_string("ERROR: Tried to write filename for nonexistant char "
	       "file type, %d", filetype);
    break;
  }
  return buf;
}

bool player_creating(const char *name) {
  // a player is being created if it's attached to a socket and does not exist
  bool char_found       = FALSE;
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL;
  ITERATE_LIST(sock, sock_i) {
    CHAR_DATA *ch = socketGetChar(sock);
    if(ch == NULL) continue;
    if(!strcasecmp(charGetName(ch), name)) {
      char_found = TRUE;
      break;
    }
  } deleteListIterator(sock_i);

  return (char_found && !player_exists(name));
}

bool account_creating(const char *name) {
  // a player is being created if it's attached to a socket and does not exist
  bool       acct_found = FALSE;
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL;
  ITERATE_LIST(sock, sock_i) {
    ACCOUNT_DATA *acct = socketGetAccount(sock);
    if(acct == NULL) continue;
    if(!strcasecmp(accountGetName(acct), name)) {
      acct_found = TRUE;
      break;
    }
  } deleteListIterator(sock_i);

  return (acct_found && !account_exists(name));
}

bool player_exists(const char *name) {
  // there's two ways a character can exists. Either someone is already making
  // a character with that name, or there is a character with that name in
  // storage. We'll check both of these.
  const char *fname = get_save_filename(name, FILETYPE_PFILE);
  return file_exists(fname);
}

bool account_exists(const char *name) {
  const char *fname = get_save_filename(name, FILETYPE_ACCOUNT);
  return file_exists(fname);
}

void save_pfile(CHAR_DATA *ch) {
  STORAGE_SET *set = charStore(ch);
  storage_write(set, get_save_filename(charGetName(ch), FILETYPE_PFILE));
  storage_close(set);
}

void load_ofile(CHAR_DATA *ch) {
  STORAGE_SET *set = storage_read(get_save_filename(charGetName(ch), 
						    FILETYPE_OFILE));
  if(set == NULL)
    return;

  STORAGE_SET   *obj_set = NULL;
  OBJ_DATA          *obj = NULL;

  // inventory first
  STORAGE_SET_LIST *list = read_list(set, "inventory");
  while( (obj_set = storage_list_next(list)) != NULL) {
    obj = objRead(obj_set);
    obj_to_char(obj, ch);
  }

  // and then equipped items
  list = read_list(set, "equipment");
  while( (obj_set = storage_list_next(list)) != NULL) {
    if(storage_contains(obj_set, "object")) {
      obj = objRead(read_set(obj_set, "object"));
      if(!do_equip(ch, obj, read_string(obj_set, "equipped"), TRUE))
	obj_to_char(obj, ch);
    }
  }

  storage_close(set);
}

void save_objfile(CHAR_DATA *ch) {
  STORAGE_SET *set = new_storage_set();
  // write all of the inventory
  store_list(set, "inventory", gen_store_list(charGetInventory(ch), objStore));
  
  // for equipped items, it's not so easy - we also have to record
  // whereabouts on the body the equipment was worn on
  STORAGE_SET_LIST *list = new_storage_list();
  LIST *eq_list = bodyGetAllEq(charGetBody(ch));
  OBJ_DATA *obj = NULL;
  while((obj = listPop(eq_list)) != NULL) {
    STORAGE_SET *eq_set = new_storage_set();
    store_string(eq_set, "equipped", bodyEquippedWhere(charGetBody(ch), obj));
    store_set   (eq_set, "object",   objStore(obj));
    storage_list_put(list, eq_set);
  }
  deleteList(eq_list);

  store_list(set, "equipment", list);
  storage_write(set, get_save_filename(charGetName(ch), FILETYPE_OFILE));
  storage_close(set);
}

CHAR_DATA *load_player(const char *player) {
  STORAGE_SET *set = storage_read(get_save_filename(player, FILETYPE_PFILE));
  if(set == NULL)
    return NULL;
  else {
    CHAR_DATA   *ch  = charRead(set);
    storage_close(set);
    load_ofile(ch);
    return ch;
  }
}

ACCOUNT_DATA *load_account(const char *account) {
  STORAGE_SET   *set = storage_read(get_save_filename(account,
						      FILETYPE_ACCOUNT));
  if(set == NULL)
    return NULL;
  else {
    ACCOUNT_DATA *acct = accountRead(set);
    storage_close(set);
    return acct;
  }
}


//*****************************************************************************
// implementation of save.h
//*****************************************************************************
void init_save(void) {
  account_table = newHashtable();
  player_table  = newHashtable();
}

ACCOUNT_DATA *get_account(const char *account) {
  SAVE_REF_DATA *ref_data = hashGet(account_table, account);
  // account is not loaded in memory... try loading it
  if(ref_data == NULL) {
    ACCOUNT_DATA *acct = load_account(account);
    // it doesn't exist. Return NULL
    if(acct == NULL)
      return NULL;
    else {
      hashPut(account_table, account, newSaveRefData(acct));
      return acct;
    }
  }
  // up our reference count and return
  else {
    ref_data->refcnt++;
    return ref_data->data;
  }
}

CHAR_DATA *get_player(const char *player) {
  SAVE_REF_DATA *ref_data = hashGet(player_table, player);
  // player is not loaded in memory... try loading it
  if(ref_data == NULL) {
    CHAR_DATA *ch = load_player(player);
    // it doesn't exist. Return NULL
    if(ch == NULL)
      return NULL;
    else {
      hashPut(player_table, player, newSaveRefData(ch));
      return ch;
    }
  }
  // up our reference count and return
  else {
    ref_data->refcnt++;
    return ref_data->data;
  }
}

void unreference_account(ACCOUNT_DATA *account) {
  SAVE_REF_DATA *ref_data = hashGet(account_table, accountGetName(account));
  if(ref_data == NULL)
    log_string("ERROR: Tried unreferencing account '%s' with no references!",
	       accountGetName(account));
  else {
    ref_data->refcnt--;
    // are we at 0 references?
    if(ref_data->refcnt == 0) {
      hashRemove(account_table, accountGetName(account));
      deleteAccount(account);
      deleteSaveRefData(ref_data);
    }
  }
}

void unreference_player(CHAR_DATA *ch) {
  SAVE_REF_DATA *ref_data = hashGet(player_table, charGetName(ch));
  if(ref_data == NULL)
    log_string("ERROR: Tried unreferencing player '%s' with no references!",
	       charGetName(ch));
  else {
    ref_data->refcnt--;
    // are we at 0 references?
    if(ref_data->refcnt == 0) {
      hashRemove(player_table, charGetName(ch));
      deleteChar(ch);
      deleteSaveRefData(ref_data);
    }
  }
}


void reference_account(ACCOUNT_DATA *account) {
  SAVE_REF_DATA *ref_data = hashGet(account_table, accountGetName(account));
  if(ref_data == NULL)
    log_string("ERROR: Tried referencing account '%s' that is not loaded!",
	       accountGetName(account));
  else
    ref_data->refcnt++;
}

void reference_player(CHAR_DATA *ch) {
  SAVE_REF_DATA *ref_data = hashGet(player_table, charGetName(ch));
  if(ref_data == NULL)
    log_string("ERROR: Tried referencing player '%s' that is not loaded!",
	       charGetName(ch));
  else
    ref_data->refcnt++;
}

void register_account(ACCOUNT_DATA *account) {
  if(account_exists(accountGetName(account)))
    log_string("ERROR: Tried to register already-registered account, '%s'",
	       accountGetName(account));
  else {
    hashPut(account_table, accountGetName(account), newSaveRefData(account));
    save_account(account);
  }
}

void register_player(CHAR_DATA *ch) {
  if(player_exists(charGetName(ch)))
    log_string("ERROR: Tried to register already-registered player, '%s'",
	       charGetName(ch));
  else {
    hashPut(player_table, charGetName(ch), newSaveRefData(ch));
    save_player(ch);
  }
}

void save_account(ACCOUNT_DATA *account) {
  if(!account) return;
  STORAGE_SET *set = accountStore(account);
  storage_write(set, get_save_filename(accountGetName(account),
				       FILETYPE_ACCOUNT));
  storage_close(set);
}

void save_player(CHAR_DATA *ch) {
  if (ch == NULL) return;

  // make sure we have a UID for the character before we start saving
  if(charGetUID(ch) == NOBODY) {
    log_string("ERROR: %s has invalid UID (%d)", charGetName(ch), NOBODY);
    send_to_char(ch, "You have an invalid ID. Please inform a god.\r\n");
    return;
  }

  // make sure we'll load back into the same room we were saved in
  charSetLoadroom(ch, (charGetRoom(ch) ? roomGetClass(charGetRoom(ch)) :
		       START_ROOM));

  save_objfile(ch);    // save the player's objects
  save_pfile(ch);      // saves the actual player data
}
