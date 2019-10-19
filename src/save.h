#ifndef __SAVE_H
#define __SAVE_H
//*****************************************************************************
//
// save.h
//
// functions having to do with saving and loading accounts and player (and 
// their objects).
//
//*****************************************************************************

//
// ACCOUNT_DATA *load_account(const char   *account);
// CHAR_DATA    *load_player (const char   *player);
//
// Use of these functions has been deprecated. They are dangerous and can be the
// source of bugs if people try loading accounts/players already loaded. 
// Instead, I've opted for a less bug-prone system. Now, accounts and players
// have "get" functions. Whenever an account or player is got, a reference 
// counter for it is increased. When work with the account/player is finished, 
// it must be "unreferenced". When a player/account's references reach zero, it
// is freed from memory. Players and accounts ARE NOT automatically saved to
// disk before they are freed to memory (they should be, but because of how
// extract_mobile is designed, it's not really feasible to do this).
// Consequently, save now needs an init function and accounts/players must 
// be registered when they are first created.

// called at mud boot-up
void init_save(void);

ACCOUNT_DATA *get_account(const char *account);
CHAR_DATA     *get_player(const char *player);

void  unreference_account(ACCOUNT_DATA *account);
void   unreference_player(CHAR_DATA    *ch);
void    reference_account(ACCOUNT_DATA *account);
void     reference_player(CHAR_DATA    *ch);

void     register_account(ACCOUNT_DATA *account);
void      register_player(CHAR_DATA    *ch);

void         save_account(ACCOUNT_DATA *account);
void          save_player(CHAR_DATA    *ch);

bool       account_exists(const char *name);
bool        player_exists(const char *name);

bool     account_creating(const char *name);
bool      player_creating(const char *name);

#endif // __SAVE_H
