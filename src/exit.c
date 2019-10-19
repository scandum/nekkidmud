//*****************************************************************************
//
// exit.c
//
// exits are structures that keep information about the links between rooms.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "storage.h"
#include "exit.h"

#define EX_CLOSED            (1 << 0)
#define EX_LOCKED            (1 << 1)
#define EX_CLOSABLE          (1 << 2)
// lockable is handled if the exit has a key

struct exit_data {
  char *name;              // what is the name of our door for descriptions?
  char *keywords;          // what keywords can the door be referenced by?
  char *opposite;          // what is our opposite direction, if any?
  char *to;                // where do we exit to?
  char *key;               // what is the key's prototype?
  BUFFER *desc;            // what does a person see when they look at us?
  ROOM_DATA *room;         // the room we're attached to

  char *spec_enter;        // the message when we enter from this exit
  char *spec_leave;        // the message when we leave through this exit

  bitvector_t status;      // closable, closed, locked, etc...

  int hide_lev;            // how hidden is this exit?
  int pick_lev;            // how hard is it to pick this exit?
  int uid;                 // our unique identification number
};



EXIT_DATA *newExit() {
  EXIT_DATA *exit = malloc(sizeof(EXIT_DATA));
  exit->name        = strdup("");
  exit->keywords    = strdup("");
  exit->opposite    = strdup("");
  exit->spec_enter  = strdup("");
  exit->spec_leave  = strdup("");
  exit->to          = strdup("");
  exit->key         = strdup("");
  exit->desc        = newBuffer(1);
  exit->hide_lev    = 0;
  exit->pick_lev    = 0;
  exit->status      = 0;
  exit->room        = NULL;
  exit->uid         = next_uid();
  return exit;
};


void deleteExit(EXIT_DATA *exit) {
  if(exit->name)        free(exit->name);
  if(exit->spec_enter)  free(exit->spec_enter);
  if(exit->spec_leave)  free(exit->spec_leave);
  if(exit->keywords)    free(exit->keywords);
  if(exit->opposite)    free(exit->opposite);
  if(exit->to)          free(exit->to);
  if(exit->key)         free(exit->key);
  if(exit->desc)        deleteBuffer(exit->desc);

  free(exit);
};


void exitCopyTo(const EXIT_DATA *from, EXIT_DATA *to) {
  exitSetTo       (to, exitGetTo(from));
  exitSetName     (to, exitGetName(from));
  exitSetKeywords (to, exitGetKeywords(from));
  exitSetDesc     (to, exitGetDesc(from));
  exitSetTo       (to, exitGetTo(from));
  exitSetPickLev  (to, exitGetPickLev(from));
  exitSetHidden   (to, exitGetHidden(from));
  exitSetKey      (to, exitGetKey(from));
  exitSetLocked   (to, exitIsLocked(from));
  exitSetClosed   (to, exitIsClosed(from));
  exitSetClosable (to, exitIsClosable(from));
  exitSetSpecEnter(to, exitGetSpecEnter(from));
  exitSetSpecLeave(to, exitGetSpecLeave(from));
  exitSetOpposite (to, exitGetOpposite(from));
}


EXIT_DATA *exitCopy(const EXIT_DATA *exit) {
  EXIT_DATA *newexit = newExit();
  exitCopyTo(exit, newexit);
  return newexit;
}

EXIT_DATA *exitRead(STORAGE_SET *set) {
  EXIT_DATA *exit = newExit();
  exitSetName(exit,      read_string(set, "name"));
  exitSetKeywords(exit,  read_string(set, "keywords"));
  exitSetOpposite(exit,  read_string(set, "opposite"));
  exitSetDesc(exit,      read_string(set, "desc"));
  exitSetSpecEnter(exit, read_string(set, "enter"));
  exitSetSpecLeave(exit, read_string(set, "leave"));
  exitSetTo(exit,        read_string(set, "to"));
  exitSetKey(exit,       read_string(set, "key"));
  exitSetHidden(exit,    read_int   (set, "hide_level"));
  exitSetPickLev(exit,   read_int   (set, "pick_level"));
  exitSetClosable(exit,  read_bool  (set, "closable"));
  exitSetClosed(exit,    read_bool  (set, "closed"));
  exitSetLocked(exit,    read_bool  (set, "locked"));
  return exit;
}

STORAGE_SET *exitStore(EXIT_DATA *exit) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "name",       exit->name);
  store_string(set, "keywords",   exit->keywords);
  store_string(set, "opposite",   exit->opposite);
  store_string(set, "desc",       bufferString(exit->desc));
  store_string(set, "enter",      exit->spec_enter);
  store_string(set, "leave",      exit->spec_leave);
  store_string(set, "to",         exit->to);
  store_string(set, "key",        exit->key);
  store_int   (set, "hide_level", exit->hide_lev);
  store_int   (set, "pick_level", exit->pick_lev);
  store_bool  (set, "closable",   exitIsClosable(exit));
  store_bool  (set, "closed",     exitIsClosed(exit));
  store_bool  (set, "locked",     exitIsLocked(exit));
  return set;
}



//*****************************************************************************
// is, get and set functions
//*****************************************************************************
bool        exitIsClosable(const EXIT_DATA *exit) {
  return IS_SET(exit->status, EX_CLOSABLE);
};

bool        exitIsClosed(const EXIT_DATA *exit) {
  return IS_SET(exit->status, EX_CLOSED);
};

bool        exitIsLocked(const EXIT_DATA *exit) {
  return IS_SET(exit->status, EX_LOCKED);
};

bool        exitIsName(const EXIT_DATA *exit, const char *name) {
  return is_keyword(exit->keywords, name, TRUE);
}

int         exitGetUID(const EXIT_DATA *exit) {
  return exit->uid;
}

int         exitGetHidden(const EXIT_DATA *exit) {
  return exit->hide_lev;
}

int         exitGetPickLev(const EXIT_DATA *exit) {
  return exit->pick_lev;
}

const char *exitGetKey(const EXIT_DATA *exit) {
  return exit->key;
}

const char *exitGetTo(const EXIT_DATA *exit) {
  return exit->to;
}

const char *exitGetName(const EXIT_DATA *exit) {
  return exit->name;
}

const char *exitGetKeywords(const EXIT_DATA *exit) {
  return exit->keywords;
}

const char *exitGetOpposite(const EXIT_DATA *exit) {
  return exit->opposite;
}

const char *exitGetDesc(const EXIT_DATA *exit) {
  return bufferString(exit->desc);
}

const char *exitGetSpecEnter(const EXIT_DATA *exit) {
  return exit->spec_enter;
}

const char *exitGetSpecLeave(const EXIT_DATA *exit) {
  return exit->spec_leave;
}

BUFFER *exitGetDescBuffer(const EXIT_DATA *exit) {
  return exit->desc;
}

ROOM_DATA *exitGetRoom(const EXIT_DATA *exit) {
  return exit->room;
}

void        exitSetClosable(EXIT_DATA *exit, bool closable) {
  if(closable) SET_BIT(exit->status, EX_CLOSABLE);
  else         REMOVE_BIT(exit->status, EX_CLOSABLE);
}

void        exitSetClosed(EXIT_DATA *exit, bool closed) {
  if(closed)    SET_BIT(exit->status, EX_CLOSED);
  else          REMOVE_BIT(exit->status, EX_CLOSED);
}

void        exitSetLocked(EXIT_DATA *exit, bool locked) {
  if(locked)    SET_BIT(exit->status, EX_LOCKED);
  else          REMOVE_BIT(exit->status, EX_LOCKED);
}

void        exitSetKey(EXIT_DATA *exit, const char *key) {
  if(exit->key) free(exit->key);
  exit->key = strdupsafe(key);
}

void        exitSetHidden(EXIT_DATA *exit, int hide_lev) {
  exit->hide_lev = hide_lev;
}

void        exitSetPickLev(EXIT_DATA *exit, int pick_lev) {
  exit->pick_lev = pick_lev;
}

void        exitSetTo(EXIT_DATA *exit, const char *room) {
  if(exit->to) free(exit->to);
  exit->to = strdupsafe(room);
}

void        exitSetName(EXIT_DATA *exit, const char *name) {
  if(exit->name) free(exit->name);
  exit->name   = strdupsafe(name);
}

void        exitSetKeywords(EXIT_DATA *exit, const char *keywords) {
  if(exit->keywords) free(exit->keywords);
  exit->keywords   = strdupsafe(keywords);
}

void        exitSetOpposite(EXIT_DATA *exit, const char *opposite) {
  if(exit->opposite) free(exit->opposite);
  exit->opposite   = strdupsafe(opposite);
}

void        exitSetDesc(EXIT_DATA *exit, const char *desc) {
  bufferClear(exit->desc);
  bufferCat(exit->desc, (desc ? desc : ""));
}

void        exitSetSpecEnter(EXIT_DATA *exit, const char *enter) {
  if(exit->spec_enter)  free(exit->spec_enter);
  exit->spec_enter    = strdupsafe(enter);
}

void        exitSetSpecLeave(EXIT_DATA *exit, const char *leave) {
  if(exit->spec_leave)  free(exit->spec_leave);
  exit->spec_leave    = strdupsafe(leave);
}

void exitSetRoom(EXIT_DATA *exit, ROOM_DATA *room) {
  exit->room = room;
}
