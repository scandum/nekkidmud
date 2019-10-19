//*****************************************************************************
//
// alias.c
//
// Aliases allow a player to set up one command as another (e.g. "eat bread"
// as "food").
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../auxiliary.h"
#include "../storage.h"
#include "../character.h"
#include "../socket.h"
#include "../action.h"
#include "../hooks.h"

#include "alias.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pychar.h"



//*****************************************************************************
// auxiliary data
//*****************************************************************************
typedef struct alias_aux_data {
  HASHTABLE *aliases;
  int    alias_queue;
} ALIAS_AUX_DATA;


ALIAS_AUX_DATA *
newAliasAuxData() {
  ALIAS_AUX_DATA *data = calloc(1, sizeof(ALIAS_AUX_DATA));
  //
  // Hashtables can take up lots of storage space. Because of this, let's
  // not create any tables until it's actually needed. This will cut down
  // on lots of memory usage w.r.t. NPCs who do not use aliases
  //  data->aliases        = newHashtable();
  //
  return data;
}


void
deleteAliasAuxData(ALIAS_AUX_DATA *data) {
  if(data->aliases) {
    HASH_ITERATOR *hash_i = newHashIterator(data->aliases);
    const char     *alias = NULL;
    char             *cmd = NULL;

    ITERATE_HASH(alias, cmd, hash_i) 
      free(cmd);
    deleteHashIterator(hash_i);
    deleteHashtable(data->aliases);
  }
  free(data);
}


void
aliasAuxDataCopyTo(ALIAS_AUX_DATA *from, ALIAS_AUX_DATA *to) {
  // aliases are for personal use, and it is doubtful we would
  // ever have a motivation for copying them over. Thus, we shall
  // not do any copying here.
}


ALIAS_AUX_DATA *
aliasAuxDataCopy(ALIAS_AUX_DATA *data) {
  // as with aliasAuxDataCopyTo, there seems to be little reason
  // why we would ever want to copy aliases. They are personal,
  // and have no reason for being copied. Thus, let us just return
  // a new datastructure.
  return newAliasAuxData();
}


STORAGE_SET *aliasAuxDataStore(ALIAS_AUX_DATA *data) {
  // if we have no hashtable, return an empty set
  if(data->aliases == NULL || hashSize(data->aliases) == 0)
    return new_storage_set();

  STORAGE_SET *set       = new_storage_set();
  STORAGE_SET_LIST *list = new_storage_list();
  HASH_ITERATOR  *hash_i = newHashIterator(data->aliases);
  const char       *name = NULL;
  const char        *cmd = NULL;

  store_list(set, "aliases", list);
  ITERATE_HASH(name, cmd, hash_i) {
    STORAGE_SET *alias_set = new_storage_set();
    store_string(alias_set, "key", name);
    store_string(alias_set, "val", hashIteratorCurrentVal(hash_i));
    storage_list_put(list, alias_set);
  }
  deleteHashIterator(hash_i);
  return set;
}


ALIAS_AUX_DATA *aliasAuxDataRead(STORAGE_SET *set) {
  // if the file contains no alias data, then don't load any
  if(!storage_contains(set, "aliases"))
    return newAliasAuxData();

  ALIAS_AUX_DATA *data   = newAliasAuxData();
  data->aliases          = newHashtable();
  STORAGE_SET_LIST *list = read_list(set, "aliases");
  STORAGE_SET *var       = NULL;

  while( (var = storage_list_next(list)) != NULL)
    hashPut(data->aliases, read_string(var, "key"), 
	    strdup(read_string(var, "val")));
  return data;
}



//*****************************************************************************
// functions for interacting with character aliases
//*****************************************************************************
LIST *charGetAliases(CHAR_DATA *ch) {
  ALIAS_AUX_DATA *data = charGetAuxiliaryData(ch, "alias_aux_data");
  if(data->aliases == NULL)
    return newList();
  return hashCollect(data->aliases);
}

const char *charGetAlias(CHAR_DATA *ch, const char *alias) {
  ALIAS_AUX_DATA *data = charGetAuxiliaryData(ch, "alias_aux_data");
  if(data->aliases == NULL)
    return NULL;
  return hashGet(data->aliases, alias);
}

void charClearAliases(CHAR_DATA *ch) {
  LIST *aliases = charGetAliases(ch);
  LIST_ITERATOR *alias_i = newListIterator(aliases);
  const char      *alias = NULL;
  ITERATE_LIST(alias, alias_i) {
    charSetAlias(ch, alias, NULL);
  } deleteListIterator(alias_i);
  deleteListWith(aliases, free);
}

void charSetAlias(CHAR_DATA *ch, const char *alias, const char *cmd){
  ALIAS_AUX_DATA *data = charGetAuxiliaryData(ch, "alias_aux_data");
  // if our alias table is NULL, create a new one
  if(data->aliases == NULL)
    data->aliases = newHashtable();

  // pull out the last one
  char *oldcmd = hashRemove(data->aliases, alias);

  if(oldcmd != NULL)
    free(oldcmd);

  // put in the new one if it exists
  if(cmd && *cmd) {
    hashPut(data->aliases, alias, strdup(cmd));
  }
}

int charGetAliasesQueued(CHAR_DATA *ch) {
  ALIAS_AUX_DATA *data = charGetAuxiliaryData(ch, "alias_aux_data");
  return data->alias_queue;
}

void charSetAliasesQueued(CHAR_DATA *ch, int amnt) {
  ALIAS_AUX_DATA *data = charGetAuxiliaryData(ch, "alias_aux_data");
  data->alias_queue = amnt;
}

BUFFER *expand_alias(CHAR_DATA *ch, const char *alias, const char *arg) {
  static int func_depth      = 0;
  static int MAX_ALIAS_DEPTH = 10;
  BUFFER *cmd = newBuffer(SMALL_BUFFER);
  func_depth++;

  BUFFER *filled_alias = newBuffer(SMALL_BUFFER);
  bufferCat(filled_alias, alias);
  // now, replace all of our parameters 
  int i;
  for(i = 1; i < 10; i++) {
    char param[3];
    char one_arg[SMALL_BUFFER];
    sprintf(param, "$%d", i);
    arg_num(arg, one_arg, i);
    bufferReplace(filled_alias, param, one_arg, TRUE);
  }

  // then we replace the wildcard
  bufferReplace(filled_alias, "$*", arg, TRUE);
  alias = bufferString(filled_alias);

  if(func_depth <= MAX_ALIAS_DEPTH) {
    for(i = 0; alias[i] != '\0'; i++) {
      // do we have an embedded alias or not?
      if(alias[i] != '[')
	bprintf(cmd, "%c", alias[i]);

      else {
	// figure out the start and end of the embedded alias
	int start = i;
	int depth = 1;
	for(i++; alias[i] != '\0' && depth > 0; i++) {
	  if(alias[i] == '[')
	    depth++;
	  else if(alias[i] == ']')
	    depth--;
	}

	// only cat something if we closed the alias off
	if(depth == 0) {
	  // make a copy of it, minus the opening and closing braces
	  char *newstring = strdup(alias+start+1);
	  newstring[i-start-2] = '\0';
	  char newcmd[SMALL_BUFFER];
	  char *newarg = one_arg(newstring, newcmd);
	  const char *format = charGetAlias(ch, newcmd);
	  
	  // do we have a format? if so, expand the new alias and cat it in
	  if(format != NULL) {
	    BUFFER *newbuf = expand_alias(ch, format, newarg);
	    bufferCat(cmd, bufferString(newbuf));
	    deleteBuffer(newbuf);
	  }

	  // clean up our mess
	  free(newstring);
	}

	// because we'll be incrementing on the next go around the loop
	i--;
      }
    }
  }

  func_depth--;
  deleteBuffer(filled_alias);
  return cmd;
}



//*****************************************************************************
// character commands
//*****************************************************************************

//
// Set or delete an alias. If no argument is supplied, all aliases are listed.
COMMAND(cmd_alias) {
  // list off all of the aliases
  if(!arg || !*arg) {
    ALIAS_AUX_DATA *data = charGetAuxiliaryData(ch, "alias_aux_data");
    send_to_char(ch, "Current aliases:\r\n");
    if(data->aliases == NULL || hashSize(data->aliases) == 0)
      send_to_char(ch, "  none\r\n");
    else {
      LIST             *keys = hashCollect(data->aliases);
      listSortWith(keys, strcasecmp);
      LIST_ITERATOR   *key_i = newListIterator(keys);
      const char        *key = NULL;

      ITERATE_LIST(key, key_i) {
	send_to_char(ch, "  %-20s %s\r\n", key, (char *)hashGet(data->aliases, key));
      } deleteListIterator(key_i);
      deleteListWith(keys, free);
    }
  }
  else {
    char alias[SMALL_BUFFER];
    arg = one_arg(arg, alias);

    // try to delete an alias
    if(!arg || !*arg) {
      const char *curr_cmd = charGetAlias(ch, alias);
      if(!curr_cmd)
	send_to_char(ch, "You do not have such an alias.\r\n");
      else {
	charSetAlias(ch, alias, arg);
      }
    }

    // try to set an alias
    else {
      charSetAlias(ch, alias, arg);
      send_to_char(ch, "Alias set.\r\n");
    }
  }
}



//*****************************************************************************
// Python extensions
//*****************************************************************************
PyObject *PyChar_GetAlias(PyObject *self, PyObject *args) {
  char   *alias = NULL;
  CHAR_DATA *ch = NULL;
  if(!PyArg_ParseTuple(args, "s", &alias)) {
    PyErr_Format(PyExc_TypeError, "aliases must be string names.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Character %d does not exist.",
		 PyChar_AsUid(self));
    return NULL;
  }

  return Py_BuildValue("z", charGetAlias(ch, alias));
}

PyObject *PyChar_SetAlias(PyObject *self, PyObject *args) {
  char     *cmd = NULL;
  char   *alias = NULL;
  CHAR_DATA *ch = NULL;
  if(!PyArg_ParseTuple(args, "sz", &alias, &cmd)) {
    PyErr_Format(PyExc_TypeError, "aliases and commands must be string names.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Character %d does not exist.",
		 PyChar_AsUid(self));
    return NULL;
  }

  charSetAlias(ch, alias, cmd);
  return Py_BuildValue("i", 1);
}

PyObject *PyChar_GetAliases(PyObject *self, void *closure) {
  CHAR_DATA *ch = NULL;
  if((ch = PyChar_AsChar(self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Character %d does not exist.",
		 PyChar_AsUid(self));
    return NULL;
  }

  PyObject       *pylist = PyList_New(0);
  LIST             *list = charGetAliases(ch);
  LIST_ITERATOR *alias_i = newListIterator(list);
  const char      *alias = NULL;
  ITERATE_LIST(alias, alias_i) {
    PyObject *str = Py_BuildValue("s", alias);
    PyList_Append(pylist, str);
    Py_DECREF(str);
  } deleteListIterator(alias_i);
  deleteListWith(list, free);
  return pylist;
}



//*****************************************************************************
// implementation of alias.h
//*****************************************************************************
void init_aliases() {
  // install aliases on the character datastructure
  auxiliariesInstall("alias_aux_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_CHAR,
				       newAliasAuxData, deleteAliasAuxData,
				       aliasAuxDataCopyTo, aliasAuxDataCopy,
				       aliasAuxDataStore, aliasAuxDataRead));

  // allow people to view their aliases
  add_cmd("alias", NULL, cmd_alias, "player", TRUE);

  // Python extensions
  PyChar_addMethod("get_alias", PyChar_GetAlias,   METH_VARARGS, 
    "get_alias(name)\n"
    "\n"
    "Return character's alias by the specified name, or None.");
  PyChar_addMethod("set_alias", PyChar_SetAlias,   METH_VARARGS, 
    "set_alias(name, value)\n"
    "\n"
    "Set a character's alias. Value must be a string.");
  PyChar_addGetSetter("aliases",PyChar_GetAliases, NULL, 
    "A list of all aliases the character currently has defined. Immutable.");
}


bool try_alias(CHAR_DATA *ch, char *command, char *arg) {
  // is this command from an alias that executes multi commands?
  // if it is, don't let it trigger any further aliases, or else we might
  // get stuck in an infinite loop
  bool    aliases_ok = TRUE;
  int aliases_queued = charGetAliasesQueued(ch);
  if(aliases_queued > 0) {
    // this command came from an alias. ergo, there is going to be no echo
    // of the command. send the socket a newline so we don't print on the
    // person's prompt
    send_to_char(ch, "\r\n");
    aliases_ok = FALSE;
  }

  // make sure it didn't come from another alias
  if(!aliases_ok)
    return FALSE;
  else {
    const char *alias = charGetAlias(ch, command);
    // see if the alias exists
    if(alias == NULL)
      return FALSE;
    else {
      // break the buffer contents up into multiple commands, if there are any
      BUFFER     *buf = expand_alias(ch, alias, arg);
      LIST      *cmds = parse_strings(bufferString(buf), ';');
      char     *first = listPop(cmds);
      
      // queue all of our commands after the first onto the command list
      if(charGetSocket(ch) && listSize(cmds) > 0) {
	LIST_ITERATOR *cmd_i = newListIterator(cmds);
	char            *cmd = NULL;
	ITERATE_LIST(cmd, cmd_i) {
	  socketQueueCommand(charGetSocket(ch), cmd);
	} deleteListIterator(cmd_i);
	charSetAliasesQueued(ch, listSize(cmds) + 1);
      }
      if(first != NULL)
	do_cmd(ch, first, FALSE);

      deleteListWith(cmds, free);
      if(first) free(first);
      deleteBuffer(buf);
      return TRUE;
    }
  }
}
