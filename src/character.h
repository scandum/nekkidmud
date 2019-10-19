#ifndef __CHARACTER_H
#define __CHARACTER_H
//*****************************************************************************
//
// character.h
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



//*****************************************************************************
// character defines
//*****************************************************************************

// the group a character belongs to by default
#define DFLT_USER_GROUP        "player"



//*****************************************************************************
// character functions
//*****************************************************************************

//
// the difference between newChar and newMobile is that newMobile
// assigned a new UID whereas newChar does not.
CHAR_DATA   *newChar          (void);
CHAR_DATA    *newMobile       (void);
void         deleteChar       (CHAR_DATA *mob);

CHAR_DATA    *charRead        (STORAGE_SET *set);
STORAGE_SET *charStore        (CHAR_DATA *mob);

CHAR_DATA    *charCopy        (CHAR_DATA *mob);
void         charCopyTo       (CHAR_DATA *from, CHAR_DATA *to);


bool         charIsNPC        (CHAR_DATA *ch);
bool         charIsName       (CHAR_DATA *ch, const char *name);
bool         charIsInstance   (CHAR_DATA *ch, const char *prototype);
void         putCharInventory (CHAR_DATA *ch, OBJ_DATA *obj);
void         charResetBody    (CHAR_DATA *ch);



//*****************************************************************************
// output functions
//*****************************************************************************
void  text_to_char          (CHAR_DATA *dMob, const char *txt );
void  send_to_char          (CHAR_DATA *ch, const char *format, ...) 
__attribute__ ((format (printf, 2, 3)));
void  send_to_list          (LIST *list, const char *format, ...) 
__attribute__ ((format (printf, 2, 3)));
void send_around_char(CHAR_DATA *ch, bool hide_nosee, const char *format, ...) 
__attribute__ ((format (printf, 3, 4)));
void send_to_groups(const char *groups, const char *Format, ...)
__attribute__ ((format (printf, 2, 3)));



//*****************************************************************************
// set and get functions
//*****************************************************************************
SOCKET_DATA *charGetSocket    (CHAR_DATA *ch);
ROOM_DATA   *charGetRoom      (CHAR_DATA *ch);
ROOM_DATA   *charGetLastRoom  (CHAR_DATA *ch);
const char  *charGetClass     (CHAR_DATA *ch);
const char  *charGetPrototypes(CHAR_DATA *ch);
const char  *charGetName      (CHAR_DATA *ch);
const char  *charGetDesc      (CHAR_DATA *ch);
const char  *charGetRdesc     (CHAR_DATA *ch);
const char  *charGetMultiRdesc(CHAR_DATA *ch);
const char  *charGetMultiName (CHAR_DATA *ch);
int          charGetSex       (CHAR_DATA *ch);
// for editing with the text editor
BUFFER      *charGetDescBuffer(CHAR_DATA *ch);
BUFFER      *charGetLookBuffer(CHAR_DATA *ch);
LIST        *charGetInventory (CHAR_DATA *ch);
BODY_DATA   *charGetBody      (CHAR_DATA *ch);
const char  *charGetRace      (CHAR_DATA *ch);
const char  *charGetLoadroom  (CHAR_DATA *ch);
OBJ_DATA    *charGetFurniture (CHAR_DATA *ch);
int          charGetPos       (CHAR_DATA *ch);
int          charGetUID       (const CHAR_DATA *ch);
time_t       charGetBirth     (const CHAR_DATA *ch);
int          charGetHidden    (const CHAR_DATA *ch);
double       charGetWeight    (const CHAR_DATA *ch);
void        *charGetAuxiliaryData(const CHAR_DATA *ch, const char *name);
BITVECTOR   *charGetPrfs      (CHAR_DATA *ch);
BITVECTOR   *charGetUserGroups(CHAR_DATA *ch);

void         charSetClass     (CHAR_DATA *ch, const char *prototype);
void         charAddPrototype (CHAR_DATA *ch, const char *prototype);
void         charSetPrototypes(CHAR_DATA *ch, const char *prototypes);
void         charSetSocket    (CHAR_DATA *ch, SOCKET_DATA *socket);
void         charSetRoom      (CHAR_DATA *ch, ROOM_DATA *room);
void         charSetLastRoom  (CHAR_DATA *ch, ROOM_DATA *room);
void         charSetName      (CHAR_DATA *ch, const char *name);
void         charSetSex       (CHAR_DATA *ch, int sex);
void         charSetDesc      (CHAR_DATA *ch, const char *desc);
void         charSetRdesc     (CHAR_DATA *ch, const char *rdesc);
void         charSetMultiRdesc(CHAR_DATA *ch, const char *multi_rdesc);
void         charSetMultiName (CHAR_DATA *ch, const char *multi_name);
void         charSetBody      (CHAR_DATA *ch, BODY_DATA *body);
void         charSetRace      (CHAR_DATA *ch, const char *race);
void         charSetUID       (CHAR_DATA *ch, int uid);
void         charSetLoadroom  (CHAR_DATA *ch, const char *key);
void         charSetFurniture (CHAR_DATA *ch, OBJ_DATA *furniture);
void         charSetPos       (CHAR_DATA *ch, int pos);
void         charSetHidden    (CHAR_DATA *ch, int amnt);
void         charSetWeight    (CHAR_DATA *ch, double amnt);



//*****************************************************************************
// mob-specific functions
//*****************************************************************************
const char  *charGetKeywords   (CHAR_DATA *ch);
void         charSetKeywords   (CHAR_DATA *ch, const char *keywords);



//*****************************************************************************
// Sexes
//*****************************************************************************
#define SEX_NONE                (-1)
#define SEX_MALE                  0
#define SEX_FEMALE                1
#define SEX_NEUTRAL               2
#define NUM_SEXES                 3

const char *sexGetName(int sex);
int  sexGetNum(const char *sex);



//*****************************************************************************
// Positions
//*****************************************************************************
#define POS_NONE                (-1)
#define POS_UNCONSCIOUS           0
#define POS_SLEEPING              1
#define POS_SITTING               2
#define POS_STANDING              3
#define POS_FLYING                4
#define NUM_POSITIONS             5

const char *posGetName(int pos);
const char *posGetActionSelf(int pos);
const char *posGetActionOther(int pos);
int posGetNum(const char *pos);
int poscmp(int pos1, int pos2);

#endif // __CHARACTER_H
