//*****************************************************************************
//
// triggers.c
//
// contains all of the functions for working with triggers. This has been
// separated off from scripts.c to keep things a bit more tidy.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../storage.h"

#include "scripts.h"
#include "pyplugs.h"



//*****************************************************************************
// local datastructures, functions, and defines
//*****************************************************************************
struct trigger_data {
  char       *name; // a short description of what the trigger is intended for
  char       *type; // what type of hook does this trigger install itself as?
  char        *key; // our unique key for lookup in the world database
  BUFFER     *code; // the python script that is executed
  PyObject *pycode; // the compiled version of our python code
};



//*****************************************************************************
// implementation of triggers
//*****************************************************************************
TRIGGER_DATA  *newTrigger(void) {
  TRIGGER_DATA *data = malloc(sizeof(TRIGGER_DATA));
  data->name         = strdup("");
  data->type         = strdup("");
  data->key          = strdup("");
  data->code         = newBuffer(1);
  data->pycode       = NULL;
  return data;
}

void deleteTrigger(TRIGGER_DATA *trigger) {
  if(trigger->name) free(trigger->name);
  if(trigger->type) free(trigger->type);
  if(trigger->key)  free(trigger->key);
  deleteBuffer(trigger->code);
  Py_XDECREF(trigger->pycode);
  free(trigger);
}

STORAGE_SET *triggerStore(TRIGGER_DATA *trigger) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "name", trigger->name);
  store_string(set, "type", trigger->type);
  store_string(set, "code", bufferString(trigger->code));
  return set;
}

TRIGGER_DATA *triggerRead(STORAGE_SET *set) {
  TRIGGER_DATA *trigger = newTrigger();
  triggerSetName(trigger, read_string(set, "name"));
  triggerSetType(trigger, read_string(set, "type"));
  triggerSetCode(trigger, read_string(set, "code"));
  return trigger;
}

void triggerCopyTo(TRIGGER_DATA *from, TRIGGER_DATA *to) {
  triggerSetKey (to, triggerGetKey (from));
  triggerSetName(to, triggerGetName(from));
  triggerSetType(to, triggerGetType(from));
  triggerSetCode(to, triggerGetCode(from));
}

TRIGGER_DATA *triggerCopy(TRIGGER_DATA *trigger) {
  TRIGGER_DATA *newtrigger = newTrigger();
  triggerCopyTo(trigger, newtrigger);
  return newtrigger;
}

void triggerSetName(TRIGGER_DATA *trigger, const char *name) {
  if(trigger->name) free(trigger->name);
  trigger->name = strdupsafe(name);
}

void triggerSetType(TRIGGER_DATA *trigger, const char *type) {
  if(trigger->type) free(trigger->type);
  trigger->type = strdupsafe(type);
}

void triggerSetKey(TRIGGER_DATA *trigger, const char *key) {
  if(trigger->key) free(trigger->key);
  trigger->key = strdupsafe(key);
}

void triggerSetCode(TRIGGER_DATA *trigger, const char *code) {
  bufferClear(trigger->code);
  bufferCat(trigger->code, code);
  Py_XDECREF(trigger->pycode);
  trigger->pycode = NULL;
}

const char *triggerGetName(TRIGGER_DATA *trigger) {
  return trigger->name;
}

const char *triggerGetType(TRIGGER_DATA *trigger) {
  return trigger->type;
}

const char *triggerGetKey(TRIGGER_DATA *trigger) {
  return trigger->key;
}

const char *triggerGetCode(TRIGGER_DATA *trigger) {
  return bufferString(trigger->code);
}

BUFFER *triggerGetCodeBuffer(TRIGGER_DATA *trigger) {
  return trigger->code;
}

void triggerRun(TRIGGER_DATA *trigger, PyObject *dict) {
  // if we haven't yet run the trigger, compile the source code
  if(trigger->pycode == NULL)
    trigger->pycode = run_script_forcode(dict, bufferString(trigger->code),
					 get_key_locale(triggerGetKey(trigger)));
  // run right from the code
  else {
    run_code(trigger->pycode, dict, get_key_locale(triggerGetKey(trigger)));

    if(!last_script_ok())
      log_pyerr("Trigger %s terminated with an error:\r\n%s",
		trigger->key, bufferString(trigger->code));
  }
}
