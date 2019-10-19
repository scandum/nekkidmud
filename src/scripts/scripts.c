//*****************************************************************************
//
// scripts.c
//
// NakedMud makes extensive use of scripting. It uses scripting to generate
// objects, mobiles, and rooms when they are loaded into the game. There are
// also scripting hooks for these things (commonly referred to as triggers), 
// which allow them to be a bit more dynamic and flavorful in the game. For
// instance, greetings when someone enters a room, repsonses to questions, 
// actions when items are received.. you know... that sort of stuff. 
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../world.h"
#include "../character.h"
#include "../room.h"
#include "../exit.h"
#include "../object.h"
#include "../account.h"
#include "../socket.h"
#include "../auxiliary.h"
#include "../storage.h"
#include "../handler.h"
#include "../hooks.h"

#include "scripts.h"
#include "pyplugs.h"
#include "pyaccount.h"
#include "pychar.h"
#include "pyroom.h"
#include "pyexit.h"
#include "pyobj.h"
#include "pysocket.h"
#include "pymud.h"
#include "pymudsys.h"
#include "pyhooks.h"
#include "pyevent.h"
#include "pystorage.h"
#include "pyauxiliary.h"
#include "trighooks.h"
#include "pyolc.h"

// online editor stuff
#include "../editor/editor.h"
#include "script_editor.h"

// Python stuff
#include <compile.h>
#include <eval.h>



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../olc2/olc.h"



//*****************************************************************************
// auxiliary data
//*****************************************************************************
typedef struct {
  LIST   *triggers;
  PyObject *pyform;
} TRIGGER_AUX_DATA;

TRIGGER_AUX_DATA *newTriggerAuxData(void) {
  TRIGGER_AUX_DATA *data = malloc(sizeof(TRIGGER_AUX_DATA));
  data->triggers         = newList();
  data->pyform           = NULL;
  return data;
}

void deleteTriggerAuxData(TRIGGER_AUX_DATA *data) {
  deleteListWith(data->triggers, free);
  if(data->pyform && data->pyform->ob_refcnt > 1)
    log_string("LEAK: Memory leak (%d refcnt) on someone or something's pyform",
	       (int)data->pyform->ob_refcnt);
  Py_XDECREF(data->pyform);
  free(data);
}

void triggerAuxDataCopyTo(TRIGGER_AUX_DATA *from, TRIGGER_AUX_DATA *to) {
  deleteListWith(to->triggers, free);
  to->triggers = listCopyWith(from->triggers, strdup);
}

TRIGGER_AUX_DATA *triggerAuxDataCopy(TRIGGER_AUX_DATA *data) {
  TRIGGER_AUX_DATA *newdata = newTriggerAuxData();
  triggerAuxDataCopyTo(data, newdata);
  return newdata;
}

char *read_one_trigger(STORAGE_SET *set) {
  return strdup(read_string(set, "trigger"));
}

TRIGGER_AUX_DATA *triggerAuxDataRead(STORAGE_SET *set) {
  TRIGGER_AUX_DATA *data = malloc(sizeof(TRIGGER_AUX_DATA));
  data->triggers = gen_read_list(read_list(set, "triggers"), read_one_trigger);
  data->pyform   = NULL;
  return data;
}

STORAGE_SET *store_one_trigger(char *key) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "trigger", key);
  return set;
}

STORAGE_SET *triggerAuxDataStore(TRIGGER_AUX_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_list(set, "triggers", gen_store_list(data->triggers,store_one_trigger));
  return set;
}



//*****************************************************************************
// local datastructures, functions, and defines
//*****************************************************************************

//
// a stack that keeps track of the locale scripts are running in
LIST *locale_stack = NULL;

//
// Many thanks to Xanthros who pointed out that, if two scripts trigger
// eachother, we could get tossed into an infinite loop. He suggested a
// check for loop depth before each script is run. If we are deeper than
// some maximum depth, do not run the script.
#define MAX_LOOP_DEPTH   30
int script_loop_depth   = 0;

// a local variable used for storing whether or not the last script ran fine
bool script_ok = TRUE;

void expand_char_dynamic_descs(const char *info) {
  CHAR_DATA *me = NULL;
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &me, &ch);

  // if we're an NPC, do some special work for displaying us. We don't do 
  // dynamic descs for PCs because they will probably be describing themselves,
  // and we don't want to give them access to the scripting language.
  if(charIsNPC(me)) {
    PyObject *pyme = charGetPyForm(me);
    char   *locale = strdup(get_key_locale(charGetClass(me))); 
    expand_dynamic_descs(charGetLookBuffer(ch), pyme, ch, locale);
    Py_DECREF(pyme);
    free(locale);
  }
}

void  expand_obj_dynamic_descs(const char *info) {
  OBJ_DATA  *me = NULL;
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &me, &ch);

  PyObject *pyme = objGetPyForm(me);
  char   *locale = strdup(get_key_locale(objGetClass(me))); 
  expand_dynamic_descs(charGetLookBuffer(ch), pyme, ch, locale);
  Py_DECREF(pyme);
  free(locale);
}

void expand_room_dynamic_descs(const char *info) {
  ROOM_DATA *me = NULL;
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &me, &ch);

  PyObject *pyme = roomGetPyForm(me);
  char   *locale = strdup(get_key_locale(roomGetClass(me))); 
  expand_dynamic_descs(charGetLookBuffer(ch), pyme, ch, locale);
  Py_DECREF(pyme);
  free(locale);
}

void expand_exit_dynamic_descs(const char *info) {
  EXIT_DATA *me = NULL;
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &me, &ch);

  PyObject *pyme = newPyExit(me);
  char   *locale = strdup(get_key_locale(roomGetClass(exitGetRoom(me)))); 
  expand_dynamic_descs(charGetLookBuffer(ch), pyme, ch, locale);
  Py_DECREF(pyme);
  free(locale);
}

void finalize_scripts(void) {
  Py_Finalize();
}

void finalize_scripts_hook(const char *info) {
  finalize_scripts();
}



//*****************************************************************************
// player commands
//*****************************************************************************
//
// displays info on a script to a person
COMMAND(cmd_tstat) {
  if(!charGetSocket(ch))
    return;
  else {
    TRIGGER_DATA *trig = 
      worldGetType(gameworld, "trigger", get_fullkey_relative(arg, 
			      get_key_locale(roomGetClass(charGetRoom(ch)))));
    if(trig == NULL)
      send_to_char(ch, "No trigger exists with that key.\r\n");
    else {
      send_to_socket(charGetSocket(ch),
		     "--------------------------------------------------------------------------------\r\n"
		     "Name         : %s\r\n"
		     "Trigger type : %s\r\n"
		     "--------------------------------------------------------------------------------\r\n",
		     triggerGetName(trig), 
		     triggerGetType(trig));
      script_display(charGetSocket(ch), triggerGetCode(trig), FALSE);
    }
  }
}


//
// attach a new trigger to the given instanced object/mobile/room
COMMAND(cmd_attach) {
  TRIGGER_DATA *trig = NULL;
  char          *key = NULL;
  void          *tgt = NULL;
  int     found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, 
		 "word [to] { ch.room obj.room.inv.eq room }",
		 &key, &tgt, &found_type))
    return;

  // check to make sure our key is OK
  if((trig = worldGetType(gameworld, "trigger", 
          get_fullkey_relative(key, 
	      get_key_locale(roomGetClass(charGetRoom(ch)))))) == NULL)
    send_to_char(ch, "No trigger exists with the key, %s.\r\n", key);
  else {
    // what are we trying to attach it to?
    if(found_type == PARSE_CHAR) {
      send_to_char(ch, "Trigger %s attached to %s.\r\n", key, charGetName(tgt));
      triggerListAdd(charGetTriggers(tgt), triggerGetKey(trig));
    }
    else if(found_type == PARSE_ROOM) {
      send_to_char(ch, "Trigger %s attached to %s.\r\n", key, roomGetName(tgt));
      triggerListAdd(roomGetTriggers(tgt), triggerGetKey(trig));
    }
    else {
      send_to_char(ch, "Trigger %s attached to %s.\r\n", key, objGetName(tgt));
      triggerListAdd(objGetTriggers(tgt), triggerGetKey(trig));
    }
  }  
}


//
// detach a trigger from to the given instanced object/mobile/room
COMMAND(cmd_detach) {
  TRIGGER_DATA *trig = NULL;
  char          *key = NULL;
  void          *tgt = NULL;
  int     found_type = PARSE_NONE;

  if(!parse_args(ch, TRUE, cmd, arg, 
		 "word [from] { ch.room obj.room.inv.eq room }",
		 &key, &tgt, &found_type))
    return;

  // check to make sure our key is OK
  if((trig = worldGetType(gameworld, "trigger", 
          get_fullkey_relative(key, 
	      get_key_locale(roomGetClass(charGetRoom(ch)))))) == NULL)
    send_to_char(ch, "which trigger did you want to detach?\r\n");
  else {
    // what are we trying to detach the trigger from?
    if(found_type == PARSE_CHAR) {
      send_to_char(ch, "Trigger %s detached from %s.\r\n", key,
		   charGetName(tgt));
      triggerListRemove(charGetTriggers(tgt), triggerGetKey(trig));
    }
    else if(found_type == PARSE_ROOM) {
      send_to_char(ch, "Trigger %s detached from %s.\r\n", key,
		   roomGetName(tgt));
      triggerListRemove(roomGetTriggers(tgt), triggerGetKey(trig));
    }
    else {
      send_to_char(ch, "Trigger %s detached to %s.\r\n", key,
		   objGetName(tgt));
      triggerListRemove(objGetTriggers(tgt), triggerGetKey(trig));
    }
  }
}

const char *triggerGetListType(TRIGGER_DATA *trigger) {
  static char buf[SMALL_BUFFER];
  sprintf(buf, "%-40s %13s", triggerGetName(trigger), triggerGetType(trigger));
  return buf;
}

// this is used for the header when printing out zone trigger info
#define TRIGGER_LIST_HEADER \
"Name                                              Type"

COMMAND(cmd_tlist) {
  do_list(ch, (arg&&*arg?arg:get_key_locale(roomGetClass(charGetRoom(ch)))),
	  "trigger", TRIGGER_LIST_HEADER, triggerGetListType);
}

COMMAND(cmd_tdelete) {
  char *name = NULL;
  if(!parse_args(ch, TRUE, cmd, arg, "word", &name))
    return;
  do_delete(ch, "trigger", deleteTrigger, name);
}

COMMAND(cmd_trename) {
  char *from = NULL, *to = NULL;
  if(!parse_args(ch, TRUE, cmd, arg, "word word", &from, &to))
    return;
  do_rename(ch, "trigger", from, to);
}



//*****************************************************************************
// implementation of scripts.h - triggers portion in triggers.c
//*****************************************************************************
void init_scripts(void) {
  // create our locale stack
  locale_stack = newList();

  // initialize python
  Py_Initialize();

  // initialize all of our modules written in C
  init_PyMudSys();
  init_PyAuxiliary();
  init_PyEvent();
  init_PyStorage();
  init_PyAccount();
  init_PyChar();
  init_PyRoom();
  init_PyExit();
  init_PySocket();
  init_PyObj();
  init_PyMud();
  init_PyHooks();
  init_PyOLC();

  // initialize all of our modules written in Python
  init_pyplugs();

  // initialize the other parts to this module
  init_script_editor();
  init_trighooks();

  // so triggers can be saved to/loaded from disk
  worldAddType(gameworld, "trigger", triggerRead, triggerStore, deleteTrigger,
	       triggerSetKey);

  // deal with auxiliary data
  auxiliariesInstall("trigger_data", 
		     newAuxiliaryFuncs(AUXILIARY_TYPE_CHAR|AUXILIARY_TYPE_ROOM |
				       AUXILIARY_TYPE_OBJ|AUXILIARY_TYPE_SOCKET|
				       AUXILIARY_TYPE_ACCOUNT,
				       newTriggerAuxData,  deleteTriggerAuxData,
				       triggerAuxDataCopyTo, triggerAuxDataCopy,
				       triggerAuxDataStore,triggerAuxDataRead));

  // add in some hooks for preprocessing scripts embedded in descs
  hookAdd("preprocess_room_desc", expand_room_dynamic_descs);
  hookAdd("preprocess_char_desc", expand_char_dynamic_descs);
  hookAdd("preprocess_obj_desc",  expand_obj_dynamic_descs);
  hookAdd("preprocess_exit_desc", expand_exit_dynamic_descs);
  hookAdd("shutdown", finalize_scripts_hook);

  /*
  // add new player commands
  add_cmd("trun", NULL, cmd_scrun, "builder", FALSE);
  */
  extern COMMAND(cmd_tedit); // define the command
  add_cmd("attach",  NULL, cmd_attach, "scripter", FALSE);
  add_cmd("detach",  NULL, cmd_detach, "scripter", FALSE);
  add_cmd("tedit",   NULL, cmd_tedit,  "scripter", TRUE);
  add_cmd("tstat",   NULL, cmd_tstat,  "scripter", FALSE);
  add_cmd("tlist",   NULL, cmd_tlist,  "scripter", FALSE);
  add_cmd("tdelete", NULL, cmd_tdelete,"scripter",FALSE);
  add_cmd("trename", NULL, cmd_trename,"scripter", FALSE);
}

//
// makes a dictionary with all of the neccessary stuff in it, but without
// a __builtin__ module set
PyObject *mud_script_dict(void) {
  PyObject* dict = PyDict_New();

  // add the exit() function so people can terminate scripts
  PyObject *sys = PyImport_ImportModule("sys");
  if(sys != NULL) {
    PyObject *exit = PyDict_GetItemString(PyModule_GetDict(sys), "exit");
    if(exit != NULL) PyDict_SetItemString(dict, "exit", exit);
    if(exit != NULL) PyDict_SetItemString(dict, "end_script", exit);
    Py_DECREF(sys);
  }
  
  // merge all of the mud module contents with our current dict
  PyObject *mudmod = PyImport_ImportModule("mud");
  PyDict_Update(dict, PyModule_GetDict(mudmod));
  Py_DECREF(mudmod);
  mudmod = PyImport_ImportModule("char");
  PyDict_Update(dict, PyModule_GetDict(mudmod));
  Py_DECREF(mudmod);
  mudmod = PyImport_ImportModule("room");
  PyDict_Update(dict, PyModule_GetDict(mudmod));
  Py_DECREF(mudmod);
  mudmod = PyImport_ImportModule("obj");
  PyDict_Update(dict, PyModule_GetDict(mudmod));
  Py_DECREF(mudmod);
  mudmod = PyImport_ImportModule("event");
  PyDict_Update(dict, PyModule_GetDict(mudmod));
  Py_DECREF(mudmod);
  mudmod = PyImport_ImportModule("random");
  PyDict_SetItemString(dict, "random", mudmod);
  Py_DECREF(mudmod);

  return dict;
}

PyObject *restricted_script_dict(void) {
  // build up our basic dictionary
  PyObject *dict = mud_script_dict();
  
  // add restricted builtin modules
  PyObject *builtins = PyImport_ImportModule("__restricted_builtin__");
  if(builtins != NULL) {
    PyDict_SetItemString(dict, "__builtins__", builtins);
    Py_DECREF(builtins);
  }

  return dict;
}

PyObject *unrestricted_script_dict(void) {
  PyObject *dict = mud_script_dict();

  // add builtins
  PyObject *builtins = PyImport_ImportModule("__builtin__");
  if(builtins != NULL) {
    PyDict_SetItemString(dict, "__builtins__", builtins);
    Py_DECREF(builtins);
  }

  return dict;
}

void run_code(PyObject *code, PyObject *dict, const char *locale) {
  if(script_loop_depth >= MAX_LOOP_DEPTH) {
    // should we flag some sort of error, here?
    //***********
    // FINISH ME
    //***********

    script_ok = FALSE;
  }
  else {
    listPush(locale_stack, strdupsafe(locale));

    // try executing the code
    script_ok = TRUE;
    script_loop_depth++;
    PyObject *retval = PyEval_EvalCode((PyCodeObject *)code, dict, dict);
    script_loop_depth--;

    // did we throw an error?
    if(retval == NULL && PyErr_Occurred() != PyExc_SystemExit)
      script_ok = FALSE;

    // garbage collection
    free(listPop(locale_stack));
    Py_XDECREF(retval);
  }
}

PyObject *run_script_forcode(PyObject *dict, const char *script, 
			     const char *locale) {
  // try compiling the code
  PyObject *retval = Py_CompileString(script, "<string>", Py_file_input);

  // try running the code
  if(retval != NULL)
    run_code(retval, dict, locale);
  
  // did we end up with an error?
  if(retval == NULL || !last_script_ok())
    log_pyerr("Script terminated with an error:\r\n%s", script);

  // return our code object
  return retval;
}

void run_script(PyObject *dict, const char *script, const char *locale) {
  PyObject *code = run_script_forcode(dict, script, locale);
  Py_XDECREF(code);
}

PyObject *eval_script(PyObject *dict, const char *statement,const char *locale){
  listPush(locale_stack, strdupsafe(locale));
 
  // run the statement
  PyObject *retval = PyRun_String(statement, Py_eval_input, dict, dict);

  // did we encounter an error?
  if(retval == NULL)
    log_pyerr("eval_script terminated with an error:\r\n%s", statement);

  free(listPop(locale_stack));
  return retval;
}

//
// Tries to expand out a conditional statement. Returns whatever is expanded.
// Moves i to the proper place after the conditional has been expanded in the
// originating buffer.
BUFFER *expand_dynamic_conditional(BUFFER *desc,     PyObject *dict, 
				   PyObject *retval, const char *locale,
				   int *pos) {
  BUFFER *new_desc = newBuffer(bufferLength(desc)*2);
  BUFFER     *code = newBuffer(1); // code we have to evaluate
  int         j, i = *pos + 1;     // +1 to skip the current closing ]
  bool       match = PyObject_IsTrue(retval);

  while(TRUE) {
    // copy over all of our contents for the new desc and increment i
    while(*(bufferString(desc)+i) &&
	  !startswith(bufferString(desc)+i, "[else]") &&
	  !startswith(bufferString(desc)+i, "[elif ") &&
	  !startswith(bufferString(desc)+i, "[/if]")) {
      if(match == TRUE)
	bprintf(new_desc, "%c", *(bufferString(desc) + i));
      i++;
    }

    // did we terminate?
    if(match == TRUE || startswith(bufferString(desc)+i, "[/if]"))
      break;
    // we haven't had a match yet. Are we trying an else or elif?
    else if(startswith(bufferString(desc)+i, "[else]")) {
      match  = TRUE;
      i     += 6;
    }
    else if(startswith(bufferString(desc)+i, "[elif ")) {
      // skip the elif and spaces
      i += 6;
      while(isspace(*(bufferString(desc)+i)))
	i++;

      // find our end
      int end = next_letter_in(bufferString(desc) + i, ']');

      // make sure we have it
      if(end == -1)
	break;

      // copy everything between start and end, and format the code
      bufferClear(code);
      for(j = 0; j < end; j++)
	bprintf(code, "%c", *(bufferString(desc) + i + j));
      bufferReplace(code, "\n", "", TRUE);

      // skip i up to and beyond the ] marker
      i = i + end + 1;
      
      // evaluate the code
      PyObject *retval = eval_script(dict, bufferString(code), locale);
      match            = retval && PyObject_IsTrue(retval);

      // did we encounter an error?
      if(retval == NULL)
	break;
    }
  }

  // skip everything up to our closing [/if]
  while(*(bufferString(desc)+i) && !startswith(bufferString(desc)+i, "[/if]"))
    i++;
  if(startswith(bufferString(desc)+i, "[/if]"))
    i += 4; // put us at the closing ], not the end of the ending if block

  // garbage collection
  deleteBuffer(code);
 
  // return our expansion and move our i
  *pos = i;
  return new_desc;
}

void expand_dynamic_descs_dict(BUFFER *desc, PyObject *dict,const char *locale){
  // make our new buffer
  BUFFER *new_desc = newBuffer(bufferLength(desc)*2);
  BUFFER     *code = newBuffer(1); // for things we have to evaluate
  bool   proc_cond = FALSE; // are we processing a conditional statement?

  // copy over all of our description that is not dynamic text
  int start, end, i, j, size = bufferLength(desc);
  for(i = 0; i < size; i++) {
    // figure out when our next dynamic desc is.
    start = next_letter_in(bufferString(desc) + i, '[');

    // no more
    if(start == -1) {
      // copy the rest and skip to the end of the buffer
      bprintf(new_desc, "%s", bufferString(desc) + i);
      i = size - 1;
    }
    // we have another desc
    else {
      // copy everything up to start
      while(start > 0) {
	bprintf(new_desc, "%c", *(bufferString(desc) + i));
	start--;
	i++;
      }

      // skip the start marker
      i++;

      // find our end
      end = next_letter_in(bufferString(desc) + i, ']');

      // make sure we have it
      if(end == -1)
	break;

      // copy everything between start and end, and format the code
      bufferClear(code);
      for(j = 0; j < end; j++)
	bprintf(code, "%c", *(bufferString(desc) + i + j));
      bufferReplace(code, "\n", " ", TRUE);
      bufferReplace(code, "\r", "", TRUE);

      // skip i up to the end
      i = i + end;

      // are we trying to process a conditional statement?
      if(!strncasecmp(bufferString(code), "if ", 3)) {
	// strip out the leading if and whitespace
	char *code_copy = strdup(bufferString(code));
	char       *ptr = code_copy + 3;
	while(isspace(*ptr)) ptr++;
	
	// copy over the cleaned-up code, signal we're processing a conditional
	bufferClear(code);
	bufferCat(code, ptr);
	proc_cond = TRUE;

	// garbage collection
	free(code_copy);
      }

      // evaluate the code
      PyObject *retval = eval_script(dict, bufferString(code), locale);

      // did we encounter an error?
      if(retval == NULL)
	break;
      // are we evaluating a conditional or no?
      else if(proc_cond) {
	BUFFER *cond_retval =
	  expand_dynamic_conditional(desc, dict, retval, locale, &i);

	// if we have something to print, expand its embedded python
	if(*bufferString(cond_retval))
	  expand_dynamic_descs_dict(cond_retval, dict, locale);

	bufferCat(new_desc, bufferString(cond_retval));
	deleteBuffer(cond_retval);
	proc_cond = FALSE;
      }
      // append the output
      else if(PyString_Check(retval))
	bprintf(new_desc, "%s", PyString_AsString(retval));
      else if(PyInt_Check(retval))
	bprintf(new_desc, "%ld", PyInt_AsLong(retval));
      else if(PyFloat_Check(retval))
	bprintf(new_desc, "%lf", PyFloat_AsDouble(retval));
      // invalid return type...
      else if(retval != Py_None)
	log_string("dynamic desc had invalid evaluation: %s", 
		   bufferString(code));

      // oddly, freeing retval here corrupt memory. 
      // And not freeing it doesn't cause a memory leak. So bizarre...
      //   Py_XDECREF(retval);
    }
  }

  // copy over our contents
  bufferCopyTo(new_desc, desc);

  // garbage collection
  deleteBuffer(code);
  deleteBuffer(new_desc);
}

void expand_dynamic_descs(BUFFER *desc, PyObject *me, CHAR_DATA *ch, 
			  const char *locale) {
  // set up our dictionary
  PyObject *dict = restricted_script_dict();
  PyDict_SetItemString(dict, "me", me);
  PyDict_SetItemString(dict, "ch", charGetPyFormBorrowed(ch));

  // expand the dynamic description
  expand_dynamic_descs_dict(desc, dict, locale);

  // garbage collection
  Py_XDECREF(dict);
}

void expand_to_char(CHAR_DATA *ch, const char *mssg, PyObject *dict, 
		    const char *locale, bool newline) {
  BUFFER *buf = newBuffer(1);
  bufferCat(buf, mssg);
  if(dict != NULL) {
    // build the script dictionary
    PyObject *script_dict = restricted_script_dict();
    PyDict_Update(script_dict, dict);

    // do the expansion
    expand_dynamic_descs_dict(buf, script_dict, locale);

    // garbage collection and end
    Py_XDECREF(script_dict);
  }

  if(newline == TRUE)
    bufferCat(buf, "\r\n");
  text_to_char(ch, bufferString(buf));

  // garbage collection
  deleteBuffer(buf);
}

PyObject *py_gen_do_trigs(PyObject *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[] ={ "type","ch","obj","room","exit","cmd","arg","opts",NULL };
  char       *type = NULL;
  char        *cmd = NULL;
  char        *arg = NULL;
  PyObject   *pych = NULL;
  PyObject  *pyobj = NULL;
  PyObject *pyroom = NULL;
  PyObject *pyexit = NULL;
  PyObject *pyopts = NULL;
  LIST       *opts = NULL;
  CHAR_DATA    *ch = NULL;
  OBJ_DATA    *obj = NULL;
  ROOM_DATA  *room = NULL;
  EXIT_DATA  *exit = NULL;
  void         *me = NULL;
  int      me_type = -1;
  bool        fail = FALSE;

  // first, parse all of our arguments
  if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|OOOOssO", kwlist, 
				  &type, &pych, &pyobj, &pyroom, &pyexit,
				  &cmd, &arg, &pyopts)) {
    PyErr_Format(PyExc_TypeError, "do_trigs supplied invalid arguments.");
    return NULL;
  }

  // make sure we exist, and are of a valid type
  if(PyChar_Check(self)) {
    me = PyChar_AsChar(self);
    me_type = TRIGVAR_CHAR;
  }
  else if(PyObj_Check(self)) {
    me = PyObj_AsObj(self);
    me_type = TRIGVAR_OBJ;
  }
  else if(PyRoom_Check(self)) {
    me = PyRoom_AsRoom(self);
    me_type = TRIGVAR_ROOM;
  }

  // did we find a character?
  if(me == NULL) {
    PyErr_Format(PyExc_TypeError,"do_trigs owner does not exist.");
    return NULL;
  }

  // make sure ch is of the specified type
  if(pych!=NULL && (!PyChar_Check(pych) || (ch=PyChar_AsChar(pych)) == NULL)){
    PyErr_Format(PyExc_TypeError,"do_trigs expects ch to be character.");
    return NULL;
  }

  // make sure obj is of the specified type
  if(pyobj!=NULL && (!PyObj_Check(pyobj) || (obj=PyObj_AsObj(pyobj)) == NULL)){
    PyErr_Format(PyExc_TypeError,"do_trigs expects obj to be object.");
    return NULL;
  }

  // make sure room is of the specified type
  if(pyroom!=NULL&&(!PyRoom_Check(pyroom)||(room=PyRoom_AsRoom(pyroom))==NULL)){
    PyErr_Format(PyExc_TypeError,"do_trigs expects room to be room.");
    return NULL;
  }

  // make sure exit is of the specified type
  if(pyexit!=NULL&&(!PyExit_Check(pyexit)||(exit=PyExit_AsExit(pyexit))==NULL)){
    PyErr_Format(PyExc_TypeError,"do_trigs expects exit to be an exit.");
    return NULL;
  }

  // parse opts
  if(pyopts != NULL && !PyDict_Check(pyopts)) {
    PyErr_Format(PyExc_TypeError,"do_trigs expects opts to be a dict.");
    return NULL;
  }
  else if(pyopts != NULL) {
    PyObject  *pairs = PyDict_Items(pyopts);
    int            i = 0;
    opts             = newList();
    // go through each pair and add it to the pyopts list
    for(; i < PyList_Size(pairs); i++) {
      PyObject  *pair = PyList_GetItem(pairs, i);
      PyObject *pykey = PyTuple_GetItem(pair, 0);
      PyObject *pyval = PyTuple_GetItem(pair, 1);
      OPT_VAR    *opt = NULL;
      char       *key = NULL;

      // make sure the key is a string
      if(PyString_Check(pykey))
	key = PyString_AsString(pykey);
      else {
	PyErr_Format(PyExc_TypeError, "do_trigs opt keys must be strings.");
	fail = TRUE;
	break;
      }

      // check to make sure the val is a valid type
      if(PyChar_Check(pyval)) {
	CHAR_DATA *val = PyChar_AsChar(pyval);
	if(val != NULL)
	  opt = newOptVar(key, val, TRIGVAR_CHAR);
	else {
	  PyErr_Format(PyExc_TypeError,"%s opt provided nonexistent char.",key);
	  fail = TRUE;
	  break;
	}
      }
      else if(PyObj_Check(pyval)) {
	OBJ_DATA *val = PyObj_AsObj(pyval);
	if(val != NULL)
	  opt = newOptVar(key, val, TRIGVAR_OBJ);
	else {
	  PyErr_Format(PyExc_TypeError,"%s opt provided nonexistent obj.", key);
	  fail = TRUE;
	  break;
	}
      }
      else if(PyRoom_Check(pyval)) {
	ROOM_DATA *val = PyRoom_AsRoom(pyval);
	if(val != NULL)
	  opt = newOptVar(key, val, TRIGVAR_ROOM);
	else {
	  PyErr_Format(PyExc_TypeError,"%s opt provided nonexistent room.",key);
	  fail = TRUE;
	  break;
	}
      }
      else {
	PyErr_Format(PyExc_TypeError,"%s opt provided invalid value.",key);
	fail = TRUE;
	break;
      }

      // append the opt to the opt list
      listPut(opts, opt);
    }
    Py_DECREF(pairs);
  }

  // did everything succeed?
  if(fail == FALSE)
    gen_do_trigs(me,me_type,type,ch,obj,room,exit,cmd,arg,opts);

  // garbage collection
  if(opts != NULL)
    deleteListWith(opts, deleteOptVar);

  if(fail == TRUE)
    return NULL;
  return Py_BuildValue("");
}

const char *get_script_locale(void) {
  return listHead(locale_stack);
}

const char *get_smart_locale(CHAR_DATA *ch) {
  const char *locale = get_script_locale();
  if(locale == NULL && charGetRoom(ch) != NULL)
    locale = get_key_locale(roomGetClass(charGetRoom(ch)));
  return locale;
}

bool last_script_ok(void) {
  return script_ok;
}

void format_script_buffer(BUFFER *script) {
  bufferReplace(script, "\r", "", TRUE);
}

LIST *charGetTriggers(CHAR_DATA *ch) {
  TRIGGER_AUX_DATA *data = charGetAuxiliaryData(ch, "trigger_data");
  return data->triggers;
}

LIST *objGetTriggers (OBJ_DATA  *obj) {
  TRIGGER_AUX_DATA *data = objGetAuxiliaryData(obj, "trigger_data");
  return data->triggers;
}

LIST *roomGetTriggers(ROOM_DATA *room) {
  TRIGGER_AUX_DATA *data = roomGetAuxiliaryData(room, "trigger_data");
  return data->triggers;
}

PyObject *charGetPyFormBorrowed(CHAR_DATA *ch) {
  TRIGGER_AUX_DATA *data = charGetAuxiliaryData(ch, "trigger_data");
  if(data->pyform == NULL)
    data->pyform = newPyChar(ch);
  return data->pyform;
}

PyObject *objGetPyFormBorrowed(OBJ_DATA  *obj) {
  TRIGGER_AUX_DATA *data = objGetAuxiliaryData(obj, "trigger_data");
  if(data->pyform == NULL)
    data->pyform = newPyObj(obj);
  return data->pyform;
}

PyObject *roomGetPyFormBorrowed(ROOM_DATA *room) {
  TRIGGER_AUX_DATA *data = roomGetAuxiliaryData(room, "trigger_data");
  if(data->pyform == NULL)
    data->pyform = newPyRoom(room);
  return data->pyform;
}

PyObject *accountGetPyFormBorrowed(ACCOUNT_DATA *acc) {
  TRIGGER_AUX_DATA *data = accountGetAuxiliaryData(acc, "trigger_data");
  if(data->pyform == NULL)
    data->pyform = newPyAccount(acc);
  return data->pyform;
}

PyObject *socketGetPyFormBorrowed(SOCKET_DATA *sock) {
  TRIGGER_AUX_DATA *data = socketGetAuxiliaryData(sock, "trigger_data");
  if(data->pyform == NULL)
    data->pyform = newPySocket(sock);
  return data->pyform;
}

PyObject *charGetPyForm(CHAR_DATA *ch) {
  PyObject *pyform = charGetPyFormBorrowed(ch);
  Py_INCREF(pyform);
  return pyform;
}

PyObject *objGetPyForm(OBJ_DATA *obj) {
  PyObject *pyform = objGetPyFormBorrowed(obj);
  Py_INCREF(pyform);
  return pyform; 
}

PyObject *roomGetPyForm(ROOM_DATA *room) {
  PyObject *pyform = roomGetPyFormBorrowed(room);
  Py_INCREF(pyform);
  return pyform;
}

PyObject *accountGetPyForm(ACCOUNT_DATA *acc) {
  PyObject *pyform = accountGetPyFormBorrowed(acc);
  Py_INCREF(pyform);
  return pyform;
}

PyObject *socketGetPyForm(SOCKET_DATA *sock) {
  PyObject *pyform = socketGetPyFormBorrowed(sock);
  Py_INCREF(pyform);
  return pyform;
}

PyObject *PyList_fromList(LIST *list, void *convertor) {
  PyObject *pylist = PyList_New(0);
  PyObject *(*conv_func)(void *) = convertor; 
  LIST_ITERATOR *list_i = newListIterator(list);
  void            *elem = NULL;
  ITERATE_LIST(elem, list_i) {
    PyObject *pyelem = conv_func(elem);
    PyList_Append(pylist, pyelem);
    Py_DECREF(pyelem);
  } deleteListIterator(list_i);
  return pylist;
}

void triggerListAdd(LIST *list, const char *trigger) {
  if(!listGetWith(list, trigger, strcasecmp))
    listPut(list, strdup(trigger));
}

void triggerListRemove(LIST *list, const char *trigger) {
  char *val = listRemoveWith(list, trigger, strcasecmp);
  if(val) free(val);
}


//
// statements we need to highlight when showing a script
const char *control_table[] = {
  "import",
  "return",
  "except",
  "while",
  "from",
  "elif",
  "else",
  "pass",
  "try",
  "def",
  "for",
  "if",
  "in",
  "is",
  "and",
  "or",
  "not",
  "break",
  "continue",
  NULL
};


//
// returns which control string we found. returns -1 if none were found
int check_for_control(const char *ptr, int i) {
  int syn_i;
  for(syn_i = 0; control_table[syn_i] != NULL; syn_i++) {
    int len = strlen(control_table[syn_i]);
    // not enough characters for it to exist
    if(i - len + 1 < 0)
      continue;
    // we found it might have found it. Check to make
    // sure that we are surrounded by spaces or colons
    if(!strncasecmp(ptr+i-len+1, control_table[syn_i], len)) {
      // check the left side first
      if(!(i - len < 0 || isspace(ptr[i-len]) || ptr[i-len] == ':'))
	continue;
      //  and now the right side
      if(!(ptr+i+1 == '\0' || isspace(ptr[i+1]) || ptr[i+1] == ':'))
	continue;

      return syn_i;
    }
  }

  // didn't find any
  return -1;
}

void script_display(SOCKET_DATA *sock, const char *script, bool show_line_nums){
  const char *ptr = script;//buffer_string(sock->text_editor);
  char line[SMALL_BUFFER] = "\0";
  int  line_num = 1;
  int  line_i = 0, i = 0;
  bool in_line_comment = FALSE; // are we displaying a comment?
  bool in_digit = FALSE;        // are we displaying a digit?
  bool in_string  = FALSE;      // how about a string?
  char string_type = '"';       // what kinda string marker is it? ' or " ?
  int  syn_to_color = -1;       // if we're coloring flow control, which one?

  for(i = 0; ptr[i] != '\0'; i++) {
    // take off the color for digits
    if(in_digit && !isdigit(ptr[i])) {
      sprintf(line+line_i, "{n");
      line_i += 2;
      in_digit = FALSE;
    } // NO ELSE ... we might need to color something else

    // transfer over the character
    line[line_i] = ptr[i];

    // if the character is a #, color the comment red
    if(ptr[i] == '#') {
      sprintf(line+line_i, "{r#");
      line_i += 3;
      in_line_comment = TRUE;
    }

    // we've found a digit that we have to color in
    else if(isdigit(ptr[i]) && !in_digit && !in_line_comment && !in_string) {
      sprintf(line+line_i, "{y%c", ptr[i]);
      line_i += 3;
      in_digit = TRUE;
    }

    // if we've found a string marker, color/uncolor it
    else if((ptr[i] == '"' || ptr[i] == '\'') && !in_line_comment &&
	    // if we're already coloring a string and the marker
	    // types don't match up, then don't worry about it
	    !(in_string && string_type != ptr[i])) {

      if(in_string && ptr[i] == string_type)
	sprintf(line+line_i, "\%c{n", string_type);
      else
	sprintf(line+line_i, "{w%c", ptr[i]);
      
      line_i += 3;
      in_string = (in_string + 1) % 2;
      string_type = ptr[i];
    }

    // we've hit a new line
    else if(ptr[i] == '\n') {

	// do we need to show line numbers
	char line_num_info[20];
	if(show_line_nums)
	  sprintf(line_num_info, "{c%2d]  ", line_num);
	else
	  *line_num_info = '\0';

	line[line_i] = '\0';
	send_to_socket(sock, "%s{n%s{n\r\n", line_num_info, line);
	*line = '\0';
	line_i = 0;
	line_num++;
	in_line_comment = in_string = FALSE; // reset on newline
    }

    // checking while, for, if, else, elif, etc...
    // this is kinda tricky. We have to backtrack and check some stuff
    else if(!(in_line_comment || in_digit || in_string) &&
	    (syn_to_color = check_for_control(ptr, i)) != -1) {
      sprintf(line+line_i-strlen(control_table[syn_to_color])+1,
	      "{p%s{n", control_table[syn_to_color]);
      line_i += 5; // the two markers for the color, and one for new character
    }

    // didn't find anything of interest
    else
	line_i++;
  }

  line[line_i] = '\0';

  // send the last line
  if(*line) {
    if(show_line_nums)
      send_to_socket(sock, "{c%2d]{n  %s{n\r\n", line_num, line);
    else
      send_to_socket(sock, "%s{n\r\n", line);
  }

  // and kill any color that is leaking
  //  send_to_socket(sock, "{n");

  // we don't end in a newline, but we have code
  if(line_num != 1 && ptr[strlen(ptr)-1] != '\n')
    send_to_socket(sock, "{RBuffer does not end in newline!{n\r\n");
}
