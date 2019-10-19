#ifndef __HANDLER_H
#define __HANDLER_H
//*****************************************************************************
//
// handler.h
//
// this header contains definitions for all of the "handling" functions;
// functions that move things from place to place. e.g. adding and removing
// characters from rooms, as well as objects and the like.
//
//*****************************************************************************

// when something needs to be removed from the game, it should NOT be deleted.
// If it has an extraction function, it should be extracted. If it does not,
// then its from_game function should be called, AND THEN it should be deleted.
// When something needs to be put into the game, its to_game function should be
// called AND THEN it should be added to the game (e.g. loading a character and
// putting him into a room... call char_to_game first); exist functions are to
// state that something is ready for interaction with but it is *not* ready to
// be placed in the game yet. Conversely, unexist functions are to state that
// something is no longer ready for interaction but was never part of the game
// For instance, when a brand new character has been made for character 
// creation. Whenever something is put to_game, it is first make to exist.
// Whenever something is remove from_game, it is also immediately unexisted.
void      char_exist        (CHAR_DATA *ch);
void      char_unexist      (CHAR_DATA *ch);
bool      char_exists       (CHAR_DATA *ch);
void      char_to_game      (CHAR_DATA *ch);
void      char_from_game    (CHAR_DATA *ch);
void      obj_exist         (OBJ_DATA  *obj);
void      obj_unexist       (OBJ_DATA  *obj);
bool      obj_exists        (OBJ_DATA  *obj);
void      obj_to_game       (OBJ_DATA  *obj);
void      obj_from_game     (OBJ_DATA  *obj);
void      room_exist        (ROOM_DATA *room);
void      room_unexist      (ROOM_DATA *room);
bool      room_exists       (ROOM_DATA *room);
void      room_to_game      (ROOM_DATA *room);
void      room_from_game    (ROOM_DATA *room);
void      exit_exist        (EXIT_DATA *exit);
void      exit_unexist      (EXIT_DATA *exit);
bool      exit_exists       (EXIT_DATA *exit);
void      exit_to_game      (EXIT_DATA *exit);
void      exit_from_game    (EXIT_DATA *exit);


// all of these things require that the character(s) and object(s) have
// the right spatial relations to eachtoher (e.g. do_give requires the
// object be in the char's inventory and the receiver to be in the same room
// as the character, do_get requires the object be on the ground of the room 
// the character is in)
void do_remove (CHAR_DATA *ch, OBJ_DATA *obj);
void do_wear   (CHAR_DATA *ch, OBJ_DATA *obj, const char *where);
void do_drop   (CHAR_DATA *ch, OBJ_DATA *obj);
void do_give   (CHAR_DATA *ch, CHAR_DATA *recv, OBJ_DATA *obj);
void do_get    (CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container);
void do_put    (CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container);

void      obj_from_char       (OBJ_DATA *obj);
void      obj_from_obj        (OBJ_DATA *obj);
void      obj_from_room       (OBJ_DATA *obj);

void      obj_to_char         (OBJ_DATA *obj, CHAR_DATA *ch);
void      obj_to_obj          (OBJ_DATA *obj, OBJ_DATA *to);
void      obj_to_room         (OBJ_DATA *obj, ROOM_DATA *room);

void      char_from_room      (CHAR_DATA *ch);
void      char_to_room        (CHAR_DATA *ch, ROOM_DATA *room);
void      char_from_furniture (CHAR_DATA *ch);
void      char_to_furniture   (CHAR_DATA *ch, OBJ_DATA *furniture);

bool      do_equip          (CHAR_DATA *ch, OBJ_DATA *obj, const char *pos, 
			     bool by_name);
bool      try_equip         (CHAR_DATA *ch, OBJ_DATA *obj, 
			     const char *wanted_pos, const char *required_pos);
bool      try_unequip       (CHAR_DATA *ch, OBJ_DATA *obj);
bool      do_unequip        (CHAR_DATA *ch, OBJ_DATA *obj);
void      unequip_all       (CHAR_DATA *ch);


#define FOUND_NONE                   0
#define FOUND_EXIT                   1
#define FOUND_CHAR                   2
#define FOUND_OBJ                    3
#define FOUND_EDESC                  4
#define FOUND_ROOM                   5
#define FOUND_IN_OBJ                 6 // useful for portals and containers
#define FOUND_LIST                   7 // returns a list of things ... may
                                       // occur when looking for all.XXX
                                       // will only be returned if there is only
                                       // FIND_TYPE to look for. The list must 
                                       // be deleted afterwards

#define FIND_TYPE_CHAR         (1 << 0)
#define FIND_TYPE_OBJ          (1 << 1)
#define FIND_TYPE_EXIT         (1 << 2)
#define FIND_TYPE_EDESC        (1 << 3)
#define FIND_TYPE_IN_OBJ       (1 << 4)
#define FIND_TYPE_ROOM         (1 << 5)

#define FIND_TYPE_ALL          (FIND_TYPE_CHAR  | FIND_TYPE_OBJ  | \
				FIND_TYPE_ROOM  | FIND_TYPE_EXIT | \
				FIND_TYPE_EDESC | FIND_TYPE_IN_OBJ)


#define FIND_SCOPE_ROOM        (1 << 10)
#define FIND_SCOPE_INV         (1 << 11)
#define FIND_SCOPE_WORN        (1 << 12)
#define FIND_SCOPE_WORLD       (1 << 13)

#define FIND_SCOPE_VISIBLE     (1 << 18)


#define FIND_SCOPE_IMMEDIATE   (FIND_SCOPE_ROOM | FIND_SCOPE_INV | \
				FIND_SCOPE_WORN | FIND_SCOPE_VISIBLE)
#define FIND_SCOPE_ALL         (FIND_SCOPE_ROOM | FIND_SCOPE_INV | \
			        FIND_SCOPE_WORN | FIND_SCOPE_WORLD)

//
// Searches the looker's scope of vision for a thing in <find_types> with <arg>
// as the target. Sets found_type to the type of item returned. 
// arg can be manipulated in various ways, such as adding an occurance number
// to the start of arg (e.g. 2.woman) or specifying scopes of search
// (e.g. woman in bed) or combinations of the two (e.g. 2.woman in 6.bed)
// if all_ok is true, then it is possible to return a list of things
// (all of one type) if someone uses something like all.woman
//
void *generic_find(CHAR_DATA *looker, const char *arg,
		   bitvector_t find_types, 
		   bitvector_t find_scope, 
		   bool all_ok, int *found_type);

void *find_specific(CHAR_DATA *looker,
		    const char *at, 
		    const char *on, 
		    const char *in,
		    bitvector_t find_types,
		    bitvector_t find_scope,
		    bool all_ok, int *found_type);

#endif // __HANDLER_H
