#ifndef __WORLD_H
#define __WORLD_H
//*****************************************************************************
//
// world.h
//
// This is the interface for working with the world structure.
//
//*****************************************************************************

//
// Create a new, empty world. To load in zone data, the world's 
// path must be set, and then a call to worldInit
WORLD_DATA *newWorld(void);

//
// Delete the world from memory.
void deleteWorld(WORLD_DATA *world);

//
// Saves the world to disk at the specified directory path
bool worldSave(WORLD_DATA *world, const char *dirpath);

//
// Initializes the world. This includes any stuff that might need to be done
// when the world first starts up. e.g. reading in zones, or running startup
// scripts
void worldInit(WORLD_DATA *world);

//
// Pulse all of the zones in the world
void worldPulse(WORLD_DATA *world);
void worldForceReset(WORLD_DATA *world);

//
// new world interface
void    *worldGetType(WORLD_DATA *world, const char *type, const char *key);
void *worldRemoveType(WORLD_DATA *world, const char *type, const char *key);
void    worldSaveType(WORLD_DATA *world, const char *type, const char *key);
void     worldPutType(WORLD_DATA *world, const char *type, const char *key,
		      void *data);
void     worldAddType(WORLD_DATA *world, const char *type, void *reader,
		      void *storer, void *deleter, void *typesetter);

//
// some types can 'forget' what they are. This is a fudge so Python can add
// types to the world, and we can do a lookup on the functions that need to
// be called, without having to explicitly deal with Python in world.c.
// Forgetful functions are exactly the same as normal functions, except they
// take an additional initial argument, which is a const string of their type
void worldAddForgetfulType(WORLD_DATA *world, const char *type, void *reader,
			   void *storer, void *deleter, void *typesetter);

ZONE_DATA *worldRemoveZone(WORLD_DATA *world, const char *key);
ROOM_DATA    *worldGetRoom(WORLD_DATA *world, const char *key);
ROOM_DATA *worldRemoveRoom(WORLD_DATA *world, const char *key);
bool       worldRoomLoaded(WORLD_DATA *world, const char *key);
void          worldPutRoom(WORLD_DATA *world, const char *key, ROOM_DATA *room);

void            worldPutZone(WORLD_DATA *world, ZONE_DATA *zone);
ZONE_DATA      *worldGetZone(WORLD_DATA *world, const char *key);
LIST       *worldGetZoneKeys(WORLD_DATA *world);
const char *worldGetZonePath(WORLD_DATA *world, const char *key);
void            worldSetPath(WORLD_DATA *world, const char *path);
const char     *worldGetPath(WORLD_DATA *world);

#endif // __WORLD_H
