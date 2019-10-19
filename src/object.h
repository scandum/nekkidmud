#ifndef __OBJECT_H
#define __OBJECT_H
//*****************************************************************************
//
// object.h
//
// Contains the basic interface for working with the object data structure, and
// the functions needed to interact with it. If you plan on adding any other
// information to objects, it is strongly suggested you do so through auxiliary
// data (see auxiliary.h).
//
// For a recap, IF YOU PLAN ON ADDING ANY OTHER INFORMATION TO OBJECTS, IT
// IS STRONGLY SUGGESTED YOU DO SO THROUGH AUXILIARY DATA (see auxiliary.h).
//
//*****************************************************************************

OBJ_DATA    *newObj         ();
void         deleteObj      (OBJ_DATA *obj);

OBJ_DATA    *objRead        (STORAGE_SET *set);
STORAGE_SET *objStore       (OBJ_DATA *obj);

void         objCopyTo      (OBJ_DATA *from, OBJ_DATA *to);
OBJ_DATA    *objCopy        (OBJ_DATA *obj);

bool         objIsInstance  (OBJ_DATA *obj, const char *prototype);
bool         objIsName      (OBJ_DATA *obj, const char *name);
void         objAddChar     (OBJ_DATA *obj, CHAR_DATA *ch);
void         objRemoveChar  (OBJ_DATA *obj, CHAR_DATA *ch);



//*****************************************************************************
// set and get functions
//*****************************************************************************
const char  *objGetClass     (OBJ_DATA *obj);
const char  *objGetPrototypes(OBJ_DATA *obj);
const char  *objGetName      (OBJ_DATA *obj);
const char  *objGetKeywords  (OBJ_DATA *obj);
const char  *objGetRdesc     (OBJ_DATA *obj);
const char  *objGetDesc      (OBJ_DATA *obj);
const char  *objGetMultiName (OBJ_DATA *obj);
const char  *objGetMultiRdesc(OBJ_DATA *obj);
EDESC_SET   *objGetEdescs    (OBJ_DATA *obj);
const char  *objGetEdesc     (OBJ_DATA *obj, const char *keyword);
const char  *objGetInstances (OBJ_DATA *obj);
BUFFER      *objGetDescBuffer(OBJ_DATA *obj);
CHAR_DATA   *objGetCarrier   (OBJ_DATA *obj);
CHAR_DATA   *objGetWearer    (OBJ_DATA *obj);
OBJ_DATA    *objGetContainer (OBJ_DATA *obj);
ROOM_DATA   *objGetRoom      (OBJ_DATA *obj);
LIST        *objGetContents  (OBJ_DATA *obj);
LIST        *objGetUsers     (OBJ_DATA *obj);
int          objGetUID       (OBJ_DATA *obj);
time_t       objGetBirth     (OBJ_DATA *obj);
double       objGetWeight    (OBJ_DATA *obj);
double       objGetWeightRaw (OBJ_DATA *obj);
void        *objGetAuxiliaryData(const OBJ_DATA *obj, const char *name);
BITVECTOR   *objGetBits      (OBJ_DATA *obj);
int          objGetHidden    (OBJ_DATA *Obj);

void         objSetClass     (OBJ_DATA *obj, const char *type);
void         objSetPrototypes(OBJ_DATA *obj, const char *type);
void         objAddPrototype (OBJ_DATA *obj, const char *type);
void         objSetName      (OBJ_DATA *obj, const char *name);
void         objSetKeywords  (OBJ_DATA *obj, const char *keywords);
void         objSetRdesc     (OBJ_DATA *obj, const char *rdesc);
void         objSetDesc      (OBJ_DATA *obj, const char *desc);
void         objSetMultiName (OBJ_DATA *obj, const char *multi_name);
void         objSetMultiRdesc(OBJ_DATA *obj, const char *multi_rdesc);
void         objSetEdescs    (OBJ_DATA *obj, EDESC_SET *edesc);
void         objSetCarrier   (OBJ_DATA *obj, CHAR_DATA *ch);
void         objSetWearer    (OBJ_DATA *obj, CHAR_DATA *ch);
void         objSetContainer (OBJ_DATA *obj, OBJ_DATA  *cont);
void         objSetRoom      (OBJ_DATA *obj, ROOM_DATA *room);
void         objSetWeightRaw (OBJ_DATA *obj, double weight);
void         objSetHidden    (OBJ_DATA *obj, int amnt);

#endif // __OBJECT_H
