//*****************************************************************************
//
// property_table.c
//
// A little idea I thought up, mainly for quick lookup of elements by vnum.
//
// Exactly like a hashtable, except the <key> component of some element is
// built into the element itself, and defined by a function that is passed
// in when the propery table is created. Currently, the property_table only
// allows one item per property value to be stored, but later on down the
// road we will expand this so that the table will collect -sets- of items
// with a given property.
//
// the main purpose for this table is to store obj/mob/script/etc prototypes,
// as well as rooms in the game.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "property_table.h"

struct property_table {
  int num_buckets;
  int (* key_function)(void *elem);
  LIST **buckets;
};

struct property_table_iterator {
  int curr_bucket;
  PROPERTY_TABLE *table;
  LIST_ITERATOR *bucket_i;
};


//*****************************************************************************
// local functions
//*****************************************************************************

//
// Find the bucket the key belongs to
int find_bucket(int key, int num_buckets) {
  // simple for now: just take the modulo. Make sure it's always positive
  return (key < 0 ? -key : key) % num_buckets;
};



//*****************************************************************************
// implementation of property_table.h
// documentation in property_table.h
//*****************************************************************************
PROPERTY_TABLE *newPropertyTable(void *key_function, int num_buckets) {
  int i;

  PROPERTY_TABLE *table = malloc(sizeof(PROPERTY_TABLE));
  table->buckets = malloc(sizeof(LIST *) * num_buckets);

  // all NULL until they actually get a content
  for(i = 0; i < num_buckets; i++)
    table->buckets[i] = NULL;

  table->num_buckets = num_buckets;
  table->key_function = key_function;

  return table;
};


void deletePropertyTable(PROPERTY_TABLE *table) {
  int i;

  for(i = 0; i < table->num_buckets; i++)
    if(table->buckets[i] != NULL)
      deleteList(table->buckets[i]);

  free(table->buckets);
  free(table);
};


void propertyTablePut(PROPERTY_TABLE *table, void *elem) {
  // find out what bucket we belong to
  int hash_bucket = find_bucket(table->key_function(elem), table->num_buckets);

  // see if we already exist
  if(table->buckets[hash_bucket] != NULL && 
     listIn(table->buckets[hash_bucket], elem))
    return;

  // add us to the bucket
  if(table->buckets[hash_bucket] == NULL)
    table->buckets[hash_bucket] = newList();
  // listPut ensures only one copy is in the list
  listPut(table->buckets[hash_bucket], elem);
};


void *propertyTableRemove(PROPERTY_TABLE *table, int key) {
  // find out what bucket we belong to
  int hash_bucket = find_bucket(key, table->num_buckets);

  // see if the bucket exists
  if(table->buckets[hash_bucket] == NULL)
    return NULL;
  else {
    LIST_ITERATOR *list_i = newListIterator(table->buckets[hash_bucket]);
    void *elem = NULL;

    // iterate across the list until we find what we need.
    ITERATE_LIST(elem, list_i) {
      // we found it!
      if(key == table->key_function(elem)) {
	listRemove(table->buckets[hash_bucket], elem);
	break;
      }
    }

    deleteListIterator(list_i);
    return elem;
  }
};


void *propertyTableGet(PROPERTY_TABLE *table, int key) {
  // find out what bucket we belong to
  int hash_bucket = find_bucket(key, table->num_buckets);

  // see if the bucket exists
  if(table->buckets[hash_bucket] == NULL)
    return NULL;
  else {
    LIST_ITERATOR *list_i = newListIterator(table->buckets[hash_bucket]);
    void *elem = NULL;

    // iterate across the list until we find what we need.
    ITERATE_LIST(elem, list_i) {
      // we found it!
      if(key == table->key_function(elem))
	break;
    }

    deleteListIterator(list_i);
    return elem;
  }
};


bool propertyTableIn(PROPERTY_TABLE *table, int key) {
  return (propertyTableGet(table, key) != NULL);
};


//*****************************************************************************
// property table iterator
//
// we may sometimes want to iterate across all of the elements in a table.
// this lets us do so.
//*****************************************************************************
PROPERTY_TABLE_ITERATOR *newPropertyTableIterator(PROPERTY_TABLE *T) {
  PROPERTY_TABLE_ITERATOR *I = malloc(sizeof(PROPERTY_TABLE_ITERATOR));

  I->table = T;
  I->curr_bucket = 0;
  I->bucket_i = NULL;
  propertyTableIteratorReset(I);

  return I;
}


void deletePropertyTableIterator(PROPERTY_TABLE_ITERATOR *I) {
  if(I->bucket_i) deleteListIterator(I->bucket_i);
  free(I);
}


void propertyTableIteratorReset(PROPERTY_TABLE_ITERATOR *I) {
  int i;

  if(I->bucket_i) 
    deleteListIterator(I->bucket_i);
  I->bucket_i = NULL;
  I->curr_bucket = 0;

  for(i = 0; i < I->table->num_buckets; i++) {
    if(I->table->buckets[i] == NULL)
      continue;
    if(isListEmpty(I->table->buckets[i]))
      continue;
    else {
      I->curr_bucket = i;
      I->bucket_i = newListIterator(I->table->buckets[i]);
      break;
    }
  }
}


void *propertyTableIteratorNext(PROPERTY_TABLE_ITERATOR *I) {
  // we have no iterator ... we have no elements left to iterate over!
  if(I->bucket_i == NULL)
    return NULL;
  else {
    void *elem = listIteratorNext(I->bucket_i);
    // we're okay ... we have an element
    if(elem != NULL)
      return elem;
    // otherwise, we need to move onto a new bucket
    else {
      deleteListIterator(I->bucket_i);
      I->bucket_i = NULL;

      I->curr_bucket++;
      // find the next non-empty list
      for(; I->curr_bucket < I->table->num_buckets; I->curr_bucket++) {
	if(I->table->buckets[I->curr_bucket] == NULL)
	  continue;
	if(isListEmpty(I->table->buckets[I->curr_bucket]))
	  continue;
	I->bucket_i = newListIterator(I->table->buckets[I->curr_bucket]);
	break;
      }

      // we've ran out of buckets!
      if(I->curr_bucket == I->table->num_buckets)
	return NULL;
      else
	return listIteratorCurrent(I->bucket_i);
    }
  }
}


void *propertyTableIteratorCurrent(PROPERTY_TABLE_ITERATOR *I) {
  // we have no elements!
  if(I->bucket_i == NULL)
    return NULL;
  /* we're at the end of a list ... gotta find the next element
   * WAIT! We should never encounter this situation, because whenever
   * we go to get the next element, we ensure that if we're at the end
   * of a list, we go on to find the next list with elements.
  else if(listIteratorCurrent(I->bucket_i) == NULL)
    return propertyTableIteratorNext(I);
  */
  else
    return listIteratorCurrent(I->bucket_i);
}
