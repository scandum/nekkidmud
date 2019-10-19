#ifndef HOOKS_H
#define HOOKS_H
//*****************************************************************************
//
// hooks.h
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

//
// random character we use for denoting the beginning/end of a string
//
// THIS NEEDS TO BE CHANGED TO SOMETHING LESS INSIDIOUS! If we're building
// strings with escape sequences, this could be deadly.
#define HOOK_STR_MARKER '\032'

//
// prepare hooks for use
void init_hooks(void);

void hookRun(const char *type, const char *info);
void hookAdd(const char *type, void (* func)(const char *));
void hookAddMonitor(void (* func)(const char *, const char *));
void hookRemove(const char *type, void (* func)(const char *));
void hookParseInfo(const char *info, ...);
const char *hookBuildInfo(const char *format, ...);
LIST *parse_hook_info_tokens(const char *info);

#endif // HOOKS_H
