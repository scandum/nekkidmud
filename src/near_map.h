#ifndef NEAR_MAP_H
#define NEAR_MAP_H
//*****************************************************************************
//
// near_map.h
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

typedef struct near_map           NEAR_MAP;
typedef struct near_iterator NEAR_ITERATOR;

NEAR_MAP        *newNearMap(void);
void          deleteNearMap(NEAR_MAP *map);
void            *nearMapGet(NEAR_MAP *map, const char *key, bool abbrev_ok);
void             nearMapPut(NEAR_MAP *map, const char *key, 
			    const char *min_abbrev, void *data);
bool       nearMapKeyExists(NEAR_MAP *map, const char *key);
void         *nearMapRemove(NEAR_MAP *map, const char *key);
LIST  *nearMapGetAllMatches(NEAR_MAP *map, const char *key);
int             nearMapSize(NEAR_MAP *map);



//*****************************************************************************
// an iterator for going over all entries in a near-map
//*****************************************************************************

// iterate across all the elements in a near map
#define ITERATE_NEARMAP(abbrev, val, it) \
  for(abbrev = nearIteratorCurrentAbbrev(it), val = nearIteratorCurrentVal(it);\
      abbrev != NULL; \
      nearIteratorNext(it), \
      abbrev = nearIteratorCurrentAbbrev(it), val = nearIteratorCurrentVal(it))

NEAR_ITERATOR        *newNearIterator(NEAR_MAP *map);
void               deleteNearIterator(NEAR_ITERATOR *I);
void                nearIteratorReset(NEAR_ITERATOR *I);
void                 nearIteratorNext(NEAR_ITERATOR *I);
const char    *nearIteratorCurrentKey(NEAR_ITERATOR *I);
const char *nearIteratorCurrentAbbrev(NEAR_ITERATOR *I);
void          *nearIteratorCurrentVal(NEAR_ITERATOR *I);

#endif // NEAR_MAP_H
