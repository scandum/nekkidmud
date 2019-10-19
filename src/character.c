//*****************************************************************************
//
// character.c
//
// Basic implementation of the character datastructure (PCs and NPCs). Includes
// data for storing room, inventory, body, equipment, and other "essential"
// information. If you plan on adding any other information to characters, it
// is strongly suggested you do so through auxiliary data (see auxiliary.h).
//
// For a recap, IF YOU PLAN ON ADDING ANY OTHER INFORMATION TO CHARACTERS, IT
// IS STRONGLY SUGGESTED YOU DO SO THROUGH AUXILIARY DATA (see auxiliary.h).
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "body.h"
#include "races.h"
#include "auxiliary.h"
#include "storage.h"
#include "character.h"

const char *sex_names[NUM_SEXES] = {
  "male",
  "female",
  "neutral"
};

const char *sexGetName(int sex) {
  return sex_names[sex];
}

int  sexGetNum(const char *sex) {
  int i;
  for(i = 0; i < NUM_SEXES; i++)
    if(!strcasecmp(sex, sex_names[i]))
      return i;
  return SEX_NONE;
}

struct pos_data {
  char *name;
  char *action_self;
  char *action_other;
};

const struct pos_data pos_info[NUM_POSITIONS] = {
  { "unconscious", "fall unconscious", "falls unconscious" },
  { "sleeping",   "sleep",           "sleeps"           },
  { "sitting",    "sit",             "sits"             },
  { "standing",   "stand",           "stands"           },
  { "flying",     "fly",             "flies"            }
};

const char *posGetName(int pos) {
  return pos_info[pos].name;
}

const char *posGetActionSelf(int pos) {
  return pos_info[pos].action_self;
}

const char *posGetActionOther(int pos) {
  return pos_info[pos].action_other;
}

int  posGetNum(const char *pos) {
  int i;
  for(i = 0; i < NUM_POSITIONS; i++)
    if(!strcasecmp(pos, pos_info[i].name))
      return i;
  return POS_NONE;
}

int poscmp(int pos1, int pos2) {
  // a little hack... assumes that they
  // are ordered in their definitions
  if(pos1 == pos2)     return 0;
  else if(pos1 < pos2) return -1;
  else                 return 1;
}


struct char_data {
  // data for PCs only
  char                 * loadroom;

  // shared data for PCs and NPCs
  int                    uid;
  time_t                 birth;

  BODY_DATA            * body;
  char                 * race;
  char                 * prototypes;
  char                 * class;

  SOCKET_DATA          * socket;
  ROOM_DATA            * room;
  ROOM_DATA            * last_room;
  OBJ_DATA             * furniture;
  BUFFER               * desc;
  BUFFER               * look_buf;
  char                 * name;
  int                    sex;
  int                    position;
  int                    hidden;
  double                 weight;

  LIST                 * inventory;
  AUX_TABLE            * auxiliary_data;
  BITVECTOR            * prfs;
  BITVECTOR            * user_groups;

  // data for NPCs only
  char                 * rdesc;
  char                 * multi_name;
  char                 * multi_rdesc;
  char                 * keywords;
};


CHAR_DATA *newChar() {
  CHAR_DATA *ch   = calloc(1, sizeof(CHAR_DATA));

  ch->loadroom      = strdup("");
  ch->uid           = NOBODY;
  ch->birth         = current_time;

  ch->race          = strdup(raceDefault());
  ch->body          = raceCreateBody(ch->race);
  ch->room          = NULL;
  ch->last_room     = NULL;
  ch->furniture     = NULL;
  ch->socket        = NULL;
  ch->desc          = newBuffer(1);
  ch->look_buf      = newBuffer(1);
  ch->name          = strdup("");
  ch->sex           = SEX_NEUTRAL;
  ch->position      = POS_STANDING;
  ch->inventory     = newList();

  ch->class         = strdup("");
  ch->prototypes    = strdup("");
  ch->rdesc         = strdup("");
  ch->keywords      = strdup("");
  ch->multi_rdesc   = strdup("");
  ch->multi_name    = strdup("");
  ch->prfs          = bitvectorInstanceOf("char_prfs");
  ch->user_groups   = bitvectorInstanceOf("user_groups");
  bitSet(ch->user_groups, DFLT_USER_GROUP);

  ch->auxiliary_data = newAuxiliaryData(AUXILIARY_TYPE_CHAR);

  return ch;
};



//*****************************************************************************
// utility functions
//*****************************************************************************
void charSetRdesc(CHAR_DATA *ch, const char *rdesc) {
  if(ch->rdesc) free(ch->rdesc);
  ch->rdesc =   strdupsafe(rdesc);
}

void charSetMultiRdesc(CHAR_DATA *ch, const char *multi_rdesc) {
  if(ch->multi_rdesc) free(ch->multi_rdesc);
  ch->multi_rdesc =   strdupsafe(multi_rdesc);
}

void charSetMultiName(CHAR_DATA *ch, const char *multi_name) {
  if(ch->multi_name) free(ch->multi_name);
  ch->multi_name =   strdupsafe(multi_name);
}

bool charIsInstance(CHAR_DATA *ch, const char *prototype) {
  return is_keyword(ch->prototypes, prototype, FALSE);
}

bool charIsNPC( CHAR_DATA *ch) {
  return (ch->uid >= START_UID);
}

bool charIsName( CHAR_DATA *ch, const char *name) {
  if(charIsNPC(ch))
    return is_keyword(ch->keywords, name, TRUE);
  else
    return !strncasecmp(ch->name, name, strlen(name));
}


//*****************************************************************************
// set and get functions
//*****************************************************************************
LIST        *charGetInventory ( CHAR_DATA *ch) {
  return ch->inventory;
}

SOCKET_DATA *charGetSocket    ( CHAR_DATA *ch) {
  return ch->socket;
}

ROOM_DATA   *charGetRoom      ( CHAR_DATA *ch) {
  return ch->room;
}

ROOM_DATA *charGetLastRoom(CHAR_DATA *ch) {
  return ch->last_room;
}

const char *charGetClass(CHAR_DATA *ch) {
  return ch->class;
}

const char  *charGetPrototypes( CHAR_DATA *ch) {
  return ch->prototypes;
}

const char  *charGetName      ( CHAR_DATA *ch) {
  return ch->name;
}

const char  *charGetDesc      ( CHAR_DATA *ch) {
  return bufferString(ch->desc);
}

BUFFER      *charGetDescBuffer( CHAR_DATA *ch) {
  return ch->desc;
}

BUFFER      *charGetLookBuffer( CHAR_DATA *ch) {
  return ch->look_buf;
}

const char  *charGetRdesc     ( CHAR_DATA *ch) {
  return ch->rdesc;
}

const char  *charGetMultiRdesc( CHAR_DATA *ch) {
  return ch->multi_rdesc;
}

const char  *charGetMultiName( CHAR_DATA *ch) {
  return ch->multi_name;
}

int charGetSex(CHAR_DATA *ch) {
  return ch->sex;
}

int charGetPos(CHAR_DATA *ch) {
  return ch->position;
}

int charGetHidden(const CHAR_DATA *ch) {
  return ch->hidden;
}

double charGetWeight(const CHAR_DATA *ch) {
  return ch->weight;
}

BODY_DATA *charGetBody(CHAR_DATA *ch) {
  return ch->body;
}

const char *charGetRace(CHAR_DATA *ch) {
  return ch->race;
}

int charGetUID(const CHAR_DATA *ch) {
  return ch->uid;
}

time_t       charGetBirth ( const CHAR_DATA *ch) {
  return ch->birth;
}

const char *charGetLoadroom (CHAR_DATA *ch) {
  return ch->loadroom;
}

void *charGetAuxiliaryData(const CHAR_DATA *ch, const char *name) {
  return auxiliaryGet(ch->auxiliary_data, name);
}

OBJ_DATA *charGetFurniture(CHAR_DATA *ch) {
  return ch->furniture;
}

BITVECTOR *charGetPrfs(CHAR_DATA *ch) {
  return ch->prfs;
}

BITVECTOR *charGetUserGroups(CHAR_DATA *ch) {
  return ch->user_groups;
}

void         charSetSocket    ( CHAR_DATA *ch, SOCKET_DATA *socket) {
  ch->socket = socket;
}

void         charSetRoom      ( CHAR_DATA *ch, ROOM_DATA *room) {
  ch->room   = room;
}

void charSetLastRoom(CHAR_DATA *ch, ROOM_DATA *room) {
  ch->last_room = room;
}

void charSetClass(CHAR_DATA *ch, const char *prototype) {
  if(ch->class) free(ch->class);
  ch->class = strdupsafe(prototype);
}

void charSetPrototypes(CHAR_DATA *ch, const char *prototypes) {
  if(ch->prototypes) free(ch->prototypes);
  ch->prototypes = strdupsafe(prototypes);
}

void charAddPrototype(CHAR_DATA *ch, const char *prototype) {
  add_keyword(&ch->prototypes, prototype);
}

void         charSetName      ( CHAR_DATA *ch, const char *name) {
  if(ch->name) free(ch->name);
  ch->name = strdupsafe(name);
}

void         charSetSex       ( CHAR_DATA *ch, int sex) {
  ch->sex = sex;
}

void         charSetPos       ( CHAR_DATA *ch, int pos) {
  ch->position = pos;
}

void         charSetHidden    ( CHAR_DATA *ch, int amnt) {
  ch->hidden = amnt;
}

void charSetWeight(CHAR_DATA *ch, double amnt) {
  ch->weight = amnt;
}

void         charSetDesc      ( CHAR_DATA *ch, const char *desc) {
  bufferClear(ch->desc);
  bufferCat(ch->desc, (desc ? desc : ""));
}

void         charSetBody      ( CHAR_DATA *ch, BODY_DATA *body) {
  if(ch->body)
    deleteBody(ch->body);
  ch->body = body;
}

void         charSetRace  (CHAR_DATA *ch, const char *race) {
  if(ch->race) free(ch->race);
  ch->race = strdupsafe(race);
}

void         charSetUID(CHAR_DATA *ch, int uid) {
  ch->uid = uid;
}

void         charResetBody(CHAR_DATA *ch) {
  charSetBody(ch, raceCreateBody(ch->race));
}

void charSetLoadroom(CHAR_DATA *ch, const char *loadroom) {
  if(ch->loadroom) free(ch->loadroom);
  ch->loadroom = strdupsafe(loadroom);
}

void charSetFurniture(CHAR_DATA *ch, OBJ_DATA *furniture) {
  ch->furniture = furniture;
}



//*****************************************************************************
//
// mob-specific functions
//
//*****************************************************************************
CHAR_DATA *newMobile() {
  CHAR_DATA *mob = newChar();
  mob->uid = next_uid();
  return mob;
};


void deleteChar( CHAR_DATA *mob) {
  // it is assumed we've already unequipped
  // all of the items that are on our body (extract_char)
  if(mob->body) deleteBody(mob->body);
  // it's also assumed we've extracted our inventory
  deleteList(mob->inventory);

  if(mob->class)       free(mob->class);
  if(mob->prototypes)  free(mob->prototypes);
  if(mob->name)        free(mob->name);
  if(mob->desc)        deleteBuffer(mob->desc);
  if(mob->look_buf)    deleteBuffer(mob->look_buf);
  if(mob->rdesc)       free(mob->rdesc);
  if(mob->multi_rdesc) free(mob->multi_rdesc);
  if(mob->multi_name)  free(mob->multi_name);
  if(mob->keywords)    free(mob->keywords);
  if(mob->loadroom)    free(mob->loadroom);
  if(mob->race)        free(mob->race);
  if(mob->prfs)        deleteBitvector(mob->prfs);
  if(mob->user_groups) deleteBitvector(mob->user_groups);
  deleteAuxiliaryData(mob->auxiliary_data);

  free(mob);
}


CHAR_DATA *charRead(STORAGE_SET *set) {
  CHAR_DATA *mob = newMobile();

  charSetClass(mob,        read_string(set, "class"));
  charSetPrototypes(mob,   read_string(set, "prototypes"));
  charSetName(mob,         read_string(set, "name"));
  charSetKeywords(mob,     read_string(set, "keywords"));
  charSetRdesc(mob,        read_string(set, "rdesc"));
  charSetDesc(mob,         read_string(set, "desc"));
  charSetMultiRdesc(mob,   read_string(set, "multirdesc"));
  charSetMultiName(mob,    read_string(set, "multiname"));
  charSetSex(mob,          read_int   (set, "sex"));
  charSetRace(mob,         read_string(set, "race"));
  bitSet(mob->prfs,        read_string(set, "prfs"));
  bitSet(mob->user_groups, read_string(set, "user_groups"));
  charSetLoadroom(mob,     read_string(set, "loadroom"));
  charSetPos(mob,          read_int   (set, "position"));
  charSetHidden(mob,       read_int   (set, "hidden"));
  charSetWeight(mob,       read_double(set, "weight"));

  // make sure we always have the default group assigned
  if(!*bitvectorGetBits(mob->user_groups))
    bitSet(mob->user_groups, DFLT_USER_GROUP);

  // read in PC data if it exists
  if(storage_contains(set, "uid"))
    charSetUID(mob,        read_int   (set, "uid"));

  if(storage_contains(set, "birth"))
    mob->birth = read_long(set, "birth");

  deleteAuxiliaryData(mob->auxiliary_data);
  mob->auxiliary_data = auxiliaryDataRead(read_set(set, "auxiliary"), 
					  AUXILIARY_TYPE_CHAR);

  // make sure our race is OK
  if(!isRace(charGetRace(mob)))
    charSetRace(mob, raceDefault());

  // reset our body to the default for our race
  charResetBody(mob);

  return mob;
}


STORAGE_SET *charStore(CHAR_DATA *mob) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "class",      mob->class);
  store_string(set, "prototypes", mob->prototypes);
  store_string(set, "name",       mob->name);
  store_string(set, "keywords",   mob->keywords);
  store_string(set, "rdesc",      mob->rdesc);
  store_string(set, "desc",       bufferString(mob->desc));
  store_string(set, "multirdesc", mob->multi_rdesc);
  store_string(set, "multiname",  mob->multi_name);
  store_int   (set, "sex",        mob->sex);
  store_string(set, "race",       mob->race);
  store_string(set, "prfs",       bitvectorGetBits(mob->prfs));
  store_string(set, "user_groups",bitvectorGetBits(mob->user_groups));
  store_int   (set, "position",   mob->position);
  store_int   (set, "hidden",     mob->hidden);
  store_double(set, "weight",     mob->weight);
  store_long  (set, "birth",      mob->birth);

  // PC-only data
  if(!charIsNPC(mob)) {
    store_int   (set, "uid",        mob->uid);
    store_string(set, "loadroom",   mob->loadroom);
  }

  store_set(set,"auxiliary", auxiliaryDataStore(mob->auxiliary_data));
  return set;
}


void charCopyTo( CHAR_DATA *from, CHAR_DATA *to) {
  charSetKeywords   (to, charGetKeywords(from));
  charSetClass      (to, charGetClass(from));
  charSetPrototypes (to, charGetPrototypes(from));
  charSetName       (to, charGetName(from));
  charSetDesc       (to, charGetDesc(from));
  charSetRdesc      (to, charGetRdesc(from));
  charSetMultiRdesc (to, charGetMultiRdesc(from));
  charSetMultiName  (to, charGetMultiName(from));
  charSetSex        (to, charGetSex(from));
  charSetPos        (to, charGetPos(from));
  charSetHidden     (to, charGetHidden(from));
  charSetWeight     (to, charGetWeight(from));
  charSetRace       (to, charGetRace(from));
  charSetBody       (to, bodyCopy(charGetBody(from)));
  bitvectorCopyTo   (from->prfs, to->prfs);
  bitvectorCopyTo   (from->prfs, to->prfs);
  to->birth = from->birth;

  auxiliaryDataCopyTo(from->auxiliary_data, to->auxiliary_data);
}

CHAR_DATA *charCopy( CHAR_DATA *mob) {
  // use newMobile() ... we want to create a new UID
  CHAR_DATA *newmob = newMobile();
  charCopyTo(mob, newmob);
  return newmob;
}



//*****************************************************************************
// mob set and get functions
//*****************************************************************************
void charSetKeywords(CHAR_DATA *ch, const char *keywords) {
  if(ch->keywords) free(ch->keywords);
  ch->keywords = strdupsafe(keywords);
}

const char  *charGetKeywords   ( CHAR_DATA *ch) {
  return ch->keywords;
}
