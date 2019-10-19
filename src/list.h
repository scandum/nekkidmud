#ifndef __LIST_H
#define __LIST_H
//*****************************************************************************
//
// list.h
//
// the interface for a generic list structure. This list isn't a list in the
// typical way; it can only hold one of any one data at a time (i.e. character
// bob can only exist in the list once). I cannot think of any time in a MUD
// setting where someone would exist in a list more than once, so this list was
// designed with this in mind. If you want to use it otherwise, it should be
// pretty easy to comment out the checks :)
//
//*****************************************************************************

typedef struct list                       LIST;
typedef struct list_iterator              LIST_ITERATOR;

//
// Create a new list
//
LIST *newList();


//
// Delete an existing list
//
void deleteList(LIST *L);


//
// Delete the list. Also delete all of its contents with the function
// that is passed in. The function should take one argument, and should be
// compatible with the type of data in the list
//
void deleteListWith(LIST *L, void *func);


//
// Add an element to the list
//
void listPut(LIST *L, void *elem);


//
// Add an element to the end of the list
//
void listQueue(LIST *L, void *elem);


//
// Return true if the element is in the list. False otherwise
//
int listIn(LIST *L, const void *elem);


//
// Remove all instances of the elem from the list. Return
// true if successful, and false otherwise.
//
int listRemove(LIST *L, const void *elem);


//
// Remove the element in the list at the specified place,
// and return it
//
void *listRemoveNum(LIST *L, unsigned int num);


//
// remove the first element in the list, and return it
//
void *listPop(LIST *L);


//
// add the item to the head of the list
//
void listPush(LIST *L, void *elem);


//
// How many elements does the list have?
//
int listSize(LIST *L);

//
// Is the list empty?
//
int isListEmpty(LIST *L);


//
// get the element with the specific number
//
void *listGet(LIST *L, unsigned int num);


//
// Return the head of the list
//
void *listHead(LIST *L);


//
// Return the tail of the list
//
void *listTail(LIST *L);


//
// Put the element in the list in an ascending order, based on
// what the comparator, func, tells us is the order.
//
void listPutWith(LIST *L, void *elem, void *func);


//
// get an element... use a function that is passed in to find the right element
// the function should return a boolean value (TRUE if we have a match) and
// only take one argument. cmpto must be the thing we are compared against
// in func.
//
void *listGetWith(LIST *L, const void *cmpto, void *func);


//
// Similar to listGetWith, but removes the element permenantly. Returns
// TRUE if the item was found and removed. FALSE other wise. Cmpto must
// be the thing we are compared against in func.
//
void *listRemoveWith(LIST *L, const void *cmpto, void *func);


//
// Sorts the element in a list with the specified comparator function.
// Func takes two arguments, and returns 0 if the two match. -1 is returned
// if the first is less than the second, and 1 otherwise.
//
void listSortWith(LIST *L, void *func);


//
// Make a copy of the list. func is a function that takes one argument (the
// data that is in the list) and returns a copy of that data.
//
LIST *listCopyWith(LIST *L, void *func);


//
// Parses out the first n arguments of the list, and assigns them to the 
// pointers supplied as arguments in the elipsis. Assumes n <= listSize(L)
void listParse(LIST *L, int n, ...);



//*****************************************************************************
// list iterator function prototypes
//*****************************************************************************

// iterate across all the elements in a list
#define ITERATE_LIST(val, it) \
  for(val = listIteratorCurrent(it); val != NULL; val = listIteratorNext(it))

//
// Create an iterator to go over the list
//
LIST_ITERATOR *newListIterator(LIST *L);


//
// Delete the list iterator (but not the contents)
//
void deleteListIterator(LIST_ITERATOR *I);


//
// Point the list iterator back at the head of the list
//
void listIteratorReset(LIST_ITERATOR *I);


//
// Skip to the next element in the list. Return the next element
// if one exists, and NULL otherwise.
//
void *listIteratorNext(LIST_ITERATOR *I);


//
// return a pointer to the current list element
//
void *listIteratorCurrent(LIST_ITERATOR *I);


#endif // __LIST_H
