//*****************************************************************************
//
// list.c v1.1
//
// the implementation of a generic list structure.
//
// Jan 15/04:
//   Removing elements from the list while we're iterating over it can be very
// dangerous. The latest revision of the list keeps track of how many iterators
// are currently running overtop of us, and only actually removes items from
// the list until the iterator count goes down to 0; until then, the items are
// flagged as removed so they are not touched.
//
//*****************************************************************************

#include <stdlib.h>
#include <stdarg.h>
#include "list.h"

#ifndef FALSE
#define FALSE   0
#define TRUE    !(FALSE)
#endif

typedef struct list_node {
  void *elem;              // the data we contain
  struct list_node *next;  // the next node in the list
  char removed;            // has the item been removed from the list? 
                           // char == bool
} LIST_NODE;

struct list_iterator {
  struct list      *L;     // the list we're iterating over
  LIST_NODE *curr;         // the current element we're iterating on
};

struct list {
  LIST_NODE *head;         // first element in the list
  LIST_NODE *tail;         // last element in the list
  int size;                // how many elements are in the list?
  int iterators;           // how many iterators are going over us?
  char remove_pending;     // do we have to do a remove when the iterators die?
};


//
// Delete a list node, and all nodes attached to it
//
void deleteListNode(LIST_NODE *N) {
  if(N->next) deleteListNode(N->next);
  free(N);
};

//
// delete the list node, plus it's element using the supplied function
//
void deleteListNodeWith(LIST_NODE *N, void (*delete_func)(void *)) {
  if(N->next) deleteListNodeWith(N->next, delete_func);
  // we only want to delete elements that are actually in the list
  if(!N->removed)
    delete_func(N->elem);
  free(N);
}

//
// Create a new list node containing the given element
//
LIST_NODE *newListNode(void *elem) {
  LIST_NODE *N = malloc(sizeof(LIST_NODE));
  N->elem    = elem;
  N->next    = NULL;
  N->removed = FALSE;
  return N;
};


//
// take out all of the nodes that have been flagged as "removed"
// from the list.
//
void listCleanRemoved(LIST *L) {
  // go through and kill all of the elements we removed if removes are pending
  if(L->remove_pending) {
    LIST_NODE *node = L->head;
    L->remove_pending = FALSE;

    // while our head is a removed element, 
    // pop it off and delete the list node
    while(node && node->removed) {
      L->head = node->next;
      node->next = NULL;
      deleteListNode(node);
      node = L->head;
    }
    
    // go through the rest of the list
    if(node != NULL) {
      while(node->next != NULL) {
	// is our next element to be removed?
	if(node->next->removed) {
	  LIST_NODE *removed = node->next;
	  node->next = removed->next;
	  removed->next = NULL;
	  deleteListNode(removed);
	  // if we just removed the last element in the list, our next 
	  // node should be NULL and we cannot keep do our next loop, 
	  // as we'll try to assign NULL to the current node
	  if(node->next == NULL) break;
	}
	node = node->next;
      }
    }
    
    // reset the tail of our list
    L->tail = node;
    
    // if we have no elements left, clear our head and tail
    if(L->size == 0)
      L->head = L->tail = NULL;
  }
}



//*****************************************************************************
//
// List interface functions. Documentation in list.h
//
//*****************************************************************************
LIST *newList() {
  LIST *L    = malloc(sizeof(LIST));
  L->head           = NULL;
  L->tail           = NULL;
  L->size           = 0;
  L->iterators      = 0;
  L->remove_pending = FALSE;
  return L;
};


void deleteList(LIST *L) {
  if(L->head) deleteListNode(L->head);
  free(L);
};

void deleteListWith(LIST *L, void *func) {
  if(L->head) deleteListNodeWith(L->head, func);
  free(L);
}

void listPut(LIST *L, void *elem) {
  //  if(listIn(L, elem))
  //    return;

  LIST_NODE *N = newListNode(elem);
  N->next = L->head;
  L->head = N;
  L->size++;
  if(L->tail == NULL)
    L->tail = N;
};


void listQueue(LIST *L, void *elem) {
  //  if(listIn(L, elem))
  //    return;

  LIST_NODE *N = newListNode(elem);

  if(L->head == NULL) {
    L->head = N;
    L->tail = N;
  }
  else {
    L->tail->next = N;
    L->tail       = N;
  }
  L->size++;
}


void *listPop(LIST *L) {
  return listRemoveNum(L, 0);
};


void listPush(LIST *L, void *elem) {
  listPut(L, elem);
};


int listIn(LIST *L, const void *elem) {
  LIST_NODE *N = L->head;

  while(N != NULL) {
    if(!N->removed && N->elem == elem)
      return TRUE;
    N = N->next;
  }

  return FALSE;
};


int listRemove(LIST *L, const void *elem) {
  LIST_NODE *N = L->head;

  // we don't have any contents
  if(N == NULL)
    return FALSE;

  // we first have to check if it is the head
  if(!N->removed && N->elem == elem) {
    // check to see if there's an iterator on us
    if(L->iterators > 0) {
      N->removed = TRUE;
      L->size--;
      L->remove_pending = TRUE;
      return TRUE;
    }
    else {
      L->head = N->next;
      N->next = NULL;
      deleteListNode(N);
      L->size--;
      if(L->size == 0)
	L->tail = NULL;
      return TRUE;
    }
  }
  // otherwise, we can check if it's another element
  else {
    while(N->next) {
      // we found it ... remove it now
      if(!N->next->removed && N->next->elem == elem) {
	// check to see if there's an iterator on us
	if(L->iterators > 0) {
	  N->next->removed = TRUE;
	  L->remove_pending = TRUE;
	  L->size--;
	  return TRUE;
	}
	else {
	  if(N->next == L->tail)
	    L->tail = N;
	  LIST_NODE *tmp = N->next;
	  N->next = tmp->next;
	  tmp->next = NULL;
	  deleteListNode(tmp);
	  L->size--;
	  return TRUE;
	}
      }
      N = N->next;
    }
  }
  // we didn't find it
  return FALSE;
};

void *listRemoveNum(LIST *L, unsigned int num) {
  void *elem = listGet(L, num);
  if(elem) listRemove(L, elem);
  return elem;
}

int listSize(LIST *L) {
  return L->size;
}

void *listGet(LIST *L, unsigned int num) {
  LIST_NODE *node = L->head;
  unsigned int i;

  // move up to our first non-removed node
  while(node && node->removed)
    node = node->next;

  for(i = 0; i < num && node; node = node->next) {
    if(node->removed)
      continue;
    i++;
  }

  return (node ? node->elem : NULL);
}

void *listHead(LIST *L) {
  return listGet(L, 0);
}

void *listTail(LIST *L) {
  return listGet(L, L->size-1);
}

int isListEmpty(LIST *L) {
  return (L->size == 0);
}

void *listGetWith(LIST *L, const void *cmpto, void *func) {
  int (* comparator)(const void *, const void *) = func;
  LIST_NODE *node = L->head;

  for(node = L->head; node != NULL; node = node->next) {
    if(node->removed)
      continue;
    if(!comparator(cmpto, node->elem))
      return node->elem;
  }
  return NULL;
}

void *listRemoveWith(LIST *L, const void *cmpto, void *func) {
  int (* comparator)(const void *, const void *) = func;
  LIST_NODE *N = L->head;

  // we don't have any contents
  if(N == NULL)
    return NULL;

  // we first have to check if it is the head
  if(!N->removed && !comparator(cmpto, N->elem)) {
    // check to see if there's an iterator on us
    if(L->iterators > 0) {
      N->removed = TRUE;
      L->remove_pending = TRUE;
      L->size--;
      return N->elem;
    }
    else {
      void *elem = N->elem;
      L->head = N->next;
      N->next = NULL;
      deleteListNode(N);
      L->size--;
      if(L->size == 0)
	L->tail = NULL;
      return elem;
    }
  }

  // otherwise, we can check if it's another element
  else {
    while(N->next) {
      // we found it ... remove it now
      if(!N->next->removed && !comparator(cmpto, N->next->elem)) {
	// check to see if there's an iterator on us
	if(L->iterators > 0) {
	  N->next->removed = TRUE;
	  L->remove_pending = TRUE;
	  L->size--;
	  return N->next->elem;
	}
	else {
	  void *elem = N->next->elem;
	  if(N->next == L->tail)
	    L->tail = N;
	  LIST_NODE *tmp = N->next;
	  N->next = tmp->next;
	  tmp->next = NULL;
	  deleteListNode(tmp);
	  L->size--;
	  return elem;
	}
      }
      N = N->next;
    }
  }
  // we didn't find it
  return NULL;
}


void listPutWith(LIST *L, void *elem, void *func) {
  int (* comparator)(const void *, const void *) = func;
  LIST_NODE *N = L->head;

  // we don't have any contents, or we're lower than the
  // first list content then just put it at the start
  if(N == NULL || (!N->removed && comparator(elem, N->elem) < 0))
    listPut(L, elem);
  else {
    // while we've got a next element, compare ourselves to it.
    // if we are smaller than it, then sneak inbetween. Otherwise,
    // skip to the next element
    while(N->next != NULL) {
      if(!N->next->removed) {
	int val = comparator(elem, N->next->elem);
	// we're less than or equal to it... sneak in
	if(val <= 0) {
	  LIST_NODE *new_node = newListNode(elem);
	  new_node->next = N->next;
	  N->next = new_node;
	  L->size++;
	  return;
	}
      }
      N = N->next;
    }
    // if we've gotten this far, then we need to attach ourself to the end
    N->next = newListNode(elem);
    L->tail = N->next;
    L->size++;
  }
}


void listSortWith(LIST *L, void *func) {
  // make a new list, and just pop our elements
  // into it while we still have 'em
  LIST *new_list = newList();

  while(listSize(L) > 0)
    listPutWith(new_list, listPop(L), func);

  // kill all of our removed nodes
  if(L->head) deleteListNode(L->head);

  L->head = new_list->head;
  L->tail = new_list->tail;
  L->size = new_list->size;
  // FREE ... not delete. Delete would kill the things
  // we just transferred over to the old list
  free(new_list);
}


LIST *listCopyWith(LIST *L, void *func) {
  void *(* copy_func)(void *) = func;
  LIST *newlist = newList();

  LIST_NODE *N = NULL;
  for(N = L->head; N; N = N->next) {
    if(N->removed) continue;
    listQueue(newlist, copy_func(N->elem));
  }

  return newlist;
}

void listParse(LIST *L, int n, ...) {
  int i;
  va_list args;
  va_start(args, n);
  for(i = 0; i < n; i++)
    *va_arg(args, void **) = listGet(L, i);
  va_end(args);
}



//*****************************************************************************
//
// The functions for the list iterator interface. Documentation is in list.h
//
//*****************************************************************************
LIST_ITERATOR *newListIterator(LIST *L) {
  LIST_ITERATOR *I = malloc(sizeof(LIST_ITERATOR));
  I->L    = L;
  I->curr = I->L->head;
  L->iterators++;
  return I;
};

void deleteListIterator(LIST_ITERATOR *I) {
  I->L->iterators--;
  // if we're at 0 iterators, clean the list of all removed elements
  if(I->L->iterators == 0)
    listCleanRemoved(I->L);
  free(I);
};

void *listIteratorNext(LIST_ITERATOR *I) {
  if(I->curr)
    I->curr = I->curr->next;

  // skip all of the removed elements
  while(I->curr && I->curr->removed)
    I->curr = I->curr->next;

  return (I->curr ? I->curr->elem : NULL);
};

// hmmm... we should really kill this function. 
// Just let 'em create a new iterator
void listIteratorReset(LIST_ITERATOR *I) {
  // if we're the only iterator, take this opportunity to clean the list
  if(I->L->iterators == 1)
    listCleanRemoved(I->L);

  I->curr = I->L->head;
};

void *listIteratorCurrent(LIST_ITERATOR *I) {
  // hmmm... what if we're on a removed node?
  while(I->curr && I->curr->removed)
    I->curr = I->curr->next;

  return (I->curr ? I->curr->elem : NULL);
};
