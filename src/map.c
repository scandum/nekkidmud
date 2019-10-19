//*****************************************************************************
//
// map.c
//
// similar to a hashtable, but keys as well as values can be can be anything.
//
//*****************************************************************************

#include <stdlib.h>
#include "list.h"
#include "map.h"


// how big of a size does our map start out at?
#define DEFAULT_MAP_SIZE        5


int gen_hash_cmp(const void *key1, const void *key2) {
  int val = (key2 - key1);
  if(val < 0)      return -1;
  else if(val > 0) return  1;
  else             return  0;
  return val;
}

unsigned long gen_hash_func(const void *key) {
  return (unsigned long)key;
}

struct map_iterator {
  int curr_bucket;
  MAP *map;
  LIST_ITERATOR *bucket_i;
};

typedef struct map_entry {
  const void *key;
  void *val;
} MAP_ENTRY;

struct map_data {
  int size;
  int num_buckets;
  struct list **buckets;
  unsigned long (* hash_func)(const void *key);
  int           (*  compares)(const void *key1, const void *key2);
};


//
// an internal form of hashGet that returns the entire entry (key and val)
//
MAP_ENTRY *mapGetEntry(MAP *map, const void *key) {
  unsigned int bucket = map->hash_func(key) % map->num_buckets;

  if(map->buckets[bucket] == NULL)
    return NULL;
  else {
    LIST_ITERATOR *list_i = newListIterator(map->buckets[bucket]);
    MAP_ENTRY     *elem   = NULL;

    ITERATE_LIST(elem, list_i) {
      if(!map->compares(key, elem->key))
	break;
    } deleteListIterator(list_i);

    return elem;
  }
}


MAP_ENTRY *newMapEntry(const void *key, void *val) {
  MAP_ENTRY *entry = malloc(sizeof(MAP_ENTRY));
  entry->key = key;
  entry->val = val;
  return entry;
}

void deleteMapEntry(MAP_ENTRY *entry) {
  free(entry);
}


//
// Collect all of the MAP_ENTRYs in a map into a single list
LIST *mapCollectEntries(MAP *map) {
  LIST *list = newList();
  int i;
  for(i = 0; i < map->num_buckets; i++) {
    if(map->buckets[i] == NULL) continue;
    LIST_ITERATOR *list_i = newListIterator(map->buckets[i]);
    MAP_ENTRY       *elem = NULL;
    ITERATE_LIST(elem, list_i) {
      listPut(list, elem);
    } deleteListIterator(list_i);
  }
  return list;
}


//
// expand a map to the new size
void mapExpand(MAP *map, int size) {
  // collect all of the key:value pairs
  LIST     *entries = mapCollectEntries(map);
  MAP_ENTRY  *entry = NULL;
  int i;

  // delete all of the current buckets
  for(i = 0; i < map->num_buckets; i++) {
    if(map->buckets[i] == NULL) continue;
    deleteList(map->buckets[i]);
  }
  free(map->buckets);

  // now, make new buckets and set them to NULL
  map->buckets = calloc(size, sizeof(LIST *));
  map->num_buckets = size;

  // now, we put all of our entries back into the new buckets
  while((entry = listPop(entries)) != NULL) {
    unsigned int bucket = map->hash_func(entry->key) % map->num_buckets;
    if(map->buckets[bucket] == NULL) map->buckets[bucket] = newList();
    listPut(map->buckets[bucket], entry);
  }
  deleteList(entries);
}



//*****************************************************************************
//
// implementation of map.h
// documentation found in map.h
//
//*****************************************************************************
MAP *newMapSize(void *hash_func, void *compares, int size) {
  MAP *map = malloc(sizeof(MAP));
  map->size        = 0;
  map->num_buckets = size;
  map->hash_func   = (hash_func ? hash_func : gen_hash_func);
  map->compares    = (compares  ? compares  : gen_hash_cmp);
  map->buckets     = calloc(map->num_buckets, sizeof(LIST *));
  return map;
}

MAP *newMap(void *hash_func, void *compares) {
  return newMapSize(hash_func, compares, DEFAULT_MAP_SIZE);
}

void deleteMap(MAP *map) {
  int i;
  for(i = 0; i < map->num_buckets; i++) {
    if(map->buckets[i])
      deleteListWith(map->buckets[i], deleteMapEntry);
  }
  free(map->buckets);
  free(map);
}

int mapPut(MAP *map, const void *key, void *val) {
  MAP_ENTRY *elem = mapGetEntry(map, key);

  // update the val if it's already here
  if(elem)
    elem->val = val;
  else {
    // first, see if we'll need to expand the map
    if((map->size * 80)/100 > map->num_buckets)
      mapExpand(map, (map->num_buckets * 150)/100);

    unsigned int bucket = map->hash_func(key) % map->num_buckets;

    // if the bucket doesn't exist yet, create it
    if(map->buckets[bucket] == NULL)
      map->buckets[bucket] = newList();

    MAP_ENTRY *entry = newMapEntry(key, val);
    listPut(map->buckets[bucket], entry);
    map->size++;
  }

  return 1;
}

void *mapGet(MAP *map, const void *key) {
  MAP_ENTRY *elem = mapGetEntry(map, key);
  if(elem)
    return elem->val;
  else
    return NULL;
}

void *mapRemove(MAP *map, const void *key) {
  unsigned int bucket = map->hash_func(key) % map->num_buckets;

  if(map->buckets[bucket] == NULL)
    return NULL;
  else {
    LIST_ITERATOR *list_i = newListIterator(map->buckets[bucket]);
    MAP_ENTRY       *elem = NULL;

    ITERATE_LIST(elem, list_i) {
      if(!map->compares(key, elem->key))
	break;
    } deleteListIterator(list_i);

    if(elem == NULL)
      return NULL;

    void *val = elem->val;
    listRemove(map->buckets[bucket], elem);
    deleteMapEntry(elem);
    map->size--;
    return val;
  }
}

int mapIn(MAP *map, const void *key) {
  return (mapGetEntry(map, key) != NULL);
}

int mapSize(MAP *map) {
  return map->size;
}



//*****************************************************************************
//
// implementation of the hashmap iterator
// documentation in hashmap.h
//
//*****************************************************************************
MAP_ITERATOR *newMapIterator(MAP *map) {
  MAP_ITERATOR *I = malloc(sizeof(MAP_ITERATOR));
  I->map = map;
  I->bucket_i = NULL;
  mapIteratorReset(I);

  return I;
}

void        deleteMapIterator     (MAP_ITERATOR *I) {
  if(I->bucket_i) deleteListIterator(I->bucket_i);
  free(I);
}

void        mapIteratorReset      (MAP_ITERATOR *I) {
  int i;

  if(I->bucket_i) deleteListIterator(I->bucket_i);
  I->bucket_i = NULL;
  I->curr_bucket = 0;

  // bucket_i will be NULL if there are no elements
  for(i = 0; i < I->map->num_buckets; i++) {
    if(I->map->buckets[i] != NULL &&
       listSize(I->map->buckets[i]) > 0) {
      I->curr_bucket = i;
      I->bucket_i = newListIterator(I->map->buckets[i]);
      break;
    }
  }
}


void        mapIteratorNext       (MAP_ITERATOR *I) {
  // no elements in the hashmap
  if(I->bucket_i == NULL)
    return;
  // we're at the end of our list
  else if(listIteratorNext(I->bucket_i) == NULL) {
    deleteListIterator(I->bucket_i);
    I->bucket_i = NULL;
    I->curr_bucket++;
    
    for(; I->curr_bucket < I->map->num_buckets; I->curr_bucket++) {
      if(I->map->buckets[I->curr_bucket] != NULL &&
	 listSize(I->map->buckets[I->curr_bucket]) > 0) {
	I->bucket_i = newListIterator(I->map->buckets[I->curr_bucket]);
	break;
      }
    }
  }
}

const void *mapIteratorCurrentKey(MAP_ITERATOR *I) {
  if(!I->bucket_i)
    return NULL;
  else {
    MAP_ENTRY *entry = ((MAP_ENTRY *) listIteratorCurrent(I->bucket_i));
    if(entry)
      return entry->key;
    else
      return NULL;
  }
}

void       *mapIteratorCurrentVal (MAP_ITERATOR *I) {
  if(!I->bucket_i) 
    return NULL;
  else {
    MAP_ENTRY *entry = ((MAP_ENTRY *) listIteratorCurrent(I->bucket_i));
    if(entry)
      return entry->val;
    else
      return NULL;
  }
}
