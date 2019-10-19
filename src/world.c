//*****************************************************************************
//
// world.c
//
// This is the implementation of the WORLD structure.
//
//*****************************************************************************

#include <sys/stat.h>

#include "mud.h"
#include "utils.h"
#include "zone.h"
#include "storage.h"
#include "prototype.h"
#include "world.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_PERSISTENT
#include "persistent/persistent.h"
#endif



//*****************************************************************************
// defines, structures, local functions
//*****************************************************************************

// the number of rooms we would expect, in different sized worlds
#define SMALL_WORLD       3000
#define MEDIUM_WORLD      7000
#define LARGE_WORLD      15000
#define HUGE_WORLD      250000

typedef struct {
  void   *read_func;
  void  *store_func;
  void *delete_func;
  void    *key_func;
  bool    forgetful;
} WORLD_TYPE_DATA;

struct world_data {
  char            *path; // the path to our world directory
  HASHTABLE      *rooms; // this table is a communal table for rooms. Used for
  HASHTABLE *type_table; // types, and their functions
  HASHTABLE      *zones; // a table of all the zones we have
};

WORLD_TYPE_DATA *newWorldTypeData(void *reader, void *storer, void *deleter,
				  void *keysetter, bool forgetful) {
  WORLD_TYPE_DATA *data = malloc(sizeof(WORLD_TYPE_DATA));
  data->read_func       = reader;
  data->store_func      = storer;
  data->delete_func     = deleter;
  data->key_func        = keysetter;
  data->forgetful       = forgetful;
  return data;
}

void deleteWorldTypeData(WORLD_TYPE_DATA *data) {
  free(data);
}


//
// transfers all of the types from the world to the zone
void world_types_to_zone_types(WORLD_DATA *world, ZONE_DATA *zone) {
  HASH_ITERATOR *type_i = newHashIterator(world->type_table);
  WORLD_TYPE_DATA *type = NULL;
  const char       *key = NULL;
  ITERATE_HASH(key, type, type_i)
    zoneAddType(zone, key, type->read_func, type->store_func,type->delete_func,
		type->key_func);
  deleteHashIterator(type_i);
}



//*****************************************************************************
// implementation of world.h
//*****************************************************************************
WORLD_DATA *newWorld(void) {
  WORLD_DATA *world = malloc(sizeof(WORLD_DATA));
  world->type_table = newHashtable();
  world->zones      = newHashtable();
  world->rooms      = newHashtableSize(SMALL_WORLD);
  world->path       = strdup("");
  return world;
}

void deleteWorld(WORLD_DATA *world) {
  HASH_ITERATOR *type_i = newHashIterator(world->type_table);
  const char       *key = NULL;
  WORLD_TYPE_DATA *type = NULL;
  HASH_ITERATOR *zone_i = newHashIterator(world->zones);
  ZONE_DATA       *zone = NULL;

  // delete all of our type info
  ITERATE_HASH(key, type, type_i)
    deleteWorldTypeData(type);
  deleteHashIterator(type_i);
  deleteHashtable(world->type_table);

  // detach ourself from all of our zones
  ITERATE_HASH(key, zone, zone_i)
    zoneSetWorld(zone, NULL);
  deleteHashIterator(zone_i);
  deleteHashtable(world->zones);

  deleteHashtable(world->rooms);
  free(world->path);

  free(world);
}

ZONE_DATA *worldRemoveZone(WORLD_DATA *world, const char *key) {
  return hashRemove(world->zones, key);
}

bool worldSave(WORLD_DATA *world, const char *dirpath) {
  char buf[MAX_BUFFER];
  STORAGE_SET       *set = new_storage_set();
  STORAGE_SET_LIST *list = new_storage_list();
  store_list(set, "zones", list);

  HASH_ITERATOR *zone_i = newHashIterator(world->zones);
  const char       *key = NULL;
  ZONE_DATA       *zone = NULL;
  // save each zone to its own directory, and also put
  // its number in the save file for the zone list
  ITERATE_HASH(key, zone, zone_i) {
    if(zoneSave(zone)) {
      STORAGE_SET *zone_set = new_storage_set();
      store_string(zone_set, "key", zoneGetKey(zone));
      storage_list_put(list, zone_set);
    }
  } deleteHashIterator(zone_i);

  sprintf(buf, "%s/world", dirpath);
  storage_write(set, buf);
  storage_close(set);
  return TRUE;
}

void worldInit(WORLD_DATA *world) {
  char buf[MAX_BUFFER];
  sprintf(buf, "%s/world", world->path);

  STORAGE_SET       *set = storage_read(buf);
  STORAGE_SET_LIST *list = read_list(set, "zones");
  STORAGE_SET  *zone_set = NULL;

  while( (zone_set = storage_list_next(list)) != NULL) {
    ZONE_DATA *zone = NULL;
    const char *key = read_string(zone_set, "key");
    zone = zoneLoad(world, key);

    if(zone != NULL) {
      hashPut(world->zones, key, zone);
      world_types_to_zone_types(world, zone);
    }
  }
  storage_close(set);
}

void worldPulse(WORLD_DATA *world) {
  HASH_ITERATOR *zone_i = newHashIterator(world->zones);
  const char       *key = NULL;
  ZONE_DATA       *zone = NULL;

  ITERATE_HASH(key, zone, zone_i)
    zonePulse(zone);
  deleteHashIterator(zone_i);
}

void worldForceReset(WORLD_DATA *world) {
  HASH_ITERATOR *zone_i = newHashIterator(world->zones);
  const char       *key = NULL;
  ZONE_DATA       *zone = NULL;

  ITERATE_HASH(key, zone, zone_i)
    zoneForceReset(zone);
  deleteHashIterator(zone_i);
}



//*****************************************************************************
// set and get functions
//*****************************************************************************
LIST *worldGetZoneKeys(WORLD_DATA *world) {
  LIST            *keys = newList();
  HASH_ITERATOR *zone_i = newHashIterator(world->zones);
  const char       *key = NULL;
  ZONE_DATA       *zone = NULL;

  ITERATE_HASH(key, zone, zone_i)
    listQueue(keys, strdup(key));
  deleteHashIterator(zone_i);
  return keys;
}

const char *worldGetZonePath(WORLD_DATA *world, const char *key) {
  static char buf[SMALL_BUFFER];
  sprintf(buf, "%s/zones/%s", world->path, key);
  return buf;
}

const char *worldGetPath(WORLD_DATA *world) {
  return world->path;
}

void worldSetPath(WORLD_DATA *world, const char *path) {
  if(world->path) free(world->path);
  world->path    = strdupsafe(path);
}

void worldPutRoom(WORLD_DATA *world, const char *key, ROOM_DATA *room) {
  hashPut(world->rooms, key, room);
}

ROOM_DATA *worldGetRoom(WORLD_DATA *world, const char *key) {
  ROOM_DATA *room = NULL;
  // see if we have it in the room hashtable
  if( (room = hashGet(world->rooms, key)) == NULL) {
#ifdef MODULE_PERSISTENT
    if( (room = worldGetPersistentRoom(world, key)) == NULL) {
#endif
      char name[SMALL_BUFFER], locale[SMALL_BUFFER];
      if(parse_worldkey(key, name, locale)) {
	ZONE_DATA *zone = hashGet(world->zones, locale);
	if(zone != NULL) {
	  PROTO_DATA *rproto = zoneGetType(zone, "rproto", name);
	  if(rproto != NULL && (room = protoRoomRun(rproto)) != NULL)
	    worldPutRoom(world, protoGetKey(rproto), room);
	}
      }
#ifdef MODULE_PERSISTENT
    }
#endif
  }
  return room;
}

ROOM_DATA *worldRemoveRoom(WORLD_DATA *world, const char *key) {
  ROOM_DATA *room = hashRemove(world->rooms, key);
  return room;
}

bool worldRoomLoaded(WORLD_DATA *world, const char *key) {
  return hashIn(world->rooms, key);
}

void worldPutZone(WORLD_DATA *world, ZONE_DATA *zone) {
  // make sure there are no conflicts with other zones...
  if(hashIn(world->zones, zoneGetKey(zone))) {
    log_string("ERROR: tried to add new zone %s, but the world already has "
	       "a zone with that key!", zoneGetKey(zone));
    return;
  }

  // connect the world and zone
  hashPut(world->zones, zoneGetKey(zone), zone);
  zoneSetWorld(zone, world);

  // make the zone's directory
  mkdir(worldGetZonePath(world, zoneGetKey(zone)), S_IRWXU | S_IRWXG);

  // add in all of our type functions, which will create dirs as needed
  world_types_to_zone_types(world, zone);
}



//*****************************************************************************
// implementation of the new world interface
//*****************************************************************************
void *worldGetType(WORLD_DATA *world, const char *type, const char *key) {
  char name[SMALL_BUFFER], locale[SMALL_BUFFER];
  ZONE_DATA *zone = NULL;
  if(parse_worldkey(key, name, locale) && 
     (zone = hashGet(world->zones, locale)) != NULL)
    return zoneGetType(zone, type, name);
  return NULL;
}

void *worldRemoveType(WORLD_DATA *world, const char *type, const char *key) {
  char name[SMALL_BUFFER], locale[SMALL_BUFFER];
  ZONE_DATA *zone = NULL;
  if(parse_worldkey(key, name, locale) && 
     (zone = hashGet(world->zones, locale)) != NULL)
    return zoneRemoveType(zone, type, name);
  return NULL;
}

void worldSaveType(WORLD_DATA *world, const char *type, const char *key) {
  char name[SMALL_BUFFER], locale[SMALL_BUFFER];
  ZONE_DATA *zone = NULL;
  if(parse_worldkey(key, name, locale) && 
     (zone = hashGet(world->zones, locale)) != NULL)
    zoneSaveType(zone, type, name);
}

void worldPutType(WORLD_DATA *world, const char *type, const char *key,
		  void *data) {
  char name[SMALL_BUFFER], locale[SMALL_BUFFER];
  ZONE_DATA *zone = NULL;
  if(parse_worldkey(key, name, locale) && 
     (zone = hashGet(world->zones, locale)) != NULL)
    zonePutType(zone, type, name, data);
}

void worldAddType(WORLD_DATA *world, const char *type, void *reader,
		  void *storer, void *deleter, void *zonesetter) {
  // add the new type to each of our zones, too
  if(!hashIn(world->type_table, type)) {
    hashPut(world->type_table, type, 
	    newWorldTypeData(reader, storer, deleter, zonesetter, FALSE));
    HASH_ITERATOR *zone_i = newHashIterator(world->zones);
    const char       *key = NULL;
    ZONE_DATA       *zone = NULL;
    ITERATE_HASH(key, zone, zone_i)
      zoneAddType(zone, type, reader, storer, deleter, zonesetter);
    deleteHashIterator(zone_i);
  }
}

void worldAddForgetfulType(WORLD_DATA *world, const char *type, void *reader,
			   void *storer, void *deleter, void *zonesetter) {
  // add the new type to each of our zones, too
  if(!hashIn(world->type_table, type)) {
    hashPut(world->type_table, type, 
	    newWorldTypeData(reader, storer, deleter, zonesetter, TRUE));
    HASH_ITERATOR *zone_i = newHashIterator(world->zones);
    const char       *key = NULL;
    ZONE_DATA       *zone = NULL;
    ITERATE_HASH(key, zone, zone_i)
      zoneAddForgetfulType(zone, type, reader, storer, deleter, zonesetter);
    deleteHashIterator(zone_i);
  }
}

ZONE_DATA *worldGetZone(WORLD_DATA *world, const char *key) {
  return hashGet(world->zones, key);
}
