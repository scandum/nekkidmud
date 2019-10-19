//*****************************************************************************
//
// action.h
//
// this is the interface for the action handler. Actions are temporally
// delayed events (e.g. preparing a spell, swinging a sword, etc). Players
// may only be taking 1 action at a time, and any time a new action for a
// character is added, the previous one is terminated.
//
// Dec 15/04
//  * expanded actions to support actions for different places (e.g mental, 
//    feet, left/right hands). 
//
// Nov 28/04
//  * There are a couple places where actions can be improved on. First, it
//    would probably be good to store actions as a binary map instead of as
//    a hash map. Iterating over the hash map is probably going to be
//    inefficient in the long run. The second place it can use some improvement
//    is in the action loop; it would be nice if we had a way to prevent
//    players from re-entering the loop as it is going around (perhaps a poll
//    that is only active when the action loop is active), and also if we could
//    ensure that actions are run in order of their remaining delay; currently,
//    if the action loop is run with a pulse of 2, and there are two people
//    taking actions, one with a delay of 2 left and one with a delay of 1 left,
//    there is a chance that the one with a delay of 2 left will perform an
//    action first. Not that serious, but definitely more of a bug than a
//    feature.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "character.h"
#include "action.h"
#include "hooks.h"

#ifdef MODULE_FACULTY
#include "faculty/faculty.h"
#endif

typedef struct action_data ACTION_DATA;
MAP *actors = NULL;

struct action_data {
  void (*  on_complete)(void *ch, void *data, bitvector_t where, char *arg);
  void (* on_interrupt)(void *ch, void *data, bitvector_t where, char *arg);
  bitvector_t where; // which bodyparts are participating in the action?
  int   delay;       // how much longer until the action is complete?
  void *data;  // data for the action (e.g. spell data, char mining state)
  char *arg;   // an argument supplied to an action (e.g. the target of a kick)
};



//*****************************************************************************
// single action handling
//*****************************************************************************
ACTION_DATA *newAction(int delay, 	
		       bitvector_t where,
		       void *on_complete,
		       void *on_interrupt,
		       void *data, const char *arg) {
  struct action_data *action = malloc(sizeof(ACTION_DATA));
  action->on_complete  = on_complete;
  action->on_interrupt = on_interrupt;
  action->delay        = delay;
  action->data         = data;
  action->arg          = strdupsafe(arg);
  action->where        = where;
  return action;
}

void deleteAction(ACTION_DATA *action) {
  if(action->arg) free(action->arg);
  free(action);
}

void run_action(void *ch, ACTION_DATA *action) {
  if(action->on_complete)
    action->on_complete(ch, action->data, action->where, action->arg);
}


// used to kill all of a character's actions on death
void stop_all_actions(CHAR_DATA *ch) {
#ifdef MODULE_FACULTY
  interrupt_action(ch, FACULTY_ALL);
#else
  interrupt_action(ch, 1);  
#endif
}

// allows stop_all_actions to run as a hook
void stop_actions_hook(const char *info) {
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &ch);
  stop_all_actions(ch);
}



//*****************************************************************************
// actor list handling
//*****************************************************************************
void init_actions() {
  // use the standard pointer hasher and comparator
  actors = newMap(NULL, NULL);

  // make sure the character does not continue actions after being extracted
  hookAdd("char_from_game", stop_actions_hook);
}

bool is_acting(void *ch, bitvector_t where) {
  LIST *actions = mapGet(actors, ch);
  if(actions == NULL || listSize(actions) == 0)
    return FALSE;

  bool action_found    = FALSE;
  LIST_ITERATOR *act_i = newListIterator(actions);
  ACTION_DATA  *action = NULL;

  // iterate across all of our current actions and see if any
  // involve the faculties of "where"
  ITERATE_LIST(action, act_i) {
    if(IS_SET(action->where, where)) {
      action_found = TRUE;
      break;
    }
  } deleteListIterator(act_i);

  return action_found;
}

void interrupt_action(void *ch, bitvector_t where) {
  // get the list of all the actions we're performing
  LIST *actions = mapGet(actors, ch);
  if(actions == NULL)
    return;

  // if we have actions, go through and stop 'em all
  if(listSize(actions) > 0) {
    LIST_ITERATOR *act_i = newListIterator(actions);
    ACTION_DATA *action  = NULL;

    ITERATE_LIST(action, act_i) {
      // check to see if we've found an action that needs interruption
      if(IS_SET(action->where, where)) {
	if(action->on_interrupt)
	  action->on_interrupt(ch, action->data, action->where, action->arg);
	listRemove(actions, action);
	deleteAction(action);
      }
    } deleteListIterator(act_i);
  }

  // if all of the actions are gone, delete the list
  if(listSize(actions) == 0) {
    /*
     * Nope! We can't do this. If we interrupt an action midway through
     * processing actions and the list gets deleted, the iterator over it will
     * blow up when we try to delete it. Instead, deletion will now occur when
     * we try to process actions and realize there is nothing in the list to
     * process.
     *
    mapRemove(actors, ch);
    deleteList(actions);
    */
  }
}

void start_action(void           *ch, 
		  int          delay,
		  bitvector_t  where,
		  void  *on_complete,
		  void *on_interrupt,
		  void         *data,
		  const char    *arg) {
  interrupt_action(ch, where);
  ACTION_DATA *newact = newAction(delay, where, on_complete, 
				  on_interrupt, data, arg);
  // get the current list
  LIST *curr_acts     = mapGet(actors, ch);
  
  // if it's null, make a new one and add it to the map of actions
  if(curr_acts == NULL) {
    curr_acts = newList();
    mapPut(actors, ch, curr_acts);
  }

  listPut(curr_acts, newact);
}

void pulse_actions(int time) {
  MAP_ITERATOR    *ch_i = newMapIterator(actors);
  ACTION_DATA   *action = NULL;
  LIST_ITERATOR  *act_i = NULL;
  LIST         *actions = NULL;
  const CHAR_DATA *map_actor = NULL;
  CHAR_DATA         *ch = NULL;

  ITERATE_MAP(map_actor, actions, ch_i) {
    actions = mapIteratorCurrentVal(ch_i);

    // weird, we shouldn't be in here
    if(listSize(actions) == 0) {
      mapRemove(actors, map_actor);
      deleteList(actions);
    }
    // if we have actions, then go through 'em all
    else {
      // eek... small problem. The key for maps is constant, but we need
      // a non-constant character to send to run_action. Let's get the char's
      // UID, and re-look him up in the player table
      ch = propertyTableGet(mob_table, charGetUID(map_actor));

      act_i = newListIterator(actions);

      ITERATE_LIST(action, act_i) {
	// decrement the delay
	action->delay -= time;
	// pop the action from the list, and run it
	if(action->delay <= 0) {
	  listRemove(actions, action);
	  run_action(ch, action);
	  deleteAction(action);
	}
      } deleteListIterator(act_i);
    }
  } deleteMapIterator(ch_i);
}
