#ifndef FURNITURE_H
#define FURNITURE_H
//*****************************************************************************
//
// furniture.h
//
// handles all of the functioning of the furniture item type. Stores data about 
// the capacity of the furniture, and what type of furniture it is. 
//
//*****************************************************************************

#define FURNITURE_NONE       -1
#define FURNITURE_AT          0
#define FURNITURE_ON          1
#define NUM_FURNITURES        2

int  furnitureGetCapacity(OBJ_DATA *obj);
void furnitureSetCapacity(OBJ_DATA *obj, int capacity);
int  furnitureGetType(OBJ_DATA *obj);
void furnitureSetType(OBJ_DATA *obj, int type);

const char *furnitureTypeGetName(int type);
int  furnitureTypeGetNum(const char *type);

#endif // FURNITURE_H
