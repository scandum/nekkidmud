#ifndef __HASHTABLE_H
#define __HASHTABLE_H
//*****************************************************************************
//
// hashtable.h
//
// Your friendly neighbourhood hashtable. Maps a <key> to a <value>. *sigh*
// why am I not writing this MUD in C++, again?
//
//*****************************************************************************

typedef struct hashtable                  HASHTABLE;
typedef struct hashtable_iterator         HASH_ITERATOR;

//
// create a new hashtable with the specified number of buckets in it
HASHTABLE *newHashtableSize(int num_buckets);

//
// create a new hashtable. the default number of buckets are used
HASHTABLE *newHashtable(void);

//
// delete the hashtable. Nothing is done to its contents. Table must be cleared
// before being deleted to prevent memory leaks
void deleteHashtable(HASHTABLE *table);

//
// Deletes the hashtable, and also deletes all of its contents with the
// given function. Should take the form: void func(void *data)
void deleteHashtableWith(HASHTABLE *table, void *function);

int   hashPut    (HASHTABLE *table, const char *key, void *val);
void *hashGet    (HASHTABLE *table, const char *key);
void *hashRemove (HASHTABLE *table, const char *key);
int   hashIn     (HASHTABLE *table, const char *key);
int   hashSize   (HASHTABLE *table);

//
// expand a hashtable to the new size. Hashtables will automagically expand
// themselves as needed, but if you know that you are going to need a rather
// large hashtable apriori, you may wish to call this function after the table
// is created, to prevent unneccessary deallocations and reallocations of memory
void hashExpand(HASHTABLE *table, int size);

//
// returns a list of all the keys in the hashtable. The keys and the list
// must be deleted after use. Try: deleteListWith(list, free)
LIST *hashCollect(HASHTABLE *table);

//
// clears all of the contents of the hashtable
void hashClear(HASHTABLE *table);

//
// clears all of the contents of the hashtable, and deletes the vals with the
// specified function. NULL indicates no delection
void hashClearWith(HASHTABLE *table, void *func);



//*****************************************************************************
// prototypes for the hashtable iterator
//*****************************************************************************

// iterate across all the elements in a hashtable
#define ITERATE_HASH(key, val, it) \
  for(key = hashIteratorCurrentKey(it), val = hashIteratorCurrentVal(it); \
      key != NULL; \
      hashIteratorNext(it), \
      key = hashIteratorCurrentKey(it), val = hashIteratorCurrentVal(it))

HASH_ITERATOR *newHashIterator     (HASHTABLE *table);
void        deleteHashIterator     (HASH_ITERATOR *I);

void        hashIteratorReset      (HASH_ITERATOR *I);
void        hashIteratorNext       (HASH_ITERATOR *I);
const char *hashIteratorCurrentKey (HASH_ITERATOR *I);
void       *hashIteratorCurrentVal (HASH_ITERATOR *I);

#endif // __HASHTABLE_H
