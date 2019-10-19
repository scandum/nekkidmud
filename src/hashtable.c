//*****************************************************************************
//
// hashtable.c
//
// Your friendly neighbourhood hashtable. Maps a <key> to a <value>. *sigh*
// why am I not writing this MUD in C++, again?
//
//*****************************************************************************

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "list.h"
#include "hashtable.h"
#include "mud.h"
#include "utils.h"



// how big of a size do our hashtables start out at?
#define DEFAULT_HASH_SIZE        5

struct hashtable_iterator {
  unsigned int curr_bucket;
  HASHTABLE *table;
  LIST_ITERATOR *bucket_i;
};

typedef struct hashtable_entry {
  char *key;
  void *val;
} HASH_ENTRY;

struct hashtable {
  int size;
  int num_buckets;
  LIST **buckets;
};


//
// an internal form of hashGet that returns the entire entry (key and val)
HASH_ENTRY *hashGetEntry(HASHTABLE *table, const char *key){
  unsigned int bucket = string_hash(key) % table->num_buckets;

  if(table->buckets[bucket] == NULL)
    return NULL;
  else {
    LIST_ITERATOR *list_i = newListIterator(table->buckets[bucket]);
    HASH_ENTRY      *elem = NULL;

    for(;(elem = listIteratorCurrent(list_i)) != NULL; listIteratorNext(list_i))
      if(!strcasecmp(key, elem->key))
	break;
    deleteListIterator(list_i);

    return elem;
  }
}

HASH_ENTRY *newHashtableEntry(const char *key, void *val) {
  HASH_ENTRY *entry = malloc(sizeof(HASH_ENTRY));
  entry->key = strdup(key);
  entry->val = val;
  return entry;
}

void deleteHashtableEntry(HASH_ENTRY *entry) {
  if(entry->key) free(entry->key);
  free(entry);
}

//
// Collect all of the HASH_ENTRYs in a hashtable into a single list
LIST *hashCollectEntries(HASHTABLE *table) {
  LIST *list = newList();
  int i;
  for(i = 0; i < table->num_buckets; i++) {
    if(table->buckets[i] == NULL) continue;
    LIST_ITERATOR *list_i = newListIterator(table->buckets[i]);
    HASH_ENTRY      *elem = NULL;
    for(;(elem=listIteratorCurrent(list_i)) != NULL;listIteratorNext(list_i))
      listPut(list, elem);
    deleteListIterator(list_i);
  }
  return list;
}



//*****************************************************************************
// implementation of hashtable.h
// documentation in hashtable.h
//*****************************************************************************
HASHTABLE *newHashtableSize(int num_buckets) {
  int i;
  HASHTABLE *table   = malloc(sizeof(HASHTABLE));
  table->num_buckets = num_buckets;
  table->size        = 0;
  table->buckets = malloc(sizeof(LIST *) * num_buckets);
  for(i = 0; i < num_buckets; i++)
    table->buckets[i] = NULL;
  return table;
}

HASHTABLE *newHashtable(void) {
  return newHashtableSize(DEFAULT_HASH_SIZE);
}


void  deleteHashtable(HASHTABLE *table) {
  int i;
  for(i = 0; i < table->num_buckets; i++) {
    if(table->buckets[i])
      deleteListWith(table->buckets[i], deleteHashtableEntry);
  }

  free(table->buckets);
  free(table);
}

void deleteHashtableWith(HASHTABLE *table, void *func) {
  hashClearWith(table, func);
  deleteHashtable(table);
}

//
// expand a hashtable to the new size
void hashExpand(HASHTABLE *table, int size) {
  // collect all of the key:value pairs
  LIST     *entries = hashCollectEntries(table);
  HASH_ENTRY *entry = NULL;
  int i;

  // delete all of the current buckets
  for(i = 0; i < table->num_buckets; i++) {
    if(table->buckets[i] == NULL) continue;
    deleteList(table->buckets[i]);
  }
  free(table->buckets);

  // now, make new buckets and set them to NULL
  table->buckets = calloc(size, sizeof(LIST *));
  table->num_buckets = size;

  // now, we put all of our entries back into the new buckets
  while((entry = listPop(entries)) != NULL) {
    unsigned int bucket = string_hash(entry->key) % table->num_buckets;
    if(table->buckets[bucket] == NULL) table->buckets[bucket] = newList();
    listPut(table->buckets[bucket], entry);
  }
  deleteList(entries);
}


int hashPut(HASHTABLE *table, const char *key, void *val) {
  HASH_ENTRY *elem = hashGetEntry(table, key);

  // if it's already in, update the value
  if(elem) {
    elem->val = val;
    return 1;
  }
  else {
    // first, see if we'll need to expand the table
    if((table->size * 80)/100 > table->num_buckets)
      hashExpand(table, (table->num_buckets * 150)/100);

    unsigned int bucket = string_hash(key) % table->num_buckets;

    // if the bucket doesn't exist yet, create it
    if(table->buckets[bucket] == NULL)
      table->buckets[bucket] = newList();

    HASH_ENTRY *entry = newHashtableEntry(key, val);
    listPut(table->buckets[bucket], entry);
    table->size++;
    return 1;
  }
}

void *hashGet(HASHTABLE *table, const char *key) {
  HASH_ENTRY *elem = hashGetEntry(table, key);
  if(elem != NULL)
    return elem->val;
  else
    return NULL;
}

void *hashRemove(HASHTABLE *table, const char *key) {
  unsigned int bucket = string_hash(key) % table->num_buckets;

  if(table->buckets[bucket] == NULL)
    return NULL;
  else {
    LIST_ITERATOR *list_i = newListIterator(table->buckets[bucket]);
    HASH_ENTRY *elem = NULL;

    for(;(elem = listIteratorCurrent(list_i)) != NULL; listIteratorNext(list_i))
      if(!strcasecmp(key, elem->key))
	break;
    deleteListIterator(list_i);

    if(elem) {
      void *val = elem->val;
      listRemove(table->buckets[bucket], elem);
      deleteHashtableEntry(elem);
      table->size--;
      return val;
    }
    else
      return NULL;
  }
}

int   hashIn     (HASHTABLE *table, const char *key) {
  return (hashGetEntry(table, key) != NULL);
}

int   hashSize   (HASHTABLE *table) {
  return table->size;
}

LIST *hashCollect(HASHTABLE *table) {
  LIST *list = newList();
  int i;

  for(i = 0; i < table->num_buckets; i++) {
    if(table->buckets[i] == NULL) continue;
    LIST_ITERATOR *list_i = newListIterator(table->buckets[i]);
    HASH_ENTRY      *elem = NULL;
    for(;(elem=listIteratorCurrent(list_i)) != NULL;listIteratorNext(list_i))
      listPut(list, strdup(elem->key));
    deleteListIterator(list_i);
  }
  return list;
}

void hashClear(HASHTABLE *table) {
  hashClearWith(table, NULL);
}

void hashClearWith(HASHTABLE *table, void *func) {
  void (* free_func)(void *) = func;
  int i;

  for(i = 0; i < table->num_buckets; i++) {
    if(table->buckets[i] == NULL) continue;
    HASH_ENTRY *elem = NULL;
    while( (elem = listPop(table->buckets[i])) != NULL) {
      if(free_func && elem->val)
	free_func(elem->val);
      deleteHashtableEntry(elem);
    }
  }
}



//*****************************************************************************
// implementation of the hashtable iterator
// documentation in hashtable.h
//*****************************************************************************
HASH_ITERATOR *newHashIterator(HASHTABLE *table) {
  HASH_ITERATOR *I = malloc(sizeof(HASH_ITERATOR));
  I->table = table;
  I->bucket_i = NULL;
  hashIteratorReset(I);

  return I;
}

void        deleteHashIterator     (HASH_ITERATOR *I) {
  if(I->bucket_i) deleteListIterator(I->bucket_i);
  free(I);
}

void        hashIteratorReset      (HASH_ITERATOR *I) {
  int i;

  if(I->bucket_i) deleteListIterator(I->bucket_i);
  I->bucket_i = NULL;
  I->curr_bucket = 0;

  // bucket_i will be NULL if there are no elements
  for(i = 0; i < I->table->num_buckets; i++) {
    if(I->table->buckets[i] != NULL &&
       listSize(I->table->buckets[i]) > 0) {
      I->curr_bucket = i;
      I->bucket_i = newListIterator(I->table->buckets[i]);
      break;
    }
  }
}


void        hashIteratorNext       (HASH_ITERATOR *I) {
  // no elements in the hashtable
  if(I->bucket_i == NULL) 
    return;
  // we're at the end of our list
  else if(listIteratorNext(I->bucket_i) == NULL) {
    deleteListIterator(I->bucket_i);
    I->bucket_i = NULL;
    I->curr_bucket++;
    
    for(; I->curr_bucket < I->table->num_buckets; I->curr_bucket++) {
      if(I->table->buckets[I->curr_bucket] != NULL &&
	 listSize(I->table->buckets[I->curr_bucket]) > 0) {
	I->bucket_i = newListIterator(I->table->buckets[I->curr_bucket]);
	break;
      }
    }
  }
}


const char *hashIteratorCurrentKey (HASH_ITERATOR *I) {
  if(!I->bucket_i) 
    return NULL;
  else {
    HASH_ENTRY *entry = listIteratorCurrent(I->bucket_i);
    if(entry)
      return entry->key;
    else
      return NULL;
  }
}


void       *hashIteratorCurrentVal (HASH_ITERATOR *I) {
  if(!I->bucket_i) 
    return NULL;
  else {
    HASH_ENTRY *entry = listIteratorCurrent(I->bucket_i);
    if(entry)
      return entry->val;
    else
      return NULL;
  }
}
