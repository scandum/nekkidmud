//*****************************************************************************
//
// races.c
//
// contains all of the information assocciated with different races. If you are
// wanting to add new races to the MUD, it is suggested you do so through the
// add_race() function, and make a new module for your MUD's races.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "body.h"

#include "races.h"



//*****************************************************************************
//
// local datastructures, functions, and defines
//
//*****************************************************************************

// how big is hour hashtable for holding race data?
#define RACE_TABLE_SIZE    55
HASHTABLE *race_table = NULL;


typedef struct race_data {
  char      *name;
  char      *abbrev;
  BODY_DATA *body;
  bool       pc_ok;
} RACE_DATA;


RACE_DATA *newRace(const char *name, const char *abbrev, BODY_DATA *body,
		   bool pc_ok) {
  RACE_DATA *data = malloc(sizeof(RACE_DATA));
  data->name   = strdupsafe(name);
  data->abbrev = strdupsafe(abbrev);
  data->body   = bodyCopy(body);
  data->pc_ok  = pc_ok;
  return data;
}



//*****************************************************************************
//
// implementation of race.h
//
//*****************************************************************************
void init_races() {
  // create the table for holding race data
  race_table = newHashtable();

  // make the default human body
  BODY_DATA *body = newBody();
  bodySetSize(body, BODYSIZE_MEDIUM);
  bodyAddPosition(body, "right grip",              BODYPOS_HELD,          0);
  bodyAddPosition(body, "left grip",               BODYPOS_HELD,          0);
  bodyAddPosition(body, "right foot",              BODYPOS_RIGHT_FOOT,    2);
  bodyAddPosition(body, "left foot",               BODYPOS_LEFT_FOOT,     2);
  bodyAddPosition(body, "right leg",               BODYPOS_LEG,           9);
  bodyAddPosition(body, "left leg",                BODYPOS_LEG,           9);
  bodyAddPosition(body, "waist",                   BODYPOS_WAIST,         1);
  bodyAddPosition(body, "right finger",            BODYPOS_FINGER,        1);
  bodyAddPosition(body, "left finger",             BODYPOS_FINGER,        1);
  bodyAddPosition(body, "right hand",              BODYPOS_RIGHT_HAND,    2);
  bodyAddPosition(body, "left hand",               BODYPOS_LEFT_HAND,     2);
  bodyAddPosition(body, "right wrist",             BODYPOS_WRIST,         1);
  bodyAddPosition(body, "left wrist",              BODYPOS_WRIST,         1);
  bodyAddPosition(body, "right arm",               BODYPOS_ARM,           7);
  bodyAddPosition(body, "left arm",                BODYPOS_ARM,           7);
  bodyAddPosition(body, "about body",              BODYPOS_ABOUT,         0);
  bodyAddPosition(body, "torso",                   BODYPOS_TORSO,        50);
  bodyAddPosition(body, "neck",                    BODYPOS_NECK,          1);
  bodyAddPosition(body, "right ear",               BODYPOS_EAR,           0);
  bodyAddPosition(body, "left ear",                BODYPOS_EAR,           0);
  bodyAddPosition(body, "face",                    BODYPOS_FACE,          2);
  bodyAddPosition(body, "head",                    BODYPOS_HEAD,          2);
  bodyAddPosition(body, "floating about head",     BODYPOS_FLOAT,         0);
  //                                                                  ------
  //                                                                    100

  // add the basic races
  add_race("human", "hum", body, TRUE);
  //********************************************************************
  // If you are wanting to add new, non-stock races it is suggested
  // you do so through a module and import them with add_race instead
  // of putting them directly into this folder. This will make your life
  // much easier whenever new versions of NakedMud are released and you
  // want to upgrade!
  //********************************************************************
}


void add_race(const char *name, const char *abbrev, BODY_DATA *body, int pc_ok){
  hashPut(race_table, name, newRace(name, abbrev, body, pc_ok));
}

int raceCount() {
  return hashSize(race_table);
}

bool isRace(const char *name) {
  return (hashGet(race_table, name) != NULL);
}

BODY_DATA *raceCreateBody(const char *name) {
  RACE_DATA *data = hashGet(race_table, name);
  return (data ? bodyCopy(data->body) : NULL);
}

bool raceIsForPC(const char *name) {
  RACE_DATA *data = hashGet(race_table, name);
  return (data ? data->pc_ok : FALSE);
}

const char *raceGetAbbrev(const char *name) {
  RACE_DATA *data = hashGet(race_table, name);
  return (data ? data->abbrev : NULL);
}

const char *raceDefault() {
  return "human";
}

const char *raceGetList(bool pc_only) {
  static char buf[MAX_BUFFER];
  LIST       *name_list = newList();
  HASH_ITERATOR *race_i = newHashIterator(race_table);
  const char      *name = NULL;
  RACE_DATA       *data = NULL;

  // collect all of our names
  ITERATE_HASH(name, data, race_i) {
    if(pc_only && data->pc_ok == FALSE) continue;
    listPutWith(name_list, strdup(name), strcmp);
  }
  deleteHashIterator(race_i);

  // print all the names to our buffer
  char *new_name = NULL; // gotta use a new name ptr.. can't free const
  int i = 0;
  while( (new_name = listPop(name_list)) != NULL) {
    i += snprintf(buf+i, MAX_BUFFER-i, "%s%s",
		  new_name, (listSize(name_list) > 0 ? ", " : ""));
    free(new_name);
  }

  // delete our list of names and return the buffer
  deleteListWith(name_list, free);
  return buf;
}
