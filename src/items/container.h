#ifndef CONTAINER_H
#define CONTAINER_H
//*****************************************************************************
//
// container.h
//
// handles all of the functioning of the container item type. Stores data about 
// the contents and capacity of a container, and all the functions for 
// interacting with container types.
//
//*****************************************************************************

double containerGetCapacity(OBJ_DATA *obj);
bool    containerIsClosable(OBJ_DATA *obj);
bool      containerIsClosed(OBJ_DATA *obj);
bool      containerIsLocked(OBJ_DATA *obj);
const char *containerGetKey(OBJ_DATA *obj);
int    containerGetPicKDiff(OBJ_DATA *obj);
void   containerSetCapacity(OBJ_DATA *obj, double capacity);
void     containerSetClosed(OBJ_DATA *obj, bool closed);
void   containerSetClosable(OBJ_DATA *obj, bool closable);
void     containerSetLocked(OBJ_DATA *obj, bool locked);
void        containerSetKey(OBJ_DATA *obj, const char *key);
void   containerSetPickDiff(OBJ_DATA *obj, int  diff);

#endif // CONTAINER_H
