//*****************************************************************************
//
// room.c
//
// the basic implementation of the room data structure. If you plan on adding
// any other information to rooms, it is strongly suggested you do so through
// auxiliary data (see auxiliary.h)
//
// For a recap, IF YOU PLAN ON ADDING ANY OTHER INFORMATION TO ROOMS, IT
// IS STRONGLY SUGGESTED YOU DO SO THROUGH AUXILIARY DATA (see auxiliary.h).
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "handler.h"
#include "extra_descs.h"
#include "auxiliary.h"
#include "storage.h"
#include "exit.h"
#include "room.h"
#include "character.h"
#include "object.h"

struct room_data {
  int         uid;               // what is our unique room ID number?
  time_t      birth;             // the time we were created
  int         terrain;           // what kind of terrain do we have?
  char       *name;              // what is the name of our room?
  BUFFER     *desc;              // our description

  HASHTABLE  *exits;             // a dir:exit mapping
  NEAR_MAP   *cmd_table;         // a listing for all our room-only commands
  EDESC_SET  *edescs;            // the extra descriptions in the room
  BITVECTOR  *bits;              // the bits we have turned on
  char       *class;             // what prototype do we directly inherit?
  char       *prototypes;        // what prototypes are we instances of?

  LIST       *contents;          // what objects do we contain in the room?
  LIST       *characters;        // who is in our room?

  AUX_TABLE  *auxiliary_data;    // data modules have installed in us

  bool        extracted;         // have we been extracted from the game?
};


//*****************************************************************************
//
// implementation of the room.h interface
//
//*****************************************************************************
ROOM_DATA *newRoom() {
  ROOM_DATA *room = malloc(sizeof(ROOM_DATA));

  room->uid       = next_uid();
  room->birth     = current_time;
  room->prototypes= strdup("");
  room->name      = strdup("");
  room->class     = strdup("");
  room->desc      = newBuffer(1);

  room->terrain = TERRAIN_INDOORS;

  room->bits           = bitvectorInstanceOf("room_bits");
  room->auxiliary_data = newAuxiliaryData(AUXILIARY_TYPE_ROOM);

  room->exits      = newHashtable();
  room->edescs     = newEdescSet();
  room->contents   = newList();
  room->characters = newList();
  room->extracted  = FALSE;
  room->cmd_table  = NULL;

  return room;
}


void deleteRoom(ROOM_DATA *room) {
  LIST_ITERATOR *cont_i = NULL;
  void         *content = NULL;

  // Extract all of our contents. Afterwards, delete the lists.
  cont_i = newListIterator(room->contents);
  ITERATE_LIST(content, cont_i)
    extract_obj(content);
  deleteListIterator(cont_i);

  cont_i = newListIterator(room->characters);
  ITERATE_LIST(content, cont_i)
    extract_mobile(content);
  deleteListIterator(cont_i);
  
  // delete contents
  // oops! This ain't cool... we can't delete these things with calls to
  // extract() because those extract remove the objs/chars from the room. If
  // we try removing when we're in the process of deleting lists, bad things
  // happen. So now we extract first, and THEN delete the room lists.
  //  deleteListWith(room->contents,   extract_obj);
  //  deleteListWith(room->characters, extract_mobile);
  deleteList(room->contents);
  deleteList(room->characters);

  // delete all of our exits
  HASH_ITERATOR *ex_i = newHashIterator(room->exits);
  const char     *dir = NULL;
  EXIT_DATA       *ex = NULL;
  ITERATE_HASH(dir, ex, ex_i)
    deleteExit(ex);
  deleteHashIterator(ex_i);
  deleteHashtable(room->exits);

  // delete all of our commands
  if(room->cmd_table != NULL) {
    NEAR_ITERATOR *cmd_i = newNearIterator(room->cmd_table);
    const char      *key = NULL;
    CMD_DATA        *cmd = NULL;
    ITERATE_NEARMAP(key, cmd, cmd_i)
      deleteCmd(cmd);
    deleteNearIterator(cmd_i);
    deleteNearMap(room->cmd_table);
  }
    
  // delete extra descriptions
  if(room->edescs) deleteEdescSet(room->edescs);

  // delete bits
  if(room->bits) deleteBitvector(room->bits);

  // delete strings
  if(room->prototypes) free(room->prototypes);
  if(room->class)      free(room->class);
  if(room->name)       free(room->name);
  if(room->desc)       deleteBuffer(room->desc);
  deleteAuxiliaryData(room->auxiliary_data);

  free(room);
}


STORAGE_SET *roomStore(ROOM_DATA *room) {
  STORAGE_SET          *set = new_storage_set();
  STORAGE_SET_LIST *ex_list = new_storage_list();
  store_string(set, "class",      room->class);
  store_string(set, "prototypes", room->prototypes);
  store_string(set, "name",       room->name);
  store_string(set, "desc",       bufferString(room->desc));
  store_string(set, "terrain",    terrainGetName(room->terrain));
  store_set   (set, "edescs",     edescSetStore(room->edescs));
  store_list  (set, "exits",      ex_list);
  store_long  (set, "birth",      room->birth);

  // store all of our exits. We're doing this in an odd way by putting the
  // direction name on the storage set for the exit. They should probably be
  // in different storage sets, and nested in another key:val pair storage set.
  // But this is the way we started doing it, and for the sake of compatibility,
  // we're going to keep at it...
  HASH_ITERATOR *ex_i = newHashIterator(room->exits);
  const char     *dir = NULL;
  EXIT_DATA       *ex = NULL;
  ITERATE_HASH(dir, ex, ex_i) {
    STORAGE_SET *ex_set = exitStore(ex);
    store_string(ex_set, "direction", dir);
    storage_list_put(ex_list, ex_set);
  } deleteHashIterator(ex_i);
  
  // store our auxiliary data
  store_set(set, "auxiliary", auxiliaryDataStore(room->auxiliary_data));

  // store all of our characters
  if(listSize(roomGetCharacters(room)) > 0) {
    LIST_ITERATOR      *ch_i = newListIterator(roomGetCharacters(room));
    CHAR_DATA            *ch = NULL;
    STORAGE_SET_LIST *chlist = new_storage_list();
    ITERATE_LIST(ch, ch_i) {
      if(charIsNPC(ch))
	storage_list_put(chlist, charStore(ch));
    } deleteListIterator(ch_i);
    store_list(set, "chars", chlist);
  }

  // and all of our objects
  if(listSize(roomGetContents(room)) > 0)
    store_list(set, "objs", gen_store_list(room->contents, objStore));

  return set;
}


ROOM_DATA *roomRead(STORAGE_SET *set) {
  ROOM_DATA           *room = newRoom();
  STORAGE_SET_LIST *ex_list = read_list(set, "exits");
  STORAGE_SET       *ex_set = NULL;
  roomSetClass(room, read_string(set, "class"));
  roomSetPrototypes(room, read_string(set, "prototypes"));
  roomSetName(room,       read_string(set, "name"));
  roomSetDesc(room,       read_string(set, "desc"));
  roomSetTerrain(room,    terrainGetNum(read_string(set,"terrain")));
  roomSetEdescs(room,     edescSetRead(read_set   (set, "edescs")));
  bitSet(room->bits,      read_string(set, "room_bits"));

  if(storage_contains(set, "birth"))
    room->birth = read_long(set, "birth");

  // parse and add all of our exits
  while( (ex_set = storage_list_next(ex_list)) != NULL)
    roomSetExit(room, read_string(ex_set, "direction"), exitRead(ex_set));

  deleteAuxiliaryData(room->auxiliary_data);
  room->auxiliary_data = auxiliaryDataRead(read_set(set, "auxiliary"), 
					   AUXILIARY_TYPE_ROOM);

  // read all of our characters
  if(storage_contains(set, "chars")) {
    deleteList(room->characters);
    room->characters    = gen_read_list(read_list(set, "chars"), charRead);
    CHAR_DATA       *ch = NULL;
    LIST_ITERATOR *ch_i = newListIterator(room->characters);
    ITERATE_LIST(ch, ch_i) {
      charSetRoom(ch, room);
    } deleteListIterator(ch_i);
  }

  // and all of our objects
  if(storage_contains(set, "objs")) {
    deleteList(room->contents);
    room->contents = gen_read_list(read_list(set, "objs"), objRead);
    OBJ_DATA        *obj = NULL;
    LIST_ITERATOR *obj_i = newListIterator(room->contents);
    ITERATE_LIST(obj, obj_i) {
      objSetRoom(obj, room);
    } deleteListIterator(obj_i);
  }

  return room;
}



ROOM_DATA *roomCopy(ROOM_DATA *room) {
  ROOM_DATA *R = newRoom();
  roomCopyTo(room, R);
  return R;
}


void roomCopyTo(ROOM_DATA *from, ROOM_DATA *to) {
  // we just want to copy data ABOUT the room, and not stuff
  // contained in the particular instance (e.g. players, contents)
  roomSetClass     (to, roomGetClass(from));
  roomSetPrototypes(to, roomGetPrototypes(from));
  roomSetClass     (to, roomGetClass(from));
  roomSetName      (to, roomGetName(from));
  roomSetDesc      (to, roomGetDesc(from));
  roomSetTerrain   (to, roomGetTerrain(from));
  bitvectorCopyTo  (from->bits, to->bits);
  to->birth = from->birth;

  // set our edescs
  roomSetEdescs(to, edescSetCopy(from->edescs));

  // copy all of our exits. Augh, this is ugly. If we're copying from a room to
  // another room that is in game, we have to put all of the new exits in game
  // as well, and remove all of the old exits from game. Its sort of hackish to
  // do it here since the room datastructure should have no concept of in/out
  // of game, but there's really nowhere else to put this...
  bool does_room_exist = room_exists(to);
  bool room_in_game    = listIn(room_list, to);

  // first, delete all of our old exits
  HASH_ITERATOR *ex_i = newHashIterator(to->exits);
  const char     *dir = NULL;
  EXIT_DATA       *ex = NULL;
  ITERATE_HASH(dir, ex, ex_i) {
    hashRemove(to->exits, dir);
    if(does_room_exist) exit_from_game(ex);
    deleteExit(ex);
  } deleteHashIterator(ex_i);

  // now, copy all of our new exits
  ex_i = newHashIterator(from->exits);
  ITERATE_HASH(dir, ex, ex_i) {
    roomSetExit(to, dir, exitCopy(ex));
    if(does_room_exist) exit_exist(ex);
    if(room_in_game)    exit_to_game(roomGetExit(to, dir));
  } deleteHashIterator(ex_i);

  // delete all of our old commands
  if(to->cmd_table != NULL) {
    NEAR_ITERATOR *cmd_i = newNearIterator(to->cmd_table);
    const char   *abbrev = NULL;
    CMD_DATA        *cmd = NULL;
    ITERATE_NEARMAP(abbrev, cmd, cmd_i) {
      nearMapRemove(to->cmd_table, cmdGetName(cmd));
      deleteCmd(cmd);
    } deleteNearIterator(cmd_i);
  }

  // now, copy in all of our new commands
  if(from->cmd_table != NULL) {
    if(to->cmd_table == NULL)
      to->cmd_table = newNearMap();
    NEAR_ITERATOR *cmd_i = newNearIterator(from->cmd_table);
    const char   *abbrev = NULL;
    CMD_DATA        *cmd = NULL;
    ITERATE_NEARMAP(abbrev, cmd, cmd_i) {
      nearMapPut(to->cmd_table, cmdGetName(cmd), abbrev, cmdCopy(cmd));
    } deleteNearIterator(cmd_i);
  }
  
  // copy all of our auxiliary data
  auxiliaryDataCopyTo(from->auxiliary_data, to->auxiliary_data);
}

void roomSetExtracted(ROOM_DATA *room) {
  room->extracted = TRUE;
}

bool roomIsExtracted(ROOM_DATA *room) {
  return room->extracted;
}

bool roomIsInstance(ROOM_DATA *room, const char *prototype) {
  return is_keyword(room->prototypes, prototype, FALSE);
}

const char *roomGetPrototypes(ROOM_DATA *room) {
  return room->prototypes;
}

void roomAddPrototype(ROOM_DATA *room, const char *prototype) {
  add_keyword(&room->prototypes, prototype);
}

void roomSetPrototypes(ROOM_DATA *room, const char *prototypes) {
  if(room->prototypes) free(room->prototypes);
  room->prototypes = strdupsafe(prototypes);
}



//*****************************************************************************
// add and remove functions
//*****************************************************************************
void roomRemoveChar(ROOM_DATA *room, const CHAR_DATA *ch) {
  listRemove(room->characters, ch);
}

void roomRemoveObj(ROOM_DATA *room, const OBJ_DATA *obj) {
  listRemove(room->contents, obj);
}

void roomAddChar(ROOM_DATA *room, CHAR_DATA *ch) {
  listPut(room->characters, ch);
}

void roomAddObj(ROOM_DATA *room, OBJ_DATA *obj) {
  listPut(room->contents, obj);
}



//*****************************************************************************
// exit functions
//*****************************************************************************
void roomSetExit(ROOM_DATA *room, const char *dir, EXIT_DATA *exit) {
  hashPut(room->exits, dir, exit);
  exitSetRoom(exit, room);
}

EXIT_DATA *roomGetExit(ROOM_DATA *room, const char *dir) {
  return hashGet(room->exits, dir);
}

EXIT_DATA *roomRemoveExit(ROOM_DATA *room, const char *dir) {
  EXIT_DATA *exit = hashRemove(room->exits, dir);
  if(exit != NULL) exitSetRoom(exit, NULL);
  return exit;
}

const char *roomGetExitDir(ROOM_DATA *room, EXIT_DATA *exit) {
  // go through all of our key:val pairs, and see which key matches this exit
  HASH_ITERATOR *ex_i = newHashIterator(room->exits);
  const char     *dir = NULL;
  EXIT_DATA       *ex = NULL;
  ITERATE_HASH(dir, ex, ex_i) {
    if(ex == exit) {
      deleteHashIterator(ex_i);
      return dir;
    }
  } deleteHashIterator(ex_i);
  return NULL;
}

LIST *roomGetExitNames(ROOM_DATA *room) {
  return hashCollect(room->exits);
}



//*****************************************************************************
// get and set functions for rooms
//*****************************************************************************
const char *roomGetClass(ROOM_DATA *room) {
  return room->class;
}

void roomSetClass(ROOM_DATA *room, const char *prototype) {
  if(room->class) free(room->class);
  room->class = strdupsafe(prototype);
}

LIST       *roomGetContents    (const ROOM_DATA *room) {
  return room->contents;
}

LIST       *roomGetCharacters  (const ROOM_DATA *room) {
  return room->characters;
}

const char *roomGetName        (const ROOM_DATA *room) {
  return room->name;
}

const char *roomGetDesc        (const ROOM_DATA *room) {
  return bufferString(room->desc);
}

BUFFER *roomGetDescBuffer(const ROOM_DATA *room) {
  return room->desc;
}

int         roomGetTerrain     (const ROOM_DATA *room) {
  return room->terrain;
}

int         roomGetUID         (const ROOM_DATA *room) {
  return room->uid;
}

time_t      roomGetBirth       (const ROOM_DATA *room) {
  return room->birth;
}

EDESC_SET  *roomGetEdescs      (const ROOM_DATA *room) {
  return room->edescs;
}

const char *roomGetEdesc       (const ROOM_DATA *room, const char *keyword) {
  EDESC_DATA *edesc = edescSetGet(room->edescs, keyword);
  if(edesc) return edescSetGetDesc(edesc);
  else return NULL;
}

void *roomGetAuxiliaryData     (const ROOM_DATA *room, const char *name) {
  return auxiliaryGet(room->auxiliary_data, name);
}

void        roomSetEdescs      (ROOM_DATA *room, EDESC_SET *edescs) {
  if(room->edescs) deleteEdescSet(room->edescs);
  room->edescs = edescs;
}

void        roomSetName        (ROOM_DATA *room, const char *name) {
  if(room->name) free(room->name);
  room->name = strdupsafe(name);
}

void        roomSetDesc (ROOM_DATA *room, const char *desc) {
  bufferClear(room->desc);
  bufferCat(room->desc, (desc ? desc : ""));
}

void        roomSetTerrain     (ROOM_DATA *room, int terrain_type) {
  room->terrain = terrain_type;
}

BITVECTOR *roomGetBits(const ROOM_DATA *room) {
  return room->bits;
}

NEAR_MAP *roomGetCmdTable(const ROOM_DATA *room) {
  return room->cmd_table;
}

bool roomHasCmds(const ROOM_DATA *room) {
  return room->cmd_table != NULL;
}

bool roomHasCmd(const ROOM_DATA *room, const char *name) {
  return room->cmd_table != NULL && nearMapKeyExists(room->cmd_table, name);
}

CMD_DATA *roomRemoveCmd(ROOM_DATA *room, const char *name) {
  if(room->cmd_table != NULL)
    return nearMapRemove(room->cmd_table, name);
  return NULL;
}

CMD_DATA *roomGetCmd(ROOM_DATA *room, const char *name, bool abbr_ok) {
  if(room->cmd_table != NULL)
    return nearMapGet(room->cmd_table, name, abbr_ok);
  return NULL;
}

void roomAddCmd(ROOM_DATA *room, const char *name, 
		const char *abbr, CMD_DATA *cmd) {
  CMD_DATA *old = NULL;
  if(room->cmd_table == NULL)
    room->cmd_table = newNearMap();
  else
    old = nearMapRemove(room->cmd_table, name);
  nearMapPut(room->cmd_table, name, abbr, cmd);
  if(old != NULL)
    deleteCmd(old);
}



//*****************************************************************************
// direction stuff
//*****************************************************************************
const char *dir_names[NUM_DIRS] = {
  "north",
  "east",
  "south",
  "west",
  "up",
  "down",
  "northeast",
  "southeast",
  "southwest",
  "northwest"
};

const char *dir_abbrevs[NUM_DIRS] = {
  "n", "e", "s", "w", "u", "d",
  "ne", "se", "sw", "nw"
};

const char *dirGetName(int dir) {
  return dir_names[dir];
}

const char *dirGetAbbrev(int dir) {
  return dir_abbrevs[dir];
}

int dirGetOpposite(int dir) {
  switch(dir) {
  case DIR_NORTH:        return DIR_SOUTH;
  case DIR_EAST:         return DIR_WEST;
  case DIR_SOUTH:        return DIR_NORTH;
  case DIR_WEST:         return DIR_EAST;
  case DIR_UP:           return DIR_DOWN;
  case DIR_DOWN:         return DIR_UP;
  case DIR_NORTHEAST:    return DIR_SOUTHWEST;
  case DIR_SOUTHEAST:    return DIR_NORTHWEST;
  case DIR_SOUTHWEST:    return DIR_NORTHEAST;
  case DIR_NORTHWEST:    return DIR_SOUTHEAST;
  default:               return DIR_NONE;
  }
}

int dirGetNum(const char *dir) {
  int i;
  for(i = 0; i < NUM_DIRS; i++)
    if(!strcasecmp(dir, dir_names[i]))
      return i;
  return DIR_NONE;
}

int dirGetAbbrevNum(const char *dir) {
  int i;
  for(i = 0; i < NUM_DIRS; i++)
    if(!strcasecmp(dir, dir_abbrevs[i]))
      return i;
  return DIR_NONE;
}



//*****************************************************************************
//
// terrain stuff
//
//*****************************************************************************
struct terrain_data {
  char   *name;
};

const struct terrain_data terrain_types[NUM_TERRAINS] = {
  { "Inside"        },
  { "City"          },
  { "Road"          },
  { "Alley"         },
  { "Bridge"        },
  { "Shallow Water" },
  { "Deep Water"    },
  { "Ocean"         },
  { "Underwater"    },
  { "Field"         },
  { "Plains"        },
  { "Meadow"        },
  { "Forest"        },
  { "Deep Forest"   },
  { "Hills"         },
  { "High Hills"    },
  { "Mountains"     },
  { "Swamp"         },
  { "Deep Swamp"    },
  { "Sand"          },
  { "Desert"        }, 
  { "Ice"           },
  { "Glacier"       },
  { "Cavern"        },
};


const char *terrainGetName(int terrain) {
  return terrain_types[terrain].name;
}

int terrainGetNum(const char *terrain) {
  int i;
  for(i = 0; i < NUM_TERRAINS; i++)
    if(!strcasecmp(terrain, terrain_types[i].name))
      return i;
  return TERRAIN_NONE;
}
