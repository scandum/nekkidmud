#ifndef __STORAGE_H
#define __STORAGE_H
//******************************************************************************
//
// storage.h
//
// It would be nice to have a standards for storing and loading data from files.
// I looked at XML, but the language didn't really appeal to me. YAML
// (http://www.yaml.org) caught my eye, but I did not manage to find a set of
// YAML utilities for C, so I decided to try and design my own conventions for
// storing data. Most of them are based on what I learned from my (brief)
// exposure to YAML.
//
//******************************************************************************


//
// Create a new storage set for storing data
//
STORAGE_SET *new_storage_set();


//
// create a new list for holding storage sets
//
STORAGE_SET_LIST *new_storage_list();


//
// write the storage set to the specified file
//
void storage_write(STORAGE_SET *set, const char *fname);


//
// read the storage set from the specified file
//
STORAGE_SET *storage_read(const char *fname);


//
// close and delete the specified storage set
//
void storage_close(STORAGE_SET *set);


//
// Give the next storage data from the storage list. If none exist, return NULL
//
STORAGE_SET *storage_list_next(STORAGE_SET_LIST *list);


//
// Put storage data into the storage list.
//
void storage_list_put(STORAGE_SET_LIST *list, STORAGE_SET *set);


//
// store various datatypes to the storage data
//
void    store_set(STORAGE_SET *set, const char *key, STORAGE_SET *val);
void   store_list(STORAGE_SET *set, const char *key, STORAGE_SET_LIST *val);
void store_string(STORAGE_SET *set, const char *key, const char *val);
void store_double(STORAGE_SET *set, const char *key, double val);
void    store_int(STORAGE_SET *set, const char *key, int val);
void   store_long(STORAGE_SET *set, const char *key, long val);
void   store_bool(STORAGE_SET *set, const char *key, bool val);


//
// read various datatypes fro mthe storage data
//
STORAGE_SET         *read_set(STORAGE_SET *set, const char *key);
STORAGE_SET_LIST   *read_list(STORAGE_SET *set, const char *key);
const char       *read_string(STORAGE_SET *set, const char *key);
double            read_double(STORAGE_SET *set, const char *key);
int                  read_int(STORAGE_SET *set, const char *key);
long                read_long(STORAGE_SET *set, const char *key);
bool                read_bool(STORAGE_SET *set, const char *key);


//
// returns TRUE if the key exists in the set, and false if it
// doesn't. Note that you can read key values even if the key
// doesn't exist; the values will just have a default value
// (0 for doubles and ints, "\0" for strings, an empty list
// for lists).
//
bool storage_contains(STORAGE_SET *set, const char *key);


//
// utilities to speed up the reading/saving of lists
//
STORAGE_SET_LIST *gen_store_list(LIST *list, void *storer);
LIST  *gen_read_list(STORAGE_SET_LIST *list, void *reader);

#endif // __STORAGE_H
