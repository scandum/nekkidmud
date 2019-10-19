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

#include "../mud.h"
#include "../utils.h"
#include "../character.h"
#include "../room.h"
#include "../object.h"
#include "../storage.h"
#include "../auxiliary.h"

#include "dyn_vars.h"



//*****************************************************************************
//
// local functions and datastructurs
//
//*****************************************************************************

// used in storage_sets to keep track of what kind of data we're saving
const char *dyn_var_types[] = {
  "string",
  "int",
  "long",
  "double"
};


typedef struct dyn_var {
  char *str_val;
  int   type;
} DYN_VAR;


DYN_VAR *newDynVarString(const char *str) {
  DYN_VAR *data = malloc(sizeof(DYN_VAR));
  data->str_val = strdupsafe(str);
  data->type    = DYN_VAR_STRING;
  return data;
}

DYN_VAR *newDynVarInt(int val) {
  DYN_VAR  *data = malloc(sizeof(DYN_VAR));
  char str_val[20]; sprintf(str_val, "%d", val);
  data->str_val  = strdup(str_val);
  data->type     = DYN_VAR_INT;
  return data;
}

DYN_VAR *newDynVarLong(long val) {
  DYN_VAR *data = malloc(sizeof(DYN_VAR));
  char str_val[20]; sprintf(str_val, "%ld", val);
  data->str_val  = strdup(str_val);
  data->type     = DYN_VAR_LONG;
  return data;
}

DYN_VAR *newDynVarDouble(double val) {
  DYN_VAR  *data = malloc(sizeof(DYN_VAR));
  char str_val[20]; sprintf(str_val, "%lf", val);
  data->str_val  = strdup(str_val);
  data->type     = DYN_VAR_DOUBLE;
  return data;
}

void deleteDynVar(DYN_VAR *data) {
  if(data->str_val) free(data->str_val);
  free(data);
}

DYN_VAR *dynVarCopy(DYN_VAR *data) {
  DYN_VAR *new_data = malloc(sizeof(DYN_VAR));
  new_data->str_val = strdup(data->str_val);
  new_data->type    = data->type;
  return new_data;
}

//
// delete a hashtable of vars
//
void deleteDynVarTable(HASHTABLE *table) {
  HASH_ITERATOR *hash_i = newHashIterator(table);
  const char       *key = NULL;
  DYN_VAR          *val = NULL;
  
  ITERATE_HASH(key, val, hash_i) {
    deleteDynVar(val);
  } deleteHashIterator(hash_i);
  deleteHashtable(table);
}



//*****************************************************************************
//
// dyn_var auxiliary data
//
//*****************************************************************************
typedef struct dyn_var_aux_data {
  HASHTABLE *dyn_vars;
} DYN_VAR_AUX_DATA;


DYN_VAR_AUX_DATA *
newDynVarAuxData() {
  DYN_VAR_AUX_DATA *data = malloc(sizeof(DYN_VAR_AUX_DATA));
  // Hashtables can take up lots of storage space. Because of this, let's
  // not create any tables until it's actually needed. This will cut down
  // on lots of memory usage w.r.t. NPCs who do not use character variables
  //  data->dyn_vars        = newHashtable();
  data->dyn_vars         = NULL;
  return data;
}


void
deleteDynVarAuxData(DYN_VAR_AUX_DATA *data) {
  if(data->dyn_vars)
    deleteDynVarTable(data->dyn_vars);
  free(data);
}


void
dynVarAuxDataCopyTo(DYN_VAR_AUX_DATA *from, DYN_VAR_AUX_DATA *to) {
  int from_size = (from->dyn_vars ? hashSize(from->dyn_vars) : 0);
  int to_size   = (to->dyn_vars   ? hashSize(to->dyn_vars)   : 0);

  // clear out our current data
  if(to_size > 0) {
    deleteDynVarTable(to->dyn_vars);
    to->dyn_vars = NULL;
  }

  // check to see if the "from" table exists
  if(from_size > 0) {
    // make sure the "to" table exists
    if(to->dyn_vars == NULL)
      to->dyn_vars = newHashtable();

    // copy everything over
    HASH_ITERATOR *from_i = newHashIterator(from->dyn_vars);
    const char       *key = NULL;
    DYN_VAR         *val = NULL;

    ITERATE_HASH(key, val, from_i)
      hashPut(to->dyn_vars, key, dynVarCopy(val));
    deleteHashIterator(from_i);
  }
}


DYN_VAR_AUX_DATA *
dynVarAuxDataCopy(DYN_VAR_AUX_DATA *data) {
  DYN_VAR_AUX_DATA *new_data = newDynVarAuxData();
  dynVarAuxDataCopyTo(data, new_data);
  return new_data;
}


STORAGE_SET *dynVarAuxDataStore(DYN_VAR_AUX_DATA *data) {
  // first, check if the table even exists
  if(data->dyn_vars == NULL || hashSize(data->dyn_vars) == 0)
    return new_storage_set();

  STORAGE_SET       *set = new_storage_set();
  HASH_ITERATOR  *hash_i = newHashIterator(data->dyn_vars);
  STORAGE_SET_LIST *list = new_storage_list();
  const char        *key = NULL;
  DYN_VAR          *val = NULL;

  store_list(set, "variables", list);
  // iterate across all the entries and add them
  ITERATE_HASH(key, val, hash_i) {
    STORAGE_SET *var_set = new_storage_set();
    store_string(var_set, "key",  key);
    store_string(var_set, "val",  val->str_val);
    store_string(var_set, "type", dyn_var_types[val->type]);
    storage_list_put(list, var_set);
  }
  deleteHashIterator(hash_i);
  return set;
}


HASHTABLE *variableRead(STORAGE_SET *set) {
  HASHTABLE *table       = newHashtable();
  STORAGE_SET_LIST *list = read_list(set, "list");
  STORAGE_SET *var       = NULL;

  while( (var = storage_list_next(list)) != NULL)
    hashPut(table, read_string(var, "key"), (void *)read_int(var, "val"));
  return table;
}


DYN_VAR_AUX_DATA *dynVarAuxDataRead(STORAGE_SET *set) {
  // if the set doesn't contain any entries, don't bother trying to parse
  if(!storage_contains(set, "variables"))
    return newDynVarAuxData();

  DYN_VAR_AUX_DATA *data = newDynVarAuxData();
  STORAGE_SET_LIST  *list = read_list(set, "variables");
  STORAGE_SET    *var_set = NULL;
  data->dyn_vars         = newHashtable();
  
  while( (var_set = storage_list_next(list)) != NULL) {
    const char *var_type = read_string(var_set, "type");
    if(!strcasecmp(var_type, "int"))
      hashPut(data->dyn_vars, read_string(var_set, "key"),
	      newDynVarInt(read_int(var_set, "val")));
    else if(!strcasecmp(var_type, "long"))
      hashPut(data->dyn_vars, read_string(var_set, "key"),
	      newDynVarLong(read_long(var_set, "val")));
    else if(!strcasecmp(var_type, "double"))
      hashPut(data->dyn_vars, read_string(var_set, "key"),
	      newDynVarDouble(read_double(var_set, "val")));
    else if(!strcasecmp(var_type, "string"))
      hashPut(data->dyn_vars, read_string(var_set, "key"),
	      newDynVarString(read_string(var_set, "val")));
    else
      log_string("ERROR: Tried to read unknown dyn_var type, %s.", var_type);
  }
    
  return data;
}


void init_dyn_vars() {
  // install dyn vars on the character datastructure
  auxiliariesInstall("dyn_var_aux_data",
		     newAuxiliaryFuncs(AUXILIARY_TYPE_CHAR |
				       AUXILIARY_TYPE_ROOM |
				       AUXILIARY_TYPE_OBJ,
				       newDynVarAuxData, deleteDynVarAuxData,
				       dynVarAuxDataCopyTo, dynVarAuxDataCopy,
				       dynVarAuxDataStore,dynVarAuxDataRead));
}




//*****************************************************************************
//
// functions for interacting with dyn_vars
//
//*****************************************************************************
int dynGetVarType(DYN_VAR_AUX_DATA *data, const char *key) {
  DYN_VAR *var = (data->dyn_vars ? hashGet(data->dyn_vars, key) : NULL);
  return (var ? var->type : DYN_VAR_INT);
}  

int dynGetInt(DYN_VAR_AUX_DATA *data, const char *key) {
  DYN_VAR *var = (data->dyn_vars ? hashGet(data->dyn_vars, key) : NULL);
  return (var ? atoi(var->str_val) : 0);
}

long dynGetLong(DYN_VAR_AUX_DATA *data, const char *key) {
  DYN_VAR *var = (data->dyn_vars ? hashGet(data->dyn_vars, key) : NULL);
  return (var ? atol(var->str_val) : 0);
}

double dynGetDouble(DYN_VAR_AUX_DATA *data, const char *key) {
  DYN_VAR *var = (data->dyn_vars ? hashGet(data->dyn_vars, key) : NULL);
  return (var ? atof(var->str_val) : 0);
}

const char *dynGetString(DYN_VAR_AUX_DATA *data, const char *key) {
  DYN_VAR *var = (data->dyn_vars ? hashGet(data->dyn_vars, key) : NULL);
  return (var ? var->str_val : "");
}

void dynSetInt(DYN_VAR_AUX_DATA *data, const char *key, int val) {
  DYN_VAR *old = (data->dyn_vars ? hashRemove(data->dyn_vars, key) : NULL);
  if(data->dyn_vars == NULL && val != 0)
    data->dyn_vars = newHashtable();
  if(old != NULL) 
    deleteDynVar(old);
  if(val != 0)
    hashPut(data->dyn_vars, key, newDynVarInt(val));
}

void dynSetLong(DYN_VAR_AUX_DATA *data, const char *key, long val) {
  DYN_VAR *old = (data->dyn_vars ? hashRemove(data->dyn_vars, key) : NULL);
  if(data->dyn_vars == NULL && val != 0)
    data->dyn_vars = newHashtable();
  if(old != NULL) 
    deleteDynVar(old);
  if(val != 0)
    hashPut(data->dyn_vars, key, newDynVarLong(val));
}

void dynSetDouble(DYN_VAR_AUX_DATA *data, const char *key, double val) {
  DYN_VAR *old = (data->dyn_vars ? hashRemove(data->dyn_vars, key) : NULL);
  if(data->dyn_vars == NULL && val != 0)
    data->dyn_vars = newHashtable();
  if(old != NULL) 
    deleteDynVar(old);
  if(val != 0)
    hashPut(data->dyn_vars, key, newDynVarDouble(val));
}

void dynSetString(DYN_VAR_AUX_DATA *data, const char *key, const char *val) {
  DYN_VAR *old = (data->dyn_vars ? hashRemove(data->dyn_vars, key) : NULL);
  if(data->dyn_vars == NULL && *val != '\0') 
    data->dyn_vars = newHashtable();
  if(old != NULL) 
    deleteDynVar(old);
  if(*val != '\0')
    hashPut(data->dyn_vars, key, newDynVarString(val));
}

bool dynHasVar(DYN_VAR_AUX_DATA *data, const char *key) {
  return (data->dyn_vars ? hashIn(data->dyn_vars, key) : FALSE);
}

void dynDeleteVar(DYN_VAR_AUX_DATA *data, const char *key) {
  DYN_VAR *var = (data->dyn_vars ? hashRemove(data->dyn_vars, key) : NULL);
  if(var != NULL) deleteDynVar(var);
}

int charGetVarType(CHAR_DATA *ch, const char *key) {
  return dynGetVarType(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key);
}

int charGetInt(CHAR_DATA *ch, const char *key) {
  return dynGetInt(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key);
}

long charGetLong(CHAR_DATA *ch, const char *key) {
  return dynGetLong(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key);
}

double charGetDouble(CHAR_DATA *ch, const char *key) {
  return dynGetDouble(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key);
}

const char *charGetString(CHAR_DATA *ch, const char *key) {
  return dynGetString(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key);
}

void charSetInt(CHAR_DATA *ch, const char *key, int val) {
  dynSetInt(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key, val);
}

void charSetLong(CHAR_DATA *ch, const char *key, long val) {
  dynSetLong(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key, val);
}

void charSetDouble(CHAR_DATA *ch, const char *key, double val) {
  dynSetDouble(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key, val);
}

void charSetString(CHAR_DATA *ch, const char *key, const char *val) {
  dynSetString(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key, val);
}

bool charHasVar(CHAR_DATA *ch, const char *key) {
  return dynHasVar(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key);
}

void charDeleteVar(CHAR_DATA *ch, const char *key) {
  dynDeleteVar(charGetAuxiliaryData(ch, "dyn_var_aux_data"), key);
}

int roomGetVarType(ROOM_DATA *rm, const char *key) {
  return dynGetVarType(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key);
}

int roomGetInt(ROOM_DATA *rm, const char *key) {
  return dynGetInt(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key);
}

long roomGetLong(ROOM_DATA *rm, const char *key) {
  return dynGetLong(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key);
}

double roomGetDouble(ROOM_DATA *rm, const char *key) {
  return dynGetDouble(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key);
}

const char *roomGetString(ROOM_DATA *rm, const char *key) {
  return dynGetString(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key);
}

void roomSetInt(ROOM_DATA *rm, const char *key, int val) {
  dynSetInt(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key, val);
}

void roomSetLong(ROOM_DATA *rm, const char *key, long val) {
  dynSetLong(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key, val);
}

void roomSetDouble(ROOM_DATA *rm, const char *key, double val) {
  dynSetDouble(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key, val);
}

void roomSetString(ROOM_DATA *rm, const char *key, const char *val) {
  dynSetString(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key, val);
}

bool roomHasVar(ROOM_DATA *rm, const char *key) {
  return dynHasVar(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key);
}

void roomDeleteVar(ROOM_DATA *rm, const char *key) {
  dynDeleteVar(roomGetAuxiliaryData(rm, "dyn_var_aux_data"), key);
}

int objGetVarType(OBJ_DATA *ob, const char *key) {
  return dynGetVarType(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key);
}

int objGetInt(OBJ_DATA *ob, const char *key) {
  return dynGetInt(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key);
}

long objGetLong(OBJ_DATA *ob, const char *key) {
  return dynGetLong(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key);
}

double objGetDouble(OBJ_DATA *ob, const char *key) {
  return dynGetDouble(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key);
}

const char *objGetString(OBJ_DATA *ob, const char *key) {
  return dynGetString(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key);
}

void objSetInt(OBJ_DATA *ob, const char *key, int val) {
  dynSetInt(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key, val);
}

void objSetLong(OBJ_DATA *ob, const char *key, long val) {
  dynSetLong(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key, val);
}

void objSetDouble(OBJ_DATA *ob, const char *key, double val) {
  dynSetDouble(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key, val);
}

void objSetString(OBJ_DATA *ob, const char *key, const char *val) {
  dynSetString(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key, val);
}

bool objHasVar(OBJ_DATA *ob, const char *key) {
  return dynHasVar(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key);
}

void objDeleteVar(OBJ_DATA *ob, const char *key) {
  dynDeleteVar(objGetAuxiliaryData(ob, "dyn_var_aux_data"), key);
}
