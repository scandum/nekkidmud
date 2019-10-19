#ifndef SET_H
#define SET_H
//*****************************************************************************
//
// set.h
//
// a non-ordered container that has constant lookup time.
//
//*****************************************************************************

typedef struct set_data                   SET;
typedef struct set_iterator               SET_ITERATOR;

SET  *newSet         (void);
void  deleteSet      (SET *set);
void  setPut         (SET *set, void *elem);
void *setRemove      (SET *set, void *elem);
int   setIn          (SET *set, const void *elem);
int   setSize        (SET *set);
LIST *setCollect     (SET *set);
SET  *setCopy        (SET *set);
SET  *setUnion       (SET *set1, SET *set2);
SET  *setIntersection(SET *set1, SET *set2);
void setChangeHashing(SET *set, void *cmp_func, void *hash_func);



//*****************************************************************************
//
// set iterator
//
// we may sometimes want to iterate across all of the elements in a set.
// this lets us do so.
//
//*****************************************************************************

// iterate across all the elements in a set
#define ITERATE_SET(elem, it) \
  for(elem = setIteratorCurrent(it); \
      elem != NULL; \
      setIteratorNext(it), elem = setIteratorCurrent(it))


SET_ITERATOR *newSetIterator    (SET *S);
void          deleteSetIterator (SET_ITERATOR *I);
void          setIteratorReset  (SET_ITERATOR *I);
void         *setIteratorNext   (SET_ITERATOR *I);
void         *setIteratorCurrent(SET_ITERATOR *I);

#endif // SET_H
