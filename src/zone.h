#ifndef __ZONE_H
#define __ZONE_H
//*****************************************************************************
//
// zone.h
//
// A zone is like a stand-alone module, or encounter, within the game (e.g.
// a city in the world), and contains all of the NPCs, objects, rooms, scripts,
// etc... needed for the zone to be complete.
//
//*****************************************************************************


//
// Create a new zone, with vnums bounded between (inclusive) min and max
ZONE_DATA *newZone(const char *key);

//
// Delete a zone, plus all of the prototypes contained within.
// CONSIDERATION: Should we delete rooms as well, and deposit
//                players into some sort of limbo, or should
//                we leave rooms as-is? I think we should probably
//                delete the rooms, and deposit the players in a
//                room that is known to always exist (limbo?)
void deleteZone(ZONE_DATA *zone);

//
// Load a zone from disk. 
ZONE_DATA *zoneLoad(WORLD_DATA *world, const char *key);

//
// Save a zone to the specified directory path
bool zoneSave(ZONE_DATA *zone);

//
// Pulse a zone. i.e. decrement it's reset timer. When the timer hits 0,
// set it back to the max, and reset everything in the zone
void      zonePulse(ZONE_DATA *zone);
void zoneForceReset(ZONE_DATA *zone);

//
// Copy zone-specific data, but not contents in the zone (no rooms, mobs
// objs, scripts, etc)
ZONE_DATA *zoneCopy(ZONE_DATA *zone);
void     zoneCopyTo(ZONE_DATA *from, ZONE_DATA *to);



//*****************************************************************************
// get and set functions for zones
//*****************************************************************************

//
// various get functions for zones
int      zoneGetPulseTimer(ZONE_DATA *zone);
int           zoneGetPulse(ZONE_DATA *zone);
WORLD_DATA   *zoneGetWorld(ZONE_DATA *zone);
const char    *zoneGetName(ZONE_DATA *zone);
const char    *zoneGetDesc(ZONE_DATA *zone);
const char *zoneGetEditors(ZONE_DATA *zone);
BUFFER  *zoneGetDescBuffer(ZONE_DATA *zone);
void *zoneGetAuxiliaryData(const ZONE_DATA *zone, char *name);
LIST    *zoneGetResettable(ZONE_DATA *zone);


//
// various set functions for zones
void zoneSetPulseTimer(ZONE_DATA *zone, int timer);
void      zoneSetPulse(ZONE_DATA *zone, int pulse_left);
void      zoneSetWorld(ZONE_DATA *zone, WORLD_DATA *world);
void       zoneSetName(ZONE_DATA *zone, const char *name);
void       zoneSetDesc(ZONE_DATA *zone, const char *description);
void    zoneSetEditors(ZONE_DATA *zone, const char *names);


//
// stuff for editing different prototypes stored in zones
void        zoneSetKey(ZONE_DATA *zone, const char *key);
const char *zoneGetKey(ZONE_DATA *zone);
void      *zoneGetType(ZONE_DATA *zone, const char *type, const char *key);
void   *zoneRemoveType(ZONE_DATA *zone, const char *type, const char *key);
void      zoneSaveType(ZONE_DATA *zone, const char *type, const char *key);
void       zonePutType(ZONE_DATA *zone, const char *type, const char *key,
		       void *data);
void       zoneAddType(ZONE_DATA *zone, const char *type, void *reader,
		       void *storer, void *deleter, void *keysetter);
LIST  *zoneGetTypeKeys(ZONE_DATA *zone, const char *type);

//
// some types can 'forget' what they are. This is a fudge so Python can add
// types to zones, and we can do a lookup on the functions that need to
// be called, without having to explicitly deal with Python in zone.c.
// Forgetful functions are exactly the same as normal functions, except they
// take an additional initial argument, which is a const string of their type
void zoneAddForgetfulType(ZONE_DATA *zone, const char *type, void *reader,
			  void *storer, void *deleter, void *keysetter);

#endif // __ZONE_H
