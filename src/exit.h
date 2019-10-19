#ifndef __EXIT_H
#define __EXIT_H
//*****************************************************************************
//
// exit.h
//
// exits are structures that keep information about the links between rooms.
//
//*****************************************************************************


//
// Create a new exit
//
EXIT_DATA *newExit();


//
// Delete an exit
//
void deleteExit(EXIT_DATA *exit);


//
// Make a copy of the exit
//
EXIT_DATA *exitCopy(const EXIT_DATA *exit);


//
// Copy the data from one exit to another
//
void exitCopyTo(const EXIT_DATA *from, EXIT_DATA *to);


//
// read an exit in from the storage set
//
EXIT_DATA *exitRead(STORAGE_SET *set);


//
// store the exit data in a storage set
//
STORAGE_SET *exitStore(EXIT_DATA *exit);


//*****************************************************************************
// is, get and set functions
//*****************************************************************************
bool        exitIsName         (const EXIT_DATA *exit, const char *name);
bool        exitIsClosable     (const EXIT_DATA *exit);
bool        exitIsClosed       (const EXIT_DATA *exit);
bool        exitIsLocked       (const EXIT_DATA *exit);

int         exitGetUID         (const EXIT_DATA *exit);
int         exitGetHidden      (const EXIT_DATA *exit);
int         exitGetPickLev     (const EXIT_DATA *exit);
const char *exitGetKey         (const EXIT_DATA *exit);
const char *exitGetTo          (const EXIT_DATA *exit);
const char *exitGetName        (const EXIT_DATA *exit);
const char *exitGetKeywords    (const EXIT_DATA *exit);
const char *exitGetOpposite    (const EXIT_DATA *exit);
const char *exitGetDesc        (const EXIT_DATA *exit);
const char *exitGetSpecLeave   (const EXIT_DATA *exit);
const char *exitGetSpecEnter   (const EXIT_DATA *exit);
BUFFER     *exitGetDescBuffer  (const EXIT_DATA *exit);
ROOM_DATA  *exitGetRoom        (const EXIT_DATA *exit);

void        exitSetClosable    (EXIT_DATA *exit, bool closable);
void        exitSetClosed      (EXIT_DATA *exit, bool closed);
void        exitSetLocked      (EXIT_DATA *exit, bool locked);
void        exitSetHidden      (EXIT_DATA *exit, int hide_lev);
void        exitSetPickLev     (EXIT_DATA *exit, int pick_lev);
void        exitSetKey         (EXIT_DATA *exit, const char *key);
void        exitSetTo          (EXIT_DATA *exit, const char *key);
void        exitSetName        (EXIT_DATA *exit, const char *name);
void        exitSetOpposite    (EXIT_DATA *exit, const char *opposite);
void        exitSetKeywords    (EXIT_DATA *exit, const char *keywords);
void        exitSetDesc        (EXIT_DATA *exit, const char *desc);
void        exitSetSpecLeave   (EXIT_DATA *exit, const char *leave);
void        exitSetSpecEnter   (EXIT_DATA *exit, const char *enter);
void        exitSetRoom        (EXIT_DATA *exit, ROOM_DATA *room);

#endif // __EXIT_H
