//*****************************************************************************
//
// auxiliary.c
//
// when installing a new module, it is often required that extra data be 
// stored in the basic datastructures (rooms, characters, objects, etc...). 
// However, we want to keep the modules as removed from the core of the MUD
// as possible, and having to put in extra data into the basic datastructures 
// seems like an undesirable thing. Provided below are a set of functions that 
// can be used for "installing" new data into the datastructures without having
// to directly edit the code.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "storage.h"
#include "auxiliary.h"



//*****************************************************************************
//
// local variables, datastructures, and functions
//
//*****************************************************************************

//
// the function sets used for loading and saving 
// auxiliary functions on our datatypes
HASHTABLE *auxiliary_manip_funcs = NULL;

struct auxiliary_functions {
  bitvector_t aux_type;
  bool           is_py;
  void          *(* new)(void);
  void        (* delete)(void *data);
  void        (* copyTo)(void *from, void *to);
  void        *(*  copy)(void *data);
  STORAGE_SET *(* store)(void *data);
  void        *(*  read)(STORAGE_SET *set);
};



//*****************************************************************************
//
// implementation of auxiliary.h
// documentation in auxiliary.h
//
//*****************************************************************************

void init_auxiliaries() {
  auxiliary_manip_funcs = newHashtable();
}

AUXILIARY_FUNCS *
newAuxiliaryFuncs(bitvector_t aux_type, void *new, void *delete, 
		  void *copyTo, void *copy, void *store, void *read) {
  AUXILIARY_FUNCS *newfuncs = malloc(sizeof(AUXILIARY_FUNCS));
  newfuncs->is_py    = FALSE;
  newfuncs->aux_type = aux_type;
  newfuncs->new      = new;
  newfuncs->delete   = delete;
  newfuncs->copyTo   = copyTo;
  newfuncs->copy     = copy;
  newfuncs->store    = store;
  newfuncs->read     = read;
  return newfuncs;
}

void
deleteAuxiliaryFuncs(AUXILIARY_FUNCS *funcs) {
  free(funcs);
}

void auxiliaryFuncSetIsPy(AUXILIARY_FUNCS *funcs, bool val) {
  funcs->is_py = val;
}

void
auxiliariesInstall(const char *name, AUXILIARY_FUNCS *funcs) {
  hashPut(auxiliary_manip_funcs, name, funcs);
}


void
auxiliariesUninstall(const char *name) {
  AUXILIARY_FUNCS *funcs = hashRemove(auxiliary_manip_funcs, name);
  if(funcs != NULL)
    deleteAuxiliaryFuncs(funcs);
}


AUXILIARY_FUNCS *
auxiliariesGetFuncs(const char *name) {
  return hashGet(auxiliary_manip_funcs, name);
}


AUX_TABLE *
newAuxiliaryData(bitvector_t aux_type) {
  AUX_TABLE        *data = newHashtable();
  HASH_ITERATOR  *hash_i = newHashIterator(auxiliary_manip_funcs);
  AUXILIARY_FUNCS *funcs = NULL;
  const char       *name = NULL;

  ITERATE_HASH(name, funcs, hash_i) {
    if(IS_SET(funcs->aux_type, aux_type)) {
      // are we dealing with python data or not?
      if(!funcs->is_py) 
	hashPut(data, name, funcs->new());
      else {
	// recast the new function
	void *(* new)(const char *) = (void *)funcs->new;
	hashPut(data, name, new(name));
      }
    }
  } deleteHashIterator(hash_i);
  return data;
}


void
auxiliaryEnsureDataComplete(AUX_TABLE *data, bitvector_t aux_type) {
  HASH_ITERATOR  *hash_i = newHashIterator(auxiliary_manip_funcs);
  AUXILIARY_FUNCS *funcs = NULL;
  const char       *name = NULL;

  ITERATE_HASH(name, funcs, hash_i) {
    if(IS_SET(funcs->aux_type, aux_type) && !hashGet(data, name)) {
      // are we dealing with python data or not?
      if(!funcs->is_py) 
	hashPut(data, name, funcs->new());
      else {
	// recast the new function
	void *(* new)(const char *) = (void *)funcs->new;
	hashPut(data, name, new(name));
      }
    }
  } deleteHashIterator(hash_i);
}


void
deleteAuxiliaryData(AUX_TABLE *data) {
  // go across all of the data in the hashtable, and delete it
  AUXILIARY_FUNCS *funcs  = NULL;
  HASH_ITERATOR   *hash_i = newHashIterator(data);
  void            *entry  = NULL;
  const char      *name   = NULL;

  ITERATE_HASH(name, entry, hash_i) {
    funcs = auxiliariesGetFuncs(name);
    funcs->delete(entry);
  } deleteHashIterator(hash_i);
  deleteHashtable(data);
}


STORAGE_SET *
auxiliaryDataStore(AUX_TABLE *data) {
  STORAGE_SET *set = new_storage_set();
  AUXILIARY_FUNCS *funcs  = NULL;
  HASH_ITERATOR   *hash_i = newHashIterator(data);
  void            *entry  = NULL;
  const char      *name   = NULL;

  ITERATE_HASH(name, entry, hash_i) {
    funcs = auxiliariesGetFuncs(name);
    if(funcs->store) 
      store_set(set, name, funcs->store(entry));
  } deleteHashIterator(hash_i);
  return set;
}


AUX_TABLE *
auxiliaryDataRead(STORAGE_SET *set, bitvector_t aux_type) {
  AUX_TABLE       *data = newHashtable();
  HASH_ITERATOR  *hash_i = newHashIterator(auxiliary_manip_funcs);
  AUXILIARY_FUNCS *funcs = NULL;
  const char       *name = NULL;

  ITERATE_HASH(name, funcs, hash_i) {
    if(!IS_SET(funcs->aux_type, aux_type))
      continue;
    // are we dealing with python data or not?
    if(!funcs->is_py && funcs->read)
      hashPut(data, name, funcs->read(read_set(set, name)));
    else if(funcs->read) {
      // recast the read function
      void *(* read)(const char *, STORAGE_SET *) = (void *)funcs->read;
      hashPut(data, name, read(name, read_set(set, name)));
    }
    // are we dealing with python data or not?
    else if(!funcs->is_py) 
      hashPut(data, name, funcs->new());
    else {
      // recast the new function
      void *(* new)(const char *) = (void *)funcs->new;
      hashPut(data, name, new(name));
    }
  } deleteHashIterator(hash_i);
  return data;
}


void
auxiliaryDataCopyTo(AUX_TABLE *from, AUX_TABLE *to) {
  AUXILIARY_FUNCS *funcs = NULL;
  HASH_ITERATOR  *hash_i = NULL;
  void            *entry = NULL;
  const char       *name = NULL;

  // first, delete all of the old data
  if(hashSize(to) > 0) {
    hash_i = newHashIterator(to);
    ITERATE_HASH(name, entry, hash_i) {
      funcs = auxiliariesGetFuncs(name);
      funcs->delete(entry);
    } deleteHashIterator(hash_i);
  }

  // now, copy in all of the new data
  if(hashSize(from) > 0) {
    hash_i = newHashIterator(from);
    ITERATE_HASH(name, entry, hash_i) {
      funcs = auxiliariesGetFuncs(name);
      hashPut(to, name, funcs->copy(entry));
    } deleteHashIterator(hash_i);
  }
}


AUX_TABLE *
auxiliaryDataCopy(AUX_TABLE *data) {
  AUX_TABLE *newdata = newHashtableSize(hashSize(data));
  auxiliaryDataCopyTo(data, newdata);
  return newdata;
}

void *auxiliaryGet(AUX_TABLE *table, const char *key) {
  return hashGet(table, key);
}
