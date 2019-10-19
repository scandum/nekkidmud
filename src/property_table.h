#ifndef __PROPERTY_TABLE_H
#define __PROPERTY_TABLE_H
//*****************************************************************************
//
// property_table.h
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

typedef struct property_table             PROPERTY_TABLE;
typedef struct property_table_iterator    PROPERTY_TABLE_ITERATOR;


//
// Create a new property table with the specified number of buckets
// (more buckets = faster lookup, but more memory). The key function must
// be a function that returns a positive integer (no upper bound)
//
PROPERTY_TABLE *newPropertyTable(void *key_function, int buckets);


//
// Delete the property table, but not the elements housed within it.
//
void deletePropertyTable(PROPERTY_TABLE *table);


//
// Put an element in the table. Its key is determined by the
// function we have already passed in upon creating this table.
//
void propertyTablePut(PROPERTY_TABLE *table, void *elem);


//
// Remove and return the value assocciated with this key and our
// key-function. Return NULL if nothing exists.
//
void *propertyTableRemove(PROPERTY_TABLE *table, int key);


//
// Return the value assocciated with the key and our key-function.
// return NULL if nothing exists
//
void *propertyTableGet(PROPERTY_TABLE *table, int key);


//
// Return true if a value with the specified key exists
//
bool propertyTableIn(PROPERTY_TABLE *table, int key);



//*****************************************************************************
//
// property table iterator
//
// we may sometimes want to iterate across all of the elements in a table.
// this lets us do so.
//
//*****************************************************************************

//
// Create an iterator to go over the table
//
PROPERTY_TABLE_ITERATOR *newPropertyTableIterator(PROPERTY_TABLE *T);


//
// Delete the iterator (but not the contents)
//
void deletePropertyTableIterator(PROPERTY_TABLE_ITERATOR *I);


//
// Point the iterator back at the start of the table
//
void propertyTableIteratorReset(PROPERTY_TABLE_ITERATOR *I);


//
// Skip to the next element. Return the next element
// if one exists, and NULL otherwise.
//
void *propertyTableIteratorNext(PROPERTY_TABLE_ITERATOR *I);


//
// return a pointer to the current element
//
void *propertyTableIteratorCurrent(PROPERTY_TABLE_ITERATOR *I);

#endif // __PROPERTY_TABLE_H
