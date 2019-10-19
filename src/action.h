#ifndef __ACTION_H
#define __ACTION_H
//*****************************************************************************
//
// action.h
//
// this is the interface for the action handler. Actions are temporally
// delayed events (e.g. preparing a spell, swinging a sword, etc). Players
// may only be taking 1 action at a time, and any time a new action for a
// character is added, the previous one is terminated.
//
//*****************************************************************************




//
// must called before actions can be used
//
void init_actions();


//
// Pulse all of the actions 
//
void pulse_actions(int time);


//
// Interrupt whatever action the character is taking. Do nothing if
// the character is not taking an action currently. "where" is used
// in conjunction with the faculties module. If you have not installed
// the faculties module, pass in 1.
//
void interrupt_action(void *ch, bitvector_t where);


//
// Returns TRUE if the character is performing an action with "where". If
// the faculty module is not installed, "where" should be 1.
//
bool is_acting(void *ch, bitvector_t where);


//
// Start a character performing an action. If the delay reaches 0, on_complete
// is run. If the action is terminated prematurely, on_interrupt is run.
//
// on_complete must be a pointer to a function that takes the character as
// its first argument, the data as its second argument, a bitvector for
// faculties (even if you don't have that module installed ... it must take
// this argument, even if it doesn't use it) and the char arg as its 
// fourth argument.
//
// on_interrupt must take the same arguments as on_complete.
//
// "where" is used in conjunection with the faculties module. If you have not
// installed the faculties module, pass in 1.
//
// for an example of how start_action works, see admin.c (cmd_dsay)
//
void start_action(void           *ch, 
		  int          delay,
		  bitvector_t  where,
		  void  *on_complete,
		  void *on_interrupt,
		  void         *data,
		  const char    *arg);

#endif // __ACTION_H
