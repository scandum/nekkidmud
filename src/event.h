#ifndef __EVENT_H
#define __EVENT_H
//*****************************************************************************
//
// event.h
//
// this is the interface for the event handler. Events are temporally
// delayed functions. Events and actions (see action.h) may seem similar. They
// do, however, share some distinct differences. Whereas actions can only
// be attached to characters, and only one action can be attached to a 
// character at a time, events can be attached to anything, and anything can
// have any number of events attached to it at a time. And, indeed, they do
// not even need to be attached to anything! Some examples of what might
// constitute an event are things like a quest that is scheduled to start in
// 5 minutes, a disease that will kill someone in 5 minutes unless they find
// a cure for it, or perhaps a scheduled game reboot.
//
//*****************************************************************************



//
// must called before events can be used
//
void init_events();


//
// Pulse all of the events 
//
void pulse_events(int time);


//
// Stop all events involving "thing". "thing" can be anything that
// might be involved in an event (either as the owner of the event
// or some part of the event's data).
//
void interrupt_events_involving(void *thing);


//
// Put an event into the event handler. When the delay reaches 0, 
// on_complete is called.
//
// on_complete must be a function that takes the owner as its first
// argument, the data as its second, and the argument as its third.
//
// check_involvement must be a function that takes the thing to check
// the involvement of as the first argument, and the data to check in
// as its second argument. It must return TRUE if the data contains
// a pointer to the thing, and FALSE otherwise.
//
void start_event(void *owner, 
		 int   delay,
 		 void *on_complete,
		 void *check_involvement,
		 void *data,
		 const char *arg);


//
// same deal as start_event, but will automatically re-queue the event
// after it has fired. Useful for events that are currently running (e.g.
// mudtime updating, ticks, zone reset timers).
//
void start_update(void *owner,
		  int   delay,
		  void *on_complete,
		  void *check_involvement,
		  void *data,
		  const char *arg);

#endif // __EVENT_H
