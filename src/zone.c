//*****************************************************************************
//
// zone.c
//
// A zone is like a stand-alone module, or encounter, within the game (e.g.
// a city in the world), and contains all of the NPCs, objects, rooms, scripts,
// etc... needed for the zone to be complete.
//
//*****************************************************************************

#include <sys/stat.h>
#include <dirent.h>

#include "mud.h"
#include "storage.h"
#include "utils.h"
#include "auxiliary.h"
#include "world.h"
#include "hooks.h"
#include "zone.h"



//*****************************************************************************
// zone type data
//*****************************************************************************
typedef struct {
  /*
  void      *(* read_func)(STORAGE_SET *);
  STORAGE_SET     *(* store_func)(void *);
  void            (* delete_func)(void *);
  void (* key_func)(void *, const char *);
  */
  void    *read_func;
  void   *store_func;
  void  *delete_func;
  void     *key_func;
  bool     forgetful;
  HASHTABLE *key_map;
  char         *type;
} ZONE_TYPE_DATA;

ZONE_TYPE_DATA *newZoneType(const char *type, void *reader, void *storer, 
			    void *deleter, void *keysetter, bool forgetful) {
  ZONE_TYPE_DATA *data = malloc(sizeof(ZONE_TYPE_DATA));
  data->read_func      = reader;
  data->store_func     = storer;
  data->delete_func    = deleter;
  data->key_func       = keysetter;
  data->forgetful      = forgetful;
  data->key_map        = newHashtable();
  data->type           = strdupsafe(type);
  return data;
}

void *do_zone_read(ZONE_TYPE_DATA *tdata, STORAGE_SET *set) {
  if(tdata->forgetful) {
    void *(* read_func)(const char *, STORAGE_SET *) = tdata->read_func;
    return read_func(tdata->type, set);
  }
  else {
    void *(* read_func)(STORAGE_SET *) = tdata->read_func;
    return read_func(set);
  }
}

void do_zone_setkey(ZONE_TYPE_DATA *tdata, void *data, const char *key) {
  if(tdata->forgetful) {
    void (* key_func)(const char *, void *, const char *) = tdata->key_func;
    key_func(tdata->type, data, key);
  }
  else {
    void (* key_func)(void *, const char *) = tdata->key_func;
    key_func(data, key);
  }
}

STORAGE_SET *do_zone_store(ZONE_TYPE_DATA *tdata, void *data) {
  if(tdata->forgetful) {
    STORAGE_SET *(* store_func)(const char *, void *) = tdata->store_func;
    return store_func(tdata->type, data);
  }
  else {
    STORAGE_SET *(* store_func)(void *) = tdata->store_func;
    return store_func(data);
  }
}



//*****************************************************************************
// zone data
//*****************************************************************************
struct zone_data {
  char                   *key;
  char                  *name;
  char               *editors;
  BUFFER                *desc;
  WORLD_DATA           *world;
  LIST            *resettable; // a list of rooms that need to be reset on pulse
  HASHTABLE       *type_table; // a table of our types and their functions
  int             pulse_timer; // the timer duration
  int                   pulse; // how far down have we gone?
  AUX_TABLE   *auxiliary_data; // additional data installed on us
};


ZONE_DATA *newZone(const char *key) {
  ZONE_DATA *zone   = malloc(sizeof(ZONE_DATA));
  zone->type_table  = newHashtable();
  zone->name        = strdup("");
  zone->key         = strdup(key);
  zone->desc        = newBuffer(1);
  zone->editors     = strdup("");
  zone->resettable  = newList();

  zone->pulse_timer    = -1; // never resets
  zone->pulse          = -1;
  zone->world          = NULL;
  zone->auxiliary_data = newAuxiliaryData(AUXILIARY_TYPE_ZONE);

  return zone;
}

ZONE_DATA *zoneCopy(ZONE_DATA *zone) {
  ZONE_DATA *newzone = newZone(zone->key);
  zoneCopyTo(zone, newzone);
  return newzone;
}

void zoneCopyTo(ZONE_DATA *from, ZONE_DATA *to) {
  zoneSetName(to, zoneGetName(from));
  zoneSetDesc(to, zoneGetDesc(from));
  zoneSetEditors(to, zoneGetEditors(from));
  zoneSetKey(to,   zoneGetKey(from));
  deleteListWith(to->resettable, free);
  to->resettable = listCopyWith(from->resettable, strdup);
  to->pulse_timer = from->pulse_timer;
  to->pulse = from->pulse;
  auxiliaryDataCopyTo(from->auxiliary_data, to->auxiliary_data);
}

void deleteZone(ZONE_DATA *zone){ 
  if(zone->name)        free(zone->name);
  if(zone->desc)        deleteBuffer(zone->desc);
  if(zone->editors)     free(zone->editors);
  if(zone->type_table)  deleteHashtable(zone->type_table);
  if(zone->key)         free(zone->key);
  if(zone->resettable)  deleteListWith(zone->resettable, free);

  deleteAuxiliaryData(zone->auxiliary_data);

  //*******************************************************************
  // The only time we're deleting a zone is when we're editing the copy
  // in OLC. So, We don't want to delete any of the zone's content when
  // we delete it.
  //*******************************************************************
    
  free(zone);
}


//
// Pulse a zone. i.e. decrement it's reset timer. When the timer hits 0,
// set it back to the max, and reset everything in the zone
void zonePulse(ZONE_DATA *zone) { 
  zone->pulse--;
  if(zone->pulse == 0) {
    zone->pulse = zone->pulse_timer;
    hookRun("reset_zone", hookBuildInfo("str", zoneGetKey(zone)));
  }
}

void zoneForceReset(ZONE_DATA *zone) {
  zone->pulse = 1;
  zonePulse(zone);
}

//
// parses out one resettable room from a storage set
char *read_resettable_room(STORAGE_SET *set) {
  return strdup(read_string(set, "room"));
}

ZONE_DATA *zoneLoad(WORLD_DATA *world, const char *key) {
  ZONE_DATA *zone = newZone(key);
  char fname[SMALL_BUFFER];
  zone->world = world;

  // first, load all of the zone data
  sprintf(fname, "%s/zone", worldGetZonePath(world, zone->key));
  STORAGE_SET  *set = storage_read(fname);
  zone->pulse_timer = read_int   (set, "pulse_timer");
  zoneSetName(zone,   read_string(set, "name"));
  zoneSetDesc(zone,   read_string(set, "desc"));
  zoneSetEditors(zone,read_string(set, "editors"));
  
  // add in all of our resettable rooms
  deleteList(zone->resettable);
  zone->resettable = gen_read_list(read_list(set, "resettable"),
				   read_resettable_room);

  deleteAuxiliaryData(zone->auxiliary_data);
  zone->auxiliary_data = auxiliaryDataRead(read_set(set, "auxiliary"), 
					   AUXILIARY_TYPE_ZONE);
  storage_close(set);

  return zone;
}

//
// turns an entry for a resettable room into a storage set
STORAGE_SET *store_resettable_room(char *key) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "room", key);
  return set;
}

//
// the new zone saving function
bool zoneSave(ZONE_DATA *zone) {
  char fname[MAX_BUFFER];
  
  // first, for our zone data
  sprintf(fname, "%s/zone", worldGetZonePath(zone->world, zone->key));
  STORAGE_SET *set = new_storage_set();
  store_int   (set, "pulse_timer", zone->pulse_timer);
  store_string(set, "name",        zone->name);
  store_string(set, "desc",        bufferString(zone->desc));
  store_string(set, "editors",     zone->editors);
  store_set   (set, "auxiliary",   auxiliaryDataStore(zone->auxiliary_data));
  store_list  (set, "resettable",  gen_store_list(zone->resettable,
						  store_resettable_room));

  storage_write(set, fname);
  storage_close(set);
  return TRUE;
}



//*****************************************************************************
// get and set functions for zones
//*****************************************************************************
void *zoneGetAuxiliaryData(const ZONE_DATA *zone, char *name) {
  return auxiliaryGet(zone->auxiliary_data, name);
}

int zoneGetPulseTimer(ZONE_DATA *zone) { 
  return zone->pulse_timer;
}

int zoneGetPulse(ZONE_DATA *zone) { 
  return zone->pulse;
}

WORLD_DATA *zoneGetWorld(ZONE_DATA *zone) { 
  return zone->world;
}

const char *zoneGetName(ZONE_DATA *zone) { 
  return zone->name;
}

const char *zoneGetDesc(ZONE_DATA *zone) { 
  return bufferString(zone->desc);
}

const char *zoneGetEditors(ZONE_DATA *zone) {
  return zone->editors;
}

BUFFER *zoneGetDescBuffer(ZONE_DATA *zone) {
  return zone->desc;
}

LIST *zoneGetResettable(ZONE_DATA *zone) {
  return zone->resettable;
}

void zoneSetPulseTimer(ZONE_DATA *zone, int timer) { 
  // if we normally do not reset, change that
  if(zone->pulse_timer < 0)
    zone->pulse = timer;
  zone->pulse_timer = timer;
}

void zoneSetPulse(ZONE_DATA *zone, int pulse_left) { 
  zone->pulse = pulse_left;
}

void zoneSetWorld(ZONE_DATA *zone, WORLD_DATA *world) { 
  zone->world = world;
}

void zoneSetName(ZONE_DATA *zone, const char *name) { 
  if(zone->name) free(zone->name);
  zone->name = strdupsafe(name);
}

void zoneSetDesc(ZONE_DATA *zone, const char *desc) { 
  bufferClear(zone->desc);
  bufferCat(zone->desc, desc);
}

void zoneSetEditors(ZONE_DATA *zone, const char *names) {
  if(zone->editors) free(zone->editors);
  zone->editors = strdupsafe(names);
}



//*****************************************************************************
// the new zone type interface
//*****************************************************************************

//
// returns a list of all the keys in the zone for the specified type. Doesn't
// include the locale in the key. Just the name. List and contents must be
// deleted after use.
LIST *zoneGetTypeKeys(ZONE_DATA *zone, const char *type) {
  LIST *key_list = newList();
  char path[MAX_BUFFER];
  sprintf(path, "%s/%s", worldGetZonePath(zone->world, zone->key), type);
  DIR *dir = opendir(path);
  struct dirent *entry = NULL;
  if(dir != NULL) {
    for(entry = readdir(dir); entry; entry = readdir(dir)) {
      if(!startswith(entry->d_name, "."))
	listPut(key_list, strdup(entry->d_name));
    }
    closedir(dir);
  }
  return key_list;
}

//
// loads the item with the specified key into memory and returns it
void *zoneLoadType(ZONE_DATA *zone, const char *type, const char *key) {
  ZONE_TYPE_DATA *tdata = hashGet(zone->type_table, type);
  if(tdata == NULL) 
    return NULL;
  else {
    char buf[MAX_BUFFER];
    void *data = NULL;
    sprintf(buf, "%s/%s/%s", worldGetZonePath(zone->world, zone->key), 
	    type, key);
    STORAGE_SET *set = storage_read(buf);
    if(set != NULL) {
      data = do_zone_read(tdata, set);
      hashPut(tdata->key_map, key, data);
      do_zone_setkey(tdata, data, get_fullkey(key, zone->key));
      storage_close(set);
    }
    return data;
  }
}

void *zoneGetType(ZONE_DATA *zone, const char *type, const char *key) {
  ZONE_TYPE_DATA *tdata = hashGet(zone->type_table, type);
  if(tdata == NULL) 
    return NULL;
  else {
    void *data = NULL;
    // if we haven't loaded it into memory yet, do so
    if((data = hashGet(tdata->key_map, key)) == NULL)
      data = zoneLoadType(zone, type, key);
    return data;
  }
}

void zoneSaveType(ZONE_DATA *zone, const char *type, const char *key) {
  void *data = zoneGetType(zone, type, key);
  if(data != NULL) {
    ZONE_TYPE_DATA *tdata = hashGet(zone->type_table, type);
    STORAGE_SET      *set = do_zone_store(tdata, data);
    if(set != NULL) {
      char buf[MAX_BUFFER];
      sprintf(buf,"%s/%s/%s",worldGetZonePath(zone->world,zone->key),type,key);
      storage_write(set, buf);
      storage_close(set);
    }
  }
}

void *zoneRemoveType(ZONE_DATA *zone, const char *type, const char *key) {
  ZONE_TYPE_DATA *tdata = hashGet(zone->type_table, type);
  if(tdata == NULL)
    return NULL;
  else {
    // first, delete the file for it
    char buf[MAX_BUFFER];
    sprintf(buf, "%s/%s/%s",worldGetZonePath(zone->world,zone->key),type,key);
    unlink(buf);
    // then remove it from the key map
    void *data = hashRemove(tdata->key_map, key);
    if(data != NULL)
      do_zone_setkey(tdata, data, "");
    return data;
  }
}

void zonePutType(ZONE_DATA *zone, const char *type, const char *key,
		 void *data) {
  ZONE_TYPE_DATA *tdata = hashGet(zone->type_table, type);
  if(tdata != NULL) {
    hashPut(tdata->key_map, key, data);
    do_zone_setkey(tdata, data, get_fullkey(key, zone->key));
  }
}

void zoneAddType(ZONE_DATA *zone, const char *type, void *reader, 
		 void *storer, void *deleter, void *typesetter) {
  if(!hashIn(zone->type_table, type)) {
    hashPut(zone->type_table, type, newZoneType(type, reader, storer, deleter, 
						typesetter, FALSE));
    char buf[MAX_BUFFER];
    sprintf(buf, "%s/%s", worldGetZonePath(zone->world, zone->key), type);
    mkdir(buf, S_IRWXU | S_IRWXG);
  }
}

void zoneAddForgetfulType(ZONE_DATA *zone, const char *type, void *reader, 
			  void *storer, void *deleter, void *typesetter) {
  if(!hashIn(zone->type_table, type)) {
    hashPut(zone->type_table, type, newZoneType(type, reader, storer, deleter, 
						typesetter, TRUE));
    char buf[MAX_BUFFER];
    sprintf(buf, "%s/%s", worldGetZonePath(zone->world, zone->key), type);
    mkdir(buf, S_IRWXU | S_IRWXG);
  }
}

void zoneSetKey(ZONE_DATA *zone, const char *key) {
  if(zone->key) free(zone->key);
  zone->key = strdupsafe(key);
}

const char *zoneGetKey(ZONE_DATA *zone) {
  return zone->key;
}
