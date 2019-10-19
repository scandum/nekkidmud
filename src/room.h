#ifndef __ROOM_H
#define __ROOM_H
//*****************************************************************************
//
// room.h
//
// the basic interface for working with the room structure.
//
// the basic interface for working with the room data structure. If you plan on
// adding any other information to rooms, it is strongly suggested you do so
// through auxiliary data (see auxiliary.h)
//
// For a recap, IF YOU PLAN ON ADDING ANY OTHER INFORMATION TO ROOMS, IT
// IS STRONGLY SUGGESTED YOU DO SO THROUGH AUXILIARY DATA (see auxiliary.h).
//
//*****************************************************************************

#define DIR_NONE             (-1)
#define DIR_NORTH               0
#define DIR_EAST                1
#define DIR_SOUTH               2
#define DIR_WEST                3
#define DIR_UP                  4
#define DIR_DOWN                5
#define DIR_NORTHEAST           6
#define DIR_SOUTHEAST           7
#define DIR_SOUTHWEST           8
#define DIR_NORTHWEST           9
#define NUM_DIRS               10

#define TERRAIN_NONE         (-1)
#define TERRAIN_INDOORS         0	
#define TERRAIN_CITY            1	
#define TERRAIN_ROAD            2
#define TERRAIN_ALLEY           3
#define TERRAIN_BRIDGE          4 
#define TERRAIN_SHALLOW_WATER   5	
#define TERRAIN_DEEP_WATER      6	
#define TERRAIN_OCEAN           7
#define TERRAIN_UNDERWATER	8	
#define TERRAIN_FIELD           9	
#define TERRAIN_PLAINS         10 
#define TERRAIN_MEADOW         11
#define TERRAIN_FOREST         12
#define TERRAIN_DEEP_FOREST    13 
#define TERRAIN_HILLS          14
#define TERRAIN_HIGH_HILLS     15
#define TERRAIN_MOUNTAIN       16
#define TERRAIN_SWAMP          17 
#define TERRAIN_DEEP_SWAMP     18
#define TERRAIN_SAND           19 
#define TERRAIN_DESERT         20
#define TERRAIN_ICE            21
#define TERRAIN_GLACIER        22
#define TERRAIN_CAVERN         23
#define NUM_TERRAINS           24


//
// Create a clean, new room
ROOM_DATA *newRoom();


//
// Delete a room, and detach it from all of its dependant structures
void deleteRoom(ROOM_DATA *room);


//
// make a copy of the room
ROOM_DATA *roomCopy(ROOM_DATA *room);


//
// Copy one room to another
void roomCopyTo(ROOM_DATA *from, ROOM_DATA *to);


//
// parse and store room data
ROOM_DATA    *roomRead(STORAGE_SET *set);
STORAGE_SET *roomStore(ROOM_DATA *room);

bool           roomIsInstance(ROOM_DATA *room, const char *prototype);
const char *roomGetPrototypes(ROOM_DATA *room);
void         roomAddPrototype(ROOM_DATA *room, const char *prototype);
void        roomSetPrototypes(ROOM_DATA *room, const char *prototypes);



//*****************************************************************************
// add and remove functions
//*****************************************************************************
void       roomRemoveChar     (ROOM_DATA *room, const CHAR_DATA *ch);
void       roomRemoveObj      (ROOM_DATA *room, const OBJ_DATA *obj);

void       roomAddChar        (ROOM_DATA *room, CHAR_DATA *ch);
void       roomAddObj         (ROOM_DATA *room, OBJ_DATA *obj);



//*****************************************************************************
// get and set functions for rooms
//*****************************************************************************
const char *roomGetName        (const ROOM_DATA *room);
const char *roomGetDesc        (const ROOM_DATA *room);
int         roomGetTerrain     (const ROOM_DATA *room);
BUFFER     *roomGetDescBuffer  (const ROOM_DATA *room);

void           roomSetExit(ROOM_DATA *room, const char *dir, EXIT_DATA *exit);
EXIT_DATA     *roomGetExit(ROOM_DATA *room, const char *dir);
EXIT_DATA  *roomRemoveExit(ROOM_DATA *room, const char *dir);
const char *roomGetExitDir(ROOM_DATA *room, EXIT_DATA *exit);
LIST     *roomGetExitNames(ROOM_DATA *room);

EDESC_SET  *roomGetEdescs       (const ROOM_DATA *room);
const char *roomGetEdesc        (const ROOM_DATA *room, const char *keyword);
void       *roomGetAuxiliaryData(const ROOM_DATA *room, const char *name);
LIST       *roomGetCharacters   (const ROOM_DATA *room);
LIST       *roomGetContents     (const ROOM_DATA *room);
BITVECTOR  *roomGetBits         (const ROOM_DATA *room);
int         roomGetUID          (const ROOM_DATA *room);
time_t      roomGetBirth        (const ROOM_DATA *room);
NEAR_MAP   *roomGetCmdTable     (const ROOM_DATA *room);
bool        roomHasCmds         (const ROOM_DATA *room);
bool        roomHasCmd          (const ROOM_DATA *room, const char *name);
void        roomAddCmd          (ROOM_DATA *room, const char *name, 
				 const char *abbr, CMD_DATA *cmd);
CMD_DATA   *roomRemoveCmd       (ROOM_DATA *room, const char *name);
CMD_DATA   *roomGetCmd          (ROOM_DATA *room, const char *name, 
				 bool abbr_ok);


const char *roomGetClass       (ROOM_DATA *room);
void        roomSetClass       (ROOM_DATA *room, const char *prototype);

void        roomSetEdescs      (ROOM_DATA *room, EDESC_SET *edescs);
void        roomSetName        (ROOM_DATA *room, const char *name);
void        roomSetDesc        (ROOM_DATA *room, const char *desc);
void        roomSetTerrain     (ROOM_DATA *room, int terrain_type);
void        roomSetExtracted   (ROOM_DATA *room);
bool        roomIsExtracted    (ROOM_DATA *room);



//*****************************************************************************
// direction stuff
//*****************************************************************************
const char *dirGetName(int dir);
const char *dirGetAbbrev(int dir);
int dirGetOpposite(int dir);

int dirGetNum(const char *dir);
int dirGetAbbrevNum(const char *dir);



//*****************************************************************************
// terrain stuff
//*****************************************************************************
const char *terrainGetName(int terrain);
int  terrainGetNum(const char *terrain);


#endif // __ROOM_H
