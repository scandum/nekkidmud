#ifndef ACCOUNT_H
#define ACCOUNT_H
//*****************************************************************************
//
// account.h
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

ACCOUNT_DATA       *newAccount(void);
void             deleteAccount(ACCOUNT_DATA *account);
ACCOUNT_DATA      *accountRead(STORAGE_SET *set);
STORAGE_SET      *accountStore(ACCOUNT_DATA *account);
void             accountCopyTo(ACCOUNT_DATA *from, ACCOUNT_DATA *to);
ACCOUNT_DATA      *accountCopy(ACCOUNT_DATA *account);
void            accountPutChar(ACCOUNT_DATA *account, const char *name);
void         accountRemoveChar(ACCOUNT_DATA *account, const char *name);
LIST          *accountGetChars(ACCOUNT_DATA *account);
void  *accountGetAuxiliaryData(ACCOUNT_DATA *account, const char *data);
void        accountSetPassword(ACCOUNT_DATA *account, const char *password);
const char *accountGetPassword(ACCOUNT_DATA *account);
void            accountSetName(ACCOUNT_DATA *account, const char *name);
const char     *accountGetName(ACCOUNT_DATA *account);

#endif // ACCOUNT_H
