#ifndef ITEMS_H
#define ITEMS_H
//*****************************************************************************
//
// items.h
//
// Objects can serve many different functions; they might be containers, they
// might be weapons, they might be pieces of clothing... the list goes on and
// on. The classification of what type of item the object is is what handles
// these sorts of distinctions. This is a general framework for adding new
// item types. Contains functions for storing type data on objects. Item types
// are expected to be able to handle their own subtyping (if neccessary). 
// Objects can be multi-typed (e.g. a desk with a drawer can be both a piece
// of furniture and a container).
//
//*****************************************************************************

//
// prepare the items module for use
void init_items(void);

//
// install a new item type. type should be the name of the item type.
//
// new should create a new datastructure to hold data for the object assocciated
// with its item type. It should take no arguments. The form of the function
// should be as follows:
//    itemdata *newItemData()
//
// delete should take in the datastructure used for holding data assocciated
// with the item type, and free it all from memory. The function should take
// a form as follows:
//    void deleteItemData(itemdata *data)
//
// copyTo should take a pointer from one itemdata and copy all of its
// information over to a pointer to another itemdata. The function should take
// a form as follows:
//    void itemDataCopyTo(itemdata *from, itemdata *to)
//
// copy should return a deep copy (newly allocated memory) of the supplied
// itemdata. The function should take a form as follows:
//    itemdata *itemDataCopy(itemdata *data)
//
// store should return a storage set for storing data assocciated with the
// item type for the object to disk. The function should take a form as
// follows:
//    STORAGE_SET *itemDataStore(itemdata *data)
//
// read should take in a storage set and, from it, parse a new instance
// of itemdata. The function should take a form as follows:
//    itemdata *itemDataRead(STORAGE_SET *set)
void item_add_type(const char *type, 
		   void *newfunc,void *deleter,
		   void *copyTo, void *copy, 
		   void *store,  void *read);

//
// Build a list of all the current item types. Item types are listed in
// alphabetical order. This list and all its contents must be deleted after 
// use. For a quick one-line deletion, try: 
//   deleteListWith(list, free)
LIST *itemTypeList(void);

//
// returns the item data with the specified type for the object. 
// if the object is not of type item, return NULL.
void *objGetTypeData(OBJ_DATA *obj, const char *type);

//
// Set the item to be of the specified type. 
// A new datastructure for the type data is created.
void objSetType(OBJ_DATA *obj, const char *type);

//
// If an object was of the specified type, no longer have the object
// be of that type.
void objDeleteType(OBJ_DATA *obj, const char *type);

//
// returns TRUE if the object is the specified type of object
bool objIsType(OBJ_DATA *obj, const char *type);

//
// returns a comma-separated list of the item types this object currently
// has. the list is written to a static buffer, so players don't have to
// worry about freeing the string after use
const char *objGetTypes(OBJ_DATA *obj);

#endif // ITEMS_H
