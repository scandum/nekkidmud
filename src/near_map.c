//*****************************************************************************
//
// near_map.c
//
// hash tables (and maps, more broadly speaking) map a key to a value. The
// value can only be retreived by using the key. However, there's some cases
// where we'd like to be lazy about providing the proper key and instead be
// provided with the "best match". For instance, looking up help files or
// commands. For the north command, we'd like "n", "no", "nor", nort", and 
// "north" to all be viable commands. This is what a near-map tries to
// accomplish.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "near_map.h"



//*****************************************************************************
// local datastructures, functions, and defines
//*****************************************************************************
#define NUM_NEAR_MAP_BUCKETS   27 // 26 for letters, 1 for non-alpha

struct near_map {
  LIST *bucket[NUM_NEAR_MAP_BUCKETS];
};

struct near_iterator {
  NEAR_MAP         *map;
  LIST_ITERATOR *curr_i;
  int         curr_buck;
};

typedef struct {
  char        *key;
  char *min_abbrev;
  void       *data;
} NEAR_MAP_ELEM;


NEAR_MAP_ELEM *newNearMapElem(void *data, const char *key, 
			      const char *min_abbrev) {
  NEAR_MAP_ELEM *elem = malloc(sizeof(NEAR_MAP_ELEM));
  elem->data          = data;
  elem->key           = strdupsafe(key);
  elem->min_abbrev    = strdupsafe(min_abbrev ? min_abbrev : key);
  return elem;
}

void deleteNearMapElem(NEAR_MAP_ELEM *elem) {
  if(elem->key)        free(elem->key);
  if(elem->min_abbrev) free(elem->min_abbrev);
  free(elem);
}

//
// returns the bucket that the key should map into
int get_nearmap_bucket(const char *key) {
  if(isalpha(*key))
    return 1 + tolower(*key) - 'a';
  else
    return 0;
}

int nearmapsortbycmp(NEAR_MAP_ELEM *elem1, NEAR_MAP_ELEM *elem2) {
  return strcasecmp(elem1->min_abbrev, elem2->min_abbrev);
}

int is_near_map_abbrev(const char *abbrev, NEAR_MAP_ELEM *elem) {
  return strncasecmp(abbrev, elem->key, strlen(abbrev));
}

int is_near_map_elem(const char *key, NEAR_MAP_ELEM *elem) {
  return strcasecmp(key, elem->key);
}



//*****************************************************************************
// implementation of near_map.h
//*****************************************************************************
NEAR_MAP *newNearMap(void) {
  NEAR_MAP *map = calloc(1, sizeof(NEAR_MAP));
  return map;
}

void deleteNearMap(NEAR_MAP *map) {
  int i;
  for(i = 0; i < NUM_NEAR_MAP_BUCKETS; i++)
    if(map->bucket[i] != NULL)
      deleteListWith(map->bucket[i], deleteNearMapElem);
  free(map);
}

void *nearMapGet(NEAR_MAP *map, const char *key, bool abbrev_ok) {
  // find the bucket
  LIST *buck = map->bucket[get_nearmap_bucket(key)];
  if(buck == NULL)
    return NULL;
  else {
    NEAR_MAP_ELEM *elem = NULL;
    if(abbrev_ok)
      elem = listGetWith(buck, key, is_near_map_abbrev);
    else
      elem = listGetWith(buck, key, is_near_map_elem);
    return (elem ? elem->data : NULL);
  }
}

void nearMapPut(NEAR_MAP *map, const char *key, const char *min_abbrev, 
		void *elem) {
  // find the bucket
  int buck_num = get_nearmap_bucket(key);
  LIST   *buck = map->bucket[buck_num];
  if(buck == NULL)
    map->bucket[buck_num] = buck = newList();

  NEAR_MAP_ELEM *e = newNearMapElem(elem, key, min_abbrev);
  listPutWith(buck, e, nearmapsortbycmp);
}

bool nearMapKeyExists(NEAR_MAP *map, const char *key) {
  return (nearMapGet(map, key, TRUE) != NULL);
}

void *nearMapRemove(NEAR_MAP *map, const char *key) {
  LIST *buck = map->bucket[get_nearmap_bucket(key)];
  if(buck == NULL)
    return NULL;
  else {
    NEAR_MAP_ELEM *elem = listRemoveWith(buck, key, is_near_map_elem);
    void          *data = NULL;
    if(elem != NULL) {
      data = elem->data;
      deleteNearMapElem(elem);
    }
    return data;
  }
}

LIST *nearMapGetAllMatches(NEAR_MAP *map, const char *key) {
  // find the bucket
  LIST    *buck = map->bucket[get_nearmap_bucket(key)];
  if(buck == NULL)
    return NULL;
  else {
    LIST         *matches = newList();
    LIST_ITERATOR *elem_i = newListIterator(buck);
    NEAR_MAP_ELEM   *elem = NULL;
    ITERATE_LIST(elem, elem_i) {
      if(startswith(elem->key, key))
	// changed to return keys instead of vals, 
	// so this is a little more intuitive (if not a little slower)
	listQueue(matches, strdup(elem->key)); // elem->data);
    } deleteListIterator(elem_i);

    // did we not find anything?
    if(listSize(matches) == 0) {
      deleteList(matches);
      matches = NULL;
    }

    return matches;
  }
}

// how big are we?
int nearMapSize(NEAR_MAP *map) {
  int size = 0, i;
  for(i = 0; i < NUM_NEAR_MAP_BUCKETS; i++) {
    if(map->bucket[i] != NULL)
      size += listSize(map->bucket[i]);
  }
  return size;
}



//*****************************************************************************
// implementation of near map iterator
//*****************************************************************************
NEAR_ITERATOR *newNearIterator(NEAR_MAP *map) {
  NEAR_ITERATOR *iter = calloc(1, sizeof(NEAR_ITERATOR));
  iter->map           = map;
  nearIteratorReset(iter);
  return iter;
}

void deleteNearIterator(NEAR_ITERATOR *iter) {
  if(iter->curr_i)
    deleteListIterator(iter->curr_i);
  free(iter);
}

void nearIteratorReset(NEAR_ITERATOR *iter) {
  int i;

  if(iter->curr_i)
    deleteListIterator(iter->curr_i);
  iter->curr_i = NULL;
  iter->curr_buck = 0;

  // curr_i will be NULL if there are no elems
  for(i = 0; i < NUM_NEAR_MAP_BUCKETS; i++) {
    if(iter->map->bucket[i] != NULL && listSize(iter->map->bucket[i]) > 0) {
      iter->curr_i = newListIterator(iter->map->bucket[i]);
      iter->curr_buck = i;
      break;
    }
  }
}

void nearIteratorNext(NEAR_ITERATOR *iter) {
  // no more elements left
  if(iter->curr_i == NULL)
    return;
  // we're at the end of our list
  else if(listIteratorNext(iter->curr_i) == NULL) {
    deleteListIterator(iter->curr_i);
    iter->curr_i = NULL;
    iter->curr_buck++;

    for(; iter->curr_buck < NUM_NEAR_MAP_BUCKETS; iter->curr_buck++) {
      if(iter->map->bucket[iter->curr_buck] != NULL &&
	 listSize(iter->map->bucket[iter->curr_buck]) > 0) {
	iter->curr_i = newListIterator(iter->map->bucket[iter->curr_buck]);
	break;
      }
    }
  }
}

const char *nearIteratorCurrentKey(NEAR_ITERATOR *iter) {
  if(iter->curr_i == NULL)
    return NULL;
  else {
    NEAR_MAP_ELEM *elem = listIteratorCurrent(iter->curr_i);
    return (elem ? elem->key : NULL);
  }
}

const char *nearIteratorCurrentAbbrev(NEAR_ITERATOR *iter) {
  if(iter->curr_i == NULL)
    return NULL;
  else {
    NEAR_MAP_ELEM *elem = listIteratorCurrent(iter->curr_i);
    return (elem ? elem->min_abbrev : NULL);
  }
}

void *nearIteratorCurrentVal(NEAR_ITERATOR *iter) {
  if(iter->curr_i == NULL)
    return NULL;
  else {
    NEAR_MAP_ELEM *elem = listIteratorCurrent(iter->curr_i);
    return (elem ? elem->data : NULL);
  }
}
