#ifndef PORTAL_H
#define PORTAL_H
//*****************************************************************************
//
// portal.h
//
// handles all of the functioning of the portal item type. Item keyword type
// is "portal". This will be needed for checking if an item is of type portal
// with objIsType() from items/items.h
//
//*****************************************************************************

//
// return the key of the room that the portal leads to
const char      *portalGetDest(OBJ_DATA *obj);
const char *portalGetLeaveMssg(OBJ_DATA *obj);
const char *portalGetEnterMssg(OBJ_DATA *obj);

//
// set the destination of the portal
void      portalSetDest(OBJ_DATA *obj, const char *dest);
void portalSetLeaveMssg(OBJ_DATA *obj, const char *mssg);
void portalSetEnterMssg(OBJ_DATA *obj, const char *mssg);

#endif // PORTAL_H
