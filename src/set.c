//*****************************************************************************
//
// set.h
//
// a non-ordered container that has constant lookup time.
//
//*****************************************************************************

#include <stdlib.h>
#include "list.h"
#include "set.h"

// how big of a size do our set start out at?
#define DEFAULT_SET_SIZE        5

struct set_data {
  int    num_buckets;
  int           size;
  LIST     **buckets;
  int  (* cmp)(const void *, const void *);
  int (* hash)(const void *);
};

struct set_iterator {
  int         curr_bucket; // the bucket number we're currently on
  struct set_data    *set; // the set we're iterating over
  LIST_ITERATOR *bucket_i; // the iterator for our current bucket
};



//*****************************************************************************
// local functions
//*****************************************************************************

//
// compre two elements for equality
int gen_set_cmp(const void *key1, const void *key2) {
  int val = (key2 - key1);
  if(val < 0)      return -1;
  else if(val > 0) return  1;
  else             return  0;
  return val;
}

//
// hash an item
int gen_set_hash(const void *key) {
  int val = (int)key;
  if(val < 0) return -val;
  else        return  val;
}

//
// expand a set to the new number of buckets
void setExpand(SET *set, int size) {
  // collect all of the key:value pairs
  LIST *entries = setCollect(set);
  void *entry = NULL;
  int i;

  // delete all of the current buckets
  for(i = 0; i < set->num_buckets; i++) {
    if(set->buckets[i] == NULL) continue;
    deleteList(set->buckets[i]);
  }
  free(set->buckets);

  // now, make new buckets and set them to NULL
  set->buckets = calloc(size, sizeof(LIST *));
  set->num_buckets = size;

  // now, we put all of our entries back into the new buckets
  while((entry = listPop(entries)) != NULL) {
    int bucket = set->hash(entry) % set->num_buckets;
    if(set->buckets[bucket] == NULL) set->buckets[bucket] = newList();
    listPut(set->buckets[bucket], entry);
  }
  deleteList(entries);
}



//*****************************************************************************
// implementation of set.h
//*****************************************************************************
SET *newSet(void) {
  SET *set         = calloc(1, sizeof(SET));
  set->buckets     = calloc(DEFAULT_SET_SIZE, sizeof(LIST *));
  set->num_buckets = DEFAULT_SET_SIZE;
  set->size        = 0;
  set->cmp         = gen_set_cmp;
  set->hash        = gen_set_hash;
  return set;
}

void deleteSet(SET *set) {
  int i;

  for(i = 0; i < set->num_buckets; i++)
    if(set->buckets[i] != NULL)
      deleteList(set->buckets[i]);

  free(set->buckets);
  free(set);
};

int setSize(SET *set) {
  return set->size;
}

void setPut(SET *set, void *elem) {
  // only one copy per set
  if(setIn(set, elem))
    return;

  // first, see if we'll need to expand the table
  if((set->size * 80)/100 > set->num_buckets)
    setExpand(set, (set->num_buckets * 150)/100);

  // find out what bucket we belong to
  int hash_bucket = set->hash(elem) % set->num_buckets;

  // add us to the bucket
  if(set->buckets[hash_bucket] == NULL)
    set->buckets[hash_bucket] = newList();
  // listPut ensures only one copy is in the list
  listPut(set->buckets[hash_bucket], elem);
  set->size++;
}

void *setRemove(SET *set, void *elem) {
  // find out what bucket we belong to
  int hash_bucket = set->hash(elem) % set->num_buckets;

  // see if the bucket exists
  if(set->buckets[hash_bucket] != NULL) {
    if(listRemoveWith(set->buckets[hash_bucket], elem, set->cmp)) {
      set->size--;
      return elem;
    }
  }

  return NULL;
}

int setIn(SET *set, const void *elem) {
  // find out what bucket we belong to
  int hash_bucket = set->hash(elem) % set->num_buckets;

  if(set->buckets[hash_bucket] != NULL)
    return (listGetWith(set->buckets[hash_bucket], elem, set->cmp) != NULL);
  else
    return 0;
}

LIST *setCollect(SET *set) {
  LIST *list = newList();
  int i;

  for(i = 0; i < set->num_buckets; i++) {
    if(set->buckets[i] == NULL) continue;
    LIST_ITERATOR *list_i = newListIterator(set->buckets[i]);
    void            *elem = NULL;
    ITERATE_LIST(elem, list_i) {
      listPut(list, elem);
    } deleteListIterator(list_i);
  }
  return list;
}

SET  *setCopy(SET *set) {
  SET *newset = newSet();
  setChangeHashing(newset, set->cmp, set->hash);
  setExpand(newset, set->num_buckets);
  SET_ITERATOR *set_i = newSetIterator(set);
  void          *elem = NULL;
  ITERATE_SET(elem, set_i) {
    setPut(newset, elem);
  } deleteSetIterator(set_i);
  return newset;
}

SET  *setUnion(SET *set1, SET *set2) {
  SET *newset   = NULL;
  SET *copyfrom = NULL; 
  if(set1->size > set2->size) {
    copyfrom = set2;
    newset   = setCopy(set1);
  }
  else {
    copyfrom = set1;
    newset   = setCopy(set2);
  }

  // copy all of the remaining elements
  SET_ITERATOR *set_i = newSetIterator(copyfrom);
  void          *elem = NULL;
  ITERATE_SET(elem, set_i) {
    setPut(newset, elem);
  } deleteSetIterator(set_i);

  return newset;
}

SET  *setIntersection(SET *set1, SET *set2) {
  SET *intersection = NULL;
  SET *compagainst  = NULL;
  if(set1->size > set2->size) {
    intersection = setCopy(set2);
    compagainst  = set1;
  }
  else {
    intersection = setCopy(set1);
    compagainst  = set2;
  }

  // remove everything not in the intersection
  SET_ITERATOR *set_i = newSetIterator(intersection);
  void          *elem = NULL;
  ITERATE_SET(elem, set_i) {
    if(!setIn(compagainst, elem))
      setRemove(intersection, elem);
  } deleteSetIterator(set_i);
  return intersection;
}

void setChangeHashing(SET *set, void *cmp_func, void *hash_func) {
  set->cmp  = cmp_func;
  set->hash = hash_func;
}



//*****************************************************************************
// set iterator
//
// we may sometimes want to iterate across all of the elements in a set.
// this lets us do so.
//*****************************************************************************
SET_ITERATOR *newSetIterator(SET *S) {
  SET_ITERATOR *I = malloc(sizeof(SET_ITERATOR));

  I->set = S;
  I->curr_bucket = 0;
  I->bucket_i = NULL;
  setIteratorReset(I);

  return I;
}


void deleteSetIterator(SET_ITERATOR *I) {
  if(I->bucket_i) deleteListIterator(I->bucket_i);
  free(I);
}


void setIteratorReset(SET_ITERATOR *I) {
  int i;

  if(I->bucket_i) 
    deleteListIterator(I->bucket_i);
  I->bucket_i = NULL;
  I->curr_bucket = 0;

  for(i = 0; i < I->set->num_buckets; i++) {
    if(I->set->buckets[i] == NULL)
      continue;
    if(isListEmpty(I->set->buckets[i]))
      continue;
    else {
      I->curr_bucket = i;
      I->bucket_i = newListIterator(I->set->buckets[i]);
      break;
    }
  }
}


void *setIteratorNext(SET_ITERATOR *I) {
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
      for(; I->curr_bucket < I->set->num_buckets; I->curr_bucket++) {
	if(I->set->buckets[I->curr_bucket] == NULL)
	  continue;
	if(isListEmpty(I->set->buckets[I->curr_bucket]))
	  continue;
	I->bucket_i = newListIterator(I->set->buckets[I->curr_bucket]);
	break;
      }

      // we've ran out of buckets!
      if(I->curr_bucket == I->set->num_buckets)
	return NULL;
      else
	return listIteratorCurrent(I->bucket_i);
    }
  }
}


void *setIteratorCurrent(SET_ITERATOR *I) {
  // we have no elements!
  if(I->bucket_i == NULL)
    return NULL;
  else
    return listIteratorCurrent(I->bucket_i);
}
