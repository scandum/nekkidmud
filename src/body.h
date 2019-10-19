#ifndef __BODY_H
#define __BODY_H
//*****************************************************************************
//
// body.h
//
// Different creatures are shaped in fundamentally different ways (e.g.
// bipedal humans and quadrapedal bears). Here's our attempt to create a
// structure that captures this idea.
//
//*****************************************************************************



#define BODYPOS_NONE             -1

#define BODYPOS_FLOAT             0
#define BODYPOS_HEAD              1
#define BODYPOS_FACE              2
#define BODYPOS_EAR               3
#define BODYPOS_NECK              4
#define BODYPOS_ABOUT             5
#define BODYPOS_TORSO             6
#define BODYPOS_ARM               7
#define BODYPOS_WING              8
#define BODYPOS_WRIST             9
#define BODYPOS_LEFT_HAND        10
#define BODYPOS_RIGHT_HAND       11
#define BODYPOS_FINGER           12
#define BODYPOS_WAIST            13
#define BODYPOS_LEG              14
#define BODYPOS_LEFT_FOOT        15
#define BODYPOS_RIGHT_FOOT       16
#define BODYPOS_HOOF             17
#define BODYPOS_CLAW             18
#define BODYPOS_TAIL             19
#define BODYPOS_HELD             20

#define NUM_BODYPOS              21

#define BODYSIZE_NONE            -1
#define BODYSIZE_DIMINUITIVE      0
#define BODYSIZE_TINY             1
#define BODYSIZE_SMALL            2
#define BODYSIZE_MEDIUM           3
#define BODYSIZE_LARGE            4
#define BODYSIZE_HUGE             5
#define BODYSIZE_GARGANTUAN       6
#define BODYSIZE_COLLOSAL         7
#define NUM_BODYSIZES             8

/**
 * Return a list of the postypes for a list of posnames (comma-separated)
 */
char *list_postypes(const BODY_DATA *B, const char *posnames);


/**
 * return the name of the body position type 
 */
const char *bodyposGetName(int bodypos);


/**
 * return the name of the specified bodysize
 */
const char *bodysizeGetName(int size);


/**
 * return the number assocciated with the bodysize
 */
int bodysizeGetNum(const char *size);


/**
 * returns the number of the bodyposition
 */
int bodyposGetNum(const char *bodypos);


/**
 * Create a new body
 */
BODY_DATA *newBody();


/**
 * Delete a body. Do not delete any pieces of equipment
 * equipped on it.
 */
void deleteBody(BODY_DATA *B);


/**
 * Copy the body (minus equipment)
 */
BODY_DATA *bodyCopy(const BODY_DATA *B);


/**
 * Return the size of the body
 */
int bodyGetSize(const BODY_DATA *B);

/**
 * change the body's size
 */
void bodySetSize(BODY_DATA *B, int size);

/**
 * Add a new position to the body. <type> is one of the basic
 * position types listed at the start of this header, and
 * <weight> is how much of the body's mass the piece takes up,
 * relative to the rest of the body. If the position already exists
 * on the body, just change its type and size.
 */
void bodyAddPosition(BODY_DATA *B, const char *pos, int type, int size);


/**
 * Remove a position from the body. Return true if the
 * position is removed, and false if it does not exist.
 */
bool bodyRemovePosition(BODY_DATA *B, const char *pos);


/**
 * Return the type of position the bodypart is. Return NONE if
 * no such bodypart exists on the body
 */
int bodyGetPart(const BODY_DATA *B, const char *pos);


/**
 * Return the name of a random bodypart, based on the bodypart's size,
 * relative to the other bodyparts. If pos is NULL, all bodyparts are weighted
 * in. If part is not null, it is assumed to be a list that we want to draw from
 */
const char *bodyRandPart(const BODY_DATA *B, const char *pos);


/**
 * Return the ratio of the bodypart(s)'s size the the body's total size.
 * If the part(s) does not exist then, 0 is returned.
 */
double bodyPartRatio(const BODY_DATA *B, const char *pos);


/**
 * get a list of all the bodyparts on the body. If sort is true,
 * order them from top (floating, head, etc) to bottom (legs and feet)
 */
const char **bodyGetParts(const BODY_DATA *B, bool sort, int *num_pos);


/**
 * Equip the object to the first available, valid body positions. If
 * none exist, return false. Otherwise, return true.
 */
bool bodyEquipPostypes(BODY_DATA *B, OBJ_DATA *obj, const char *types);


/**
 * Equip the object to the list of positions on the body. If one or more
 * of the posnames doesn't exist, or already is equipped, return false.
 */
bool bodyEquipPosnames(BODY_DATA *B, OBJ_DATA *obj, const char *positions);


/**
 * Returns a list of places the piece of equipment is equipped on
 * the person's body
 */
const char *bodyEquippedWhere(BODY_DATA *B, OBJ_DATA *obj);


/**
 * Return a pointer to the object that is equipped at the given bodypart.
 * If nothing is equipped, or the bodypart does not exist, return null.
 */
OBJ_DATA *bodyGetEquipment(BODY_DATA *B, const char *pos);


/**
 * Remove the object from all of the bodyparts it is equipped at. Return
 * true if successful, and false if the object is not equipped anywhere
 * on the body.
 */
bool bodyUnequip(BODY_DATA *B, const OBJ_DATA *obj);


/**
 * Unequip everything on the body, and return a list of all the
 * objects that were unequipped.
 */
LIST *bodyUnequipAll(BODY_DATA *B);


/**
 * returns a list of all equipment worn on the body. The list must
 * be deleted after use.
 */
LIST *bodyGetAllEq(BODY_DATA *B);


/**
 * Return how many positions are on the body
 */
int numBodyparts(const BODY_DATA *B);

#endif // __BODY_H
