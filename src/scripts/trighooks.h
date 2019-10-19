#ifndef TRIGHOOKS_H
#define TRIGHOOKS_H
//*****************************************************************************
//
// trighooks.h
//
// Triggers attach on to rooms, objects, and mobiles as hooks. When a hook
// event occurs, all of the triggers of the right type will run. The init 
// function here should not be touched by anything other than scripts.c
//
//*****************************************************************************

//
// called when init_scripts() is run
void init_trighooks(void);

//
// values for figuring out what "me" and optional variables are in gen_do_trigs
#define TRIGVAR_CHAR      0
#define TRIGVAR_OBJ       1
#define TRIGVAR_ROOM      2

//
// structure for defining extra variables when calling gen_do_trigs
typedef struct opt_var OPT_VAR;

//
// create and delete optional variables for use with gen_do_trigs
OPT_VAR *newOptVar(const char *name, void *data, int type);
void  deleteOptVar(OPT_VAR *var);

//
// generalized function for running all triggers of a specified type. First
// argument is the thing tirggers are being run for. Me must be a character,
// object, or room that owns the triggers. me_type is an integer value that
// specifies the owner. Type is the class of triggers to be run. Other variables
// appear as the same name in the trigger. Each can be NULL. If other variables
// are needed, a list of optionals can be provided, which must be deleted after
// use
void gen_do_trigs(void *me, int me_type, const char *type,
		  CHAR_DATA *ch,OBJ_DATA *obj, ROOM_DATA *room, EXIT_DATA *exit,
		  const char *cmd, const char *arg, LIST *optional);

//
// the trigger edit (tedit) menu displays a list of possible trigger types
// to choose from. That list can be extended by calling this function. desc
// should be a comma-separated list of things the trigger can be attached to
// (obj, mob, room, in that order)
void register_tedit_opt(const char *type, const char *desc); 

// returns a table of the available tedit opts
HASHTABLE *get_tedit_opts(void);

#endif // TRIGHOOKS_H
