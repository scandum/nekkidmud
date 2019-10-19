#ifndef __DYN_VARS_H
#define __DYN_VARS_H
//*****************************************************************************
//
// dyn_vars.c
//
// This module allows key/value pairs to be dynamically created on characters,
// objects, and rooms. Values can be strings, integers, or doubles. If a value
// is called for in the wrong type (e.g. you're trying to get a string as an 
// integer) the module will automagically handle the conversion. Variable types
// default to ints.
//
//*****************************************************************************

//
// This must be put at the top of mud.h so the rest of the MUD knows that
// we've got the alias module installed
// #define MODULE_DYN_VARS
//


// the different types of data we can store
#define DYN_VAR_STRING        0
#define DYN_VAR_INT           1
#define DYN_VAR_LONG          2
#define DYN_VAR_DOUBLE        3



//
// prepare dyn_vars for use
//
void         init_dyn_vars   (void);

int          charGetVarType   (CHAR_DATA *ch, const char *key);
int          charGetInt       (CHAR_DATA *ch, const char *key);
long         charGetLong      (CHAR_DATA *ch, const char *key);
double       charGetDouble    (CHAR_DATA *ch, const char *key);
const char  *charGetString    (CHAR_DATA *ch, const char *key);
void         charSetInt       (CHAR_DATA *ch, const char *key, int val);
void         charSetLong      (CHAR_DATA *ch, const char *key, long val);
void         charSetDouble    (CHAR_DATA *ch, const char *key, double val);
void         charSetString    (CHAR_DATA *ch, const char *key, const char *val);
bool         charHasVar       (CHAR_DATA *ch, const char *key);
void         charDeleteVar    (CHAR_DATA *ch, const char *key);

int          objGetVarType   (OBJ_DATA *ch, const char *key);
int          objGetInt       (OBJ_DATA *ch, const char *key);
long         objGetLong      (OBJ_DATA *ch, const char *key);
double       objGetDouble    (OBJ_DATA *ch, const char *key);
const char  *objGetString    (OBJ_DATA *ch, const char *key);
void         objSetInt       (OBJ_DATA *ch, const char *key, int val);
void         objSetLong      (OBJ_DATA *ch, const char *key, long val);
void         objSetDouble    (OBJ_DATA *ch, const char *key, double val);
void         objSetString    (OBJ_DATA *ch, const char *key, const char *val);
bool         objHasVar       (OBJ_DATA *ch, const char *key);
void         objDeleteVar    (OBJ_DATA *ch, const char *key);

int          roomGetVarType   (ROOM_DATA *ch, const char *key);
int          roomGetInt       (ROOM_DATA *ch, const char *key);
long         roomGetLong      (ROOM_DATA *ch, const char *key);
double       roomGetDouble    (ROOM_DATA *ch, const char *key);
const char  *roomGetString    (ROOM_DATA *ch, const char *key);
void         roomSetInt       (ROOM_DATA *ch, const char *key, int val);
void         roomSetLong      (ROOM_DATA *ch, const char *key, long val);
void         roomSetDouble    (ROOM_DATA *ch, const char *key, double val);
void         roomSetString    (ROOM_DATA *ch, const char *key, const char *val);
bool         roomHasVar       (ROOM_DATA *ch, const char *key);
void         roomDeleteVar    (ROOM_DATA *ch, const char *key);

#endif // __DYN_VARS_H
