//*****************************************************************************
//
// account.c
//
// Players first create an account, and then they can begin creating players.
// The main purpose of an account is to hold all of the information relevant to
// one player, which might extend all of that player's game characters. If you 
// plan on adding any other information to accounts, it is strongly suggested 
// you do so through auxiliary data (see auxiliary.h).
//
// For a recap, IF YOU PLAN ON ADDING ANY OTHER INFORMATION TO ACCOUNTS, IT
// IS STRONGLY SUGGESTED YOU DO SO THROUGH AUXILIARY DATA (see auxiliary.h).
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "auxiliary.h"
#include "storage.h"
#include "account.h"



//*****************************************************************************
// local datastructures, variables, defines, and functions
//*****************************************************************************
struct account_data {
  char          *name; // the name of our account
  char      *password; // our password to log on
  LIST    *characters; // our list of character names
  AUX_TABLE      *aux; // auxiliary data we have installed
};


//
// returns a storage set with a "name" key in it, corresponding to the argument
// passed in.
STORAGE_SET *accountStoreChar(const char *name) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "name", name);
  return set;
}


//
// parses out a character name from a storage set holding an account character
// entry.
char *accountReadChar(STORAGE_SET *set) {
  return strdup(read_string(set, "name"));
}



//*****************************************************************************
// implementation of account.h
//*****************************************************************************
ACCOUNT_DATA *newAccount(void) {
  ACCOUNT_DATA *account = malloc(sizeof(ACCOUNT_DATA));
  account->name         = strdup("");
  account->password     = strdup("");
  account->aux          = newAuxiliaryData(AUXILIARY_TYPE_ACCOUNT);
  account->characters   = newList();
  return account;
}

void deleteAccount(ACCOUNT_DATA *account) {
  if(account->name)       free(account->name);
  if(account->password)   free(account->password);
  if(account->aux)        deleteAuxiliaryData(account->aux);
  if(account->characters) deleteListWith(account->characters, free);
  free(account);
}

ACCOUNT_DATA *accountRead(STORAGE_SET *set) {
  ACCOUNT_DATA *account = calloc(1, sizeof(ACCOUNT_DATA));
  accountSetName(account,            read_string(set, "name"));
  accountSetPassword(account,    read_string(set, "password"));
  account->aux = auxiliaryDataRead(read_set(set, "auxiliary"),
				   AUXILIARY_TYPE_ACCOUNT);
  account->characters = gen_read_list(read_list(set, "characters"), 
				      accountReadChar);
  listSortWith(account->characters, strcasecmp);
  return account;
}

STORAGE_SET *accountStore(ACCOUNT_DATA *account) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "name",     account->name);
  store_string(set, "password", account->password);
  store_list  (set, "characters", gen_store_list(account->characters, 
						 accountStoreChar));
  store_set   (set, "auxiliary", auxiliaryDataStore(account->aux));
  return set;
}

void accountCopyTo(ACCOUNT_DATA *from, ACCOUNT_DATA *to) {
  if(to->characters) deleteListWith(to->characters, free);
  auxiliaryDataCopyTo(from->aux, to->aux);
  accountSetName(to, from->name);
  accountSetPassword(to, from->password);
  to->characters = listCopyWith(from->characters, strdup);
}

ACCOUNT_DATA *accountCopy(ACCOUNT_DATA *account) {
  ACCOUNT_DATA *newacct = newAccount();
  accountCopyTo(account, newacct);
  return newacct;
}

void accountPutChar(ACCOUNT_DATA *account, const char *name) {
  // make sure it's not already in
  if(!listGetWith(account->characters, name, strcasecmp))
    listPut(account->characters, strdup(name));
}

void accountRemoveChar(ACCOUNT_DATA *account, const char *name) {
  char *elem = listRemoveWith(account->characters, name, strcasecmp);
  if(elem) free(elem);
}

LIST *accountGetChars(ACCOUNT_DATA *account) {
  return account->characters;
}

void *accountGetAuxiliaryData(ACCOUNT_DATA *account, const char *data) {
  return auxiliaryGet(account->aux, data);
}

void accountSetPassword(ACCOUNT_DATA *account, const char *password) {
  if(account->password) free(account->password);
  account->password = strdupsafe(password);
}

const char *accountGetPassword(ACCOUNT_DATA *account) {
  return account->password;
}

void accountSetName(ACCOUNT_DATA *account, const char *name) {
  if(account->name) free(account->name);
  account->name = strdupsafe(name);
}

const char *accountGetName(ACCOUNT_DATA *account) {
  return account->name;
}
