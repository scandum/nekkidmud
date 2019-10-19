#ifndef PROTOTYPE_H
#define PROTOTYPE_H
//*****************************************************************************
//
// prototype.h
//
// generic prototype datastructure for anything that can be generated with a
// python script (e.g. object, room, character). Supports inheritance from
// other prototypes.
//
//*****************************************************************************

PROTO_DATA    *newProto(void);
void        deleteProto(PROTO_DATA *data);
void        protoCopyTo(PROTO_DATA *from, PROTO_DATA *to);
PROTO_DATA   *protoCopy(PROTO_DATA *data);
STORAGE_SET *protoStore(PROTO_DATA *data);
PROTO_DATA   *protoRead(STORAGE_SET *set);
bool           protoRun(PROTO_DATA *proto, const char *type, void *pynewfunc, 
			void *protoaddfunc, void *protoclassfunc, void *me);
CHAR_DATA  *protoMobRun(PROTO_DATA *proto);
OBJ_DATA   *protoObjRun(PROTO_DATA *proto);
ROOM_DATA *protoRoomRun(PROTO_DATA *proto);
ROOM_DATA *protoRoomInstance(PROTO_DATA *proto, const char *as);

//
// setters
void      protoSetKey(PROTO_DATA *data, const char *key);
void  protoSetParents(PROTO_DATA *data, const char *parents);
void   protoSetScript(PROTO_DATA *data, const char *script);
void protoSetAbstract(PROTO_DATA *data, bool abstract);

//
// getters
const char      *protoGetKey(PROTO_DATA *data);
const char  *protoGetParents(PROTO_DATA *data);
const char   *protoGetScript(PROTO_DATA *data);
bool         protoIsAbstract(PROTO_DATA *data);
BUFFER *protoGetScriptBuffer(PROTO_DATA *data);

#endif // PROTOTYPE_H
