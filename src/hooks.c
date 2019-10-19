//*****************************************************************************
//
// hooks.c
//
// Hooks are chunks of code that attach on to parts of game code, but aren't
// really parts of the game code. Hooks can be used for a number of things. For
// instance: processing room descriptions by outside modules before they are
// displayed to a character, running triggers when certain events happen, or
// perhaps logging how many time a room is entered or exited. We would probably
// not want to hard-code any of these things into the core of the mud if they
// are fairly stand-alone. So instead, we write hooks that attach into the game
// and execute when certain events happen.
//
// Often events that will execute hooks are set off by someone or something
// taking an action. Thus, all hooks take 3 arguments (actor, acted, arg) to
// make it easy to handle these cases. These 3 arguments do not need to be used
// for all hooks, however.
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "character.h"
#include "object.h"
#include "room.h"
#include "exit.h"
#include "account.h"
#include "socket.h"
#include "hooks.h"



//*****************************************************************************
// local functions, variables, and definitions
//*****************************************************************************

// the table of all our installed hooks
HASHTABLE *hook_table   = NULL;

// the list of functions called whenever a hook is run
LIST *monitors = NULL;

// a buffer for building hook info on
BUFFER      *info_buf = NULL;



//*****************************************************************************
// implementation of hooks.h
//*****************************************************************************
void init_hooks(void) {
  // make our required variables
  hook_table = newHashtable();
  info_buf   = newBuffer(1);
  monitors   = newList();
}

void hookRemove(const char *type, void (* func)(const char *)) {
  LIST *list = hashGet(hook_table, type);
  if(list != NULL) listRemove(list, func);
}

void hookAdd(const char *type, void (* func)(const char *)) {
  LIST *list = hashGet(hook_table, type);
  if(list == NULL) {
    list = newList();
    hashPut(hook_table, type, list);
  }
  listQueue(list, func);
}

void hookAddMonitor(void (* func)(const char *, const char *)) {
  listQueue(monitors, func);
}

void hookRun(const char *type, const char *info) {
  LIST *list = hashGet(hook_table, type);
  char *info_dup = strdup(info);
  if(list != NULL) {
    LIST_ITERATOR *list_i = newListIterator(list);
    void (* func)(const char *) = NULL;
    ITERATE_LIST(func, list_i) {
      func(info_dup);
    } deleteListIterator(list_i);
  }

  // run our monitors
  LIST_ITERATOR *mon_i = newListIterator(monitors);
  void (* mon)(const char *, const char *) = NULL;
  ITERATE_LIST(mon, mon_i) {
    mon(type, info_dup);
  } deleteListIterator(mon_i);
  free(info_dup);
}

const char *hookBuildInfo(const char *format, ...) {
  // clear our workspace
  bufferClear(info_buf);

  // parse out all of our tokens
  LIST *tokens           = parse_strings(format, ' ');
  LIST_ITERATOR *token_i = newListIterator(tokens);
  char *token            = NULL;
  int   len              = listSize(tokens);
  int   count            = 0;

  // go through all of our tokens
  va_list vargs;
  va_start(vargs, format);
  ITERATE_LIST(token, token_i) {
    if(!strcasecmp(token, "ch"))
      bprintf(info_buf, "ch.%d", charGetUID(va_arg(vargs, CHAR_DATA *)));
    else if(!strcasecmp(token, "obj"))
      bprintf(info_buf, "obj.%d", objGetUID(va_arg(vargs, OBJ_DATA *)));
    else if(!strcasecmp(token, "rm") || !strcasecmp(token, "room"))
      bprintf(info_buf, "rm.%d", roomGetUID(va_arg(vargs, ROOM_DATA *)));
    else if(!strcasecmp(token, "ex") || !strcasecmp(token, "exit"))
      bprintf(info_buf, "ex.%d", exitGetUID(va_arg(vargs, EXIT_DATA *)));
    else if(!strcasecmp(token, "sk") || !strcasecmp(token, "sock"))
      bprintf(info_buf, "sk.%d", socketGetUID(va_arg(vargs, SOCKET_DATA *)));
    else if(!strcasecmp(token, "str"))
      bprintf(info_buf, "%c%s%c", HOOK_STR_MARKER, va_arg(vargs, char *), HOOK_STR_MARKER);
    else if(!strcasecmp(token, "int"))
      bprintf(info_buf, "%d", va_arg(vargs, int));
    else if(!strcasecmp(token, "dbl"))
      bprintf(info_buf, "%lf", va_arg(vargs, double));
    // unknown type -- abort!
    else
      break;

    // add a space for the next token to be printed
    if(count < len - 1)
      bprintf(info_buf, " ");
    count++;
  } deleteListIterator(token_i);
  deleteListWith(tokens, free);
  va_end(vargs);
  return bufferString(info_buf);
}

//
// parses up info tokens
LIST *parse_hook_info_tokens(const char *info) {
  LIST *tokens = newList();
  BUFFER  *buf = newBuffer(1);
  while(*info) {
    // skip leading spaces
    while(isspace(*info))
      info++;

    char marker = ' ';
    bufferClear(buf);
    
    // are we parsing a string or something else?
    if(*info == HOOK_STR_MARKER) {
      marker = HOOK_STR_MARKER;
      bprintf(buf, "%c", HOOK_STR_MARKER);
      info++;
    }

    // fill up to the end marker
    for(;*info && *info != marker; info++)
      bprintf(buf, "%c", *info);

    // were we parsing a string?
    if(marker == HOOK_STR_MARKER)
      bprintf(buf, "%c", HOOK_STR_MARKER);

    // skip past our marker
    if(*info) info++;
    
    // append our token
    listQueue(tokens, strdup(bufferString(buf)));
  }
  deleteBuffer(buf);
  return tokens;
}

void hookParseInfo(const char *info, ...) {
  // parse out all of our tokens
  LIST *tokens           = parse_hook_info_tokens(info);
  LIST_ITERATOR *token_i = newListIterator(tokens);
  char *token            = NULL;

  // id number we'll need for parsing some values
  int id = 0;

  // go through all of our tokens
  va_list vargs;
  va_start(vargs, info);
  ITERATE_LIST(token, token_i) {
    if(startswith(token, "ch")) {
      sscanf(token, "ch.%d", &id);
      *va_arg(vargs, CHAR_DATA **) = propertyTableGet(mob_table, id);
    }
    else if(startswith(token, "obj")) {
      sscanf(token, "obj.%d", &id);
      *va_arg(vargs, OBJ_DATA **) = propertyTableGet(obj_table, id);
    }
    else if(startswith(token, "rm")) {
      sscanf(token, "rm.%d", &id);
      *va_arg(vargs, ROOM_DATA **) = propertyTableGet(room_table, id);
    }
    else if(startswith(token, "room")) {
      sscanf(token, "room.%d", &id);
      *va_arg(vargs, ROOM_DATA **) = propertyTableGet(room_table, id);
    }
    else if(startswith(token, "ex")) {
      sscanf(token, "ex.%d", &id);
      *va_arg(vargs, EXIT_DATA **) = propertyTableGet(exit_table, id);
    }
    else if(startswith(token, "exit")) {
      sscanf(token, "exit.%d", &id);
      *va_arg(vargs, EXIT_DATA **) = propertyTableGet(exit_table, id);
    }
    else if(startswith(token, "sk")) {
      sscanf(token, "sk.%d", &id);
      *va_arg(vargs, SOCKET_DATA **) = propertyTableGet(sock_table, id);
    }
    else if(startswith(token, "sock")) {
      sscanf(token, "sk.%d", &id);
      *va_arg(vargs, SOCKET_DATA **) = propertyTableGet(sock_table, id);
    }
    else if(*token == HOOK_STR_MARKER) {
      char *str = strdup(token + 1);
      str[strlen(str)-1] = '\0';
      *va_arg(vargs, char **) = str;
    }
    else if(isdigit(*token)) {
      // integer or double?
      if(next_letter_in(token, '.') > -1)
	*va_arg(vargs, double *) = atof(token);
      else
	*va_arg(vargs, int *) = atoi(token);
    }
  } deleteListIterator(token_i);
  deleteListWith(tokens, free);
  va_end(vargs);
}
