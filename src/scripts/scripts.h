#ifndef SCRIPTS_H
#define SCRIPTS_H
//*****************************************************************************
//
// scripts.h
//
// NakedMud makes extensive use of scripting. It uses scripting to generate
// objects, mobiles, and rooms when they are loaded into the game. There are
// also scripting hooks for these things (commonly referred to as triggers), 
// which allow them to be a bit more dynamic and flavorful in the game. For
// instance, greetings when someone enters a room, repsonses to questions, 
// actions when items are received.. you know... that sort of stuff. 
//
//*****************************************************************************

//
// these includes are needed by anything that deals with scripts. Might as
// well put them in here so people do not always have to include them manually
// One problem we run into is that Python.h needs to define a certain value for
// _POSIX_C_SOURCE. However, it never checks if _POSIX_C_SOURCE has already
// been defined. So, we have to do it here.
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#include <Python.h>
#include <structmember.h>

//
// this is also needed by anything that touches scripts
typedef struct trigger_data TRIGGER_DATA;

//
// called before scripts can be used
void  init_scripts(void);



//*****************************************************************************
// trigger function definitions
//*****************************************************************************

//
// triggers have 4 components: their name, their type, their key, and their
// code. A name is just a short description of what the trigger's purpose is.
// For instance, "bob's greeting trigger". The trigger type determines which
// sort of hook it installs itself as on mobs/chars/objs. The trigger's key is
// it's unique string identifier for lookup in the world databse. And, finally,
// its code is the python script that executes when this trigger is run.
TRIGGER_DATA  *newTrigger(void);
void        deleteTrigger(TRIGGER_DATA *trigger);
STORAGE_SET *triggerStore(TRIGGER_DATA *trigger);
TRIGGER_DATA *triggerRead(STORAGE_SET  *set);
void        triggerCopyTo(TRIGGER_DATA *from, TRIGGER_DATA *to);
TRIGGER_DATA *triggerCopy(TRIGGER_DATA *trigger);

//
// utilities for setting the trigger's fields
void triggerSetName(TRIGGER_DATA *trigger, const char *name);
void triggerSetType(TRIGGER_DATA *trigger, const char *type);
void  triggerSetKey(TRIGGER_DATA *trigger, const char *key);
void triggerSetCode(TRIGGER_DATA *trigger, const char *code);

//
// utilities for getting the trigger's fields
const char   *triggerGetName(TRIGGER_DATA *trigger);
const char   *triggerGetType(TRIGGER_DATA *trigger);
const char    *triggerGetKey(TRIGGER_DATA *trigger);
const char   *triggerGetCode(TRIGGER_DATA *trigger);
BUFFER *triggerGetCodeBuffer(TRIGGER_DATA *trigger);

//
// run a trigger
void triggerRun(TRIGGER_DATA *trigger, PyObject *dict);

//
// for getting lists of triggers installed on various things. Returns the
// trigger's key, and not the actual trigger. If an entry is removed from one
// of these lists, it must be freed afterwards.
LIST *charGetTriggers(CHAR_DATA *ch);
LIST *objGetTriggers (OBJ_DATA  *obj);
LIST *roomGetTriggers(ROOM_DATA *room);

//
// get the python form for a character, object, or room. These are persistent
// from the first time the python form is created. Before the pyform is 
// returned, it is increfed. Therefore, users must decref after they are done
// to avoid memory leaks.
PyObject    *charGetPyForm(CHAR_DATA    *ch);
PyObject    *roomGetPyForm(ROOM_DATA    *room);
PyObject     *objGetPyForm(OBJ_DATA     *obj);
PyObject  *socketGetPyForm(SOCKET_DATA  *sock);
PyObject *accountGetPyForm(ACCOUNT_DATA *acc);

//
// The same as above, but returns a borrowed reference so no incref/decref are
// needed
PyObject    *charGetPyFormBorrowed(CHAR_DATA    *ch);
PyObject    *roomGetPyFormBorrowed(ROOM_DATA    *room);
PyObject     *objGetPyFormBorrowed(OBJ_DATA     *obj);
PyObject  *socketGetPyFormBorrowed(SOCKET_DATA  *sock);
PyObject *accountGetPyFormBorrowed(ACCOUNT_DATA *acc);

//
// the generic function for parsing a function keyword list and running a 
// things's triggers from the arguments supplied. See documentation in
// trighooks.h for explanation of what me and me_type are, as well as arguemnts
// expected in args. If supplied, opts is expected to be a dict.
PyObject *py_gen_do_trigs(PyObject *self, PyObject *args, PyObject *kwds);

//
// returns a Python form of the given list
PyObject *PyList_fromList(LIST *list, void *convertor);

//
// adds a trigger to the trigger list. Makes sure it's not a duplicate copy
void triggerListAdd(LIST *list, const char *trigger);

//
// removes a trigger with the given name from the trigger list. Frees the
// name of the removed trigger as needed.
void triggerListRemove(LIST *list, const char *trigger);

//
// if OLC is installed, these functions can be used for handling the editing
// of char/obj/room trigger lists. The functions for actually editing _triggers_
// are hidden, as they should not be needed by any other modules
#ifdef MODULE_OLC2
void trigger_list_menu   (SOCKET_DATA *sock, LIST *triggers);
int  trigger_list_chooser(SOCKET_DATA *sock, LIST *triggers,const char *option);
bool trigger_list_parser (SOCKET_DATA *sock, LIST *triggers, int choice,
			  const char *arg);
#endif // MODULE_OLC2



//*****************************************************************************
// general scripting
//*****************************************************************************

//
// create different sorts of dictionaries, depending on how secure we want them
// to be. Dictionaries must be deleted (Py_DECREF) after being used.
PyObject   *restricted_script_dict(void);
PyObject *unrestricted_script_dict(void);

//
// Runs an arbitrary block of python code using the given dictionary. If the
// script has a locale (i.e. zone) associated with it (for instance, running
// a trigger or a mob proto) locale can be set. Otherwise, locale should be NULL
void run_script(PyObject *dict, const char *script, const char *locale);

//
// same as above, but returns the code object that is generated by compiling
// the string.
PyObject *run_script_forcode(PyObject *dict, const char *script, 
			     const char *locale);

//
// runs a python code object wit hthe given dictionary. If the script has a 
// locale (i.e. zone) associated with it (for instance, running code for a mob
// proto) locale can be set. Otherwise, locale should be NULL.
void run_code(PyObject *code, PyObject *dict, const char *locale);

//
// Evaluates a Python statement. If the statement has a locale (i.e. a zone)
// associated with it, locale can be set. Otherwise, locale should be NULL
PyObject *eval_script(PyObject *dict, const char *statement,const char *locale);

//
// Descriptions can have scripts embedded within them. Embedded scripts must be
// surrounded by a [ and an ]. Each script must be a single statement that will
// return a string or numeric value. To perform conditional output, it is 
// common to use the ite() (if, then, else) function, which takes 2 arguments
// and an optional third. Here would be some valid dynamic descriptions 
// (assuming that the variables I made up exist in your mud):
//   You see [me.getvar("flowers")] flowers blooming here.
//   [if ch.perception > 10]There is a large bird's nest on the east cliff.[/if]
//   [ite(ch.perception >10, "There is a large bird's nest on the east cliff.")]
//   [if ch.name=="Bob"]You are in your house.[else]You are in Bob's house.[/if]
//   [ite(ch.name=="Bob", "You are in your house.", "You are in Bob's house.")]
//   You are in [ite(ch.name == "Bob", "your", ch.name + "'s")] house.
// If the expansion has a locale (i.e. a zone) associated with it, locale can
// be set. Otherwise, locale should be NULL
void expand_dynamic_descs(BUFFER *desc, PyObject *me, CHAR_DATA *ch, 
			  const char *locale);

//
// same as expand_dynamic_descs, but takes a dictionary instead of "me" and "ch"
void expand_dynamic_descs_dict(BUFFER *desc, PyObject *dict,const char *locale);

//
// works similar to send_to_char, except allows for embedded Python statements
// like expand_dynamic_descs_dict does. If dict is NULL, perform text_to_char
// instead
void expand_to_char(CHAR_DATA *ch, const char *mssg, PyObject *dict,
		    const char *locale, bool newline);

//
// returns the locale our script is running in. NULL if no scripts are running,
// and an empty string if there is no locale for the script
const char *get_script_locale(void);

//
// returns a smart locale for a script. If there is a locale on the script
// stack, return that. Otherwise, return the locale for the character's room
const char *get_smart_locale(CHAR_DATA *ch);

//
// returns true if the last script ran without any errors
bool last_script_ok(void);

//
// This shows the script to the socket, and provides syntax highlighting
void script_display(SOCKET_DATA *sock, const char *script, bool show_line_nums);

//
// Python chokes on certain characters (e.g. \r) ... this fixes that
void format_script_buffer(BUFFER *script);

#endif // SCRIPTS_H
