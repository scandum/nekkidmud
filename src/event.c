//*****************************************************************************
//
// event.h
//
// this is the interface for the event handler. Events are temporally
// delayed events. Events and actions (see action.h) may seem similar. They
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

#include "mud.h"
#include "utils.h"
#include "character.h"
#include "hooks.h"
#include "event.h"

typedef struct event_data EVENT_DATA;
LIST    *events = NULL;

struct event_data {
  void *owner;   // who is the lucky person who owns this event?
  void (*  on_complete)(void *owner, void *data, char *arg);
  bool (*  check_involvement)(void *thing, void *data);
  int   delay;   // how much longer until the event is complete?
  int   tot_time;// what is the total delay before the event fires?
  void *data;    // data for the event
  char *arg;     // an argument supplied to an event
  bool  requeue; // is the event requeue'd after it goes off?
};



//*****************************************************************************
//
// event handling
//
//*****************************************************************************
EVENT_DATA *newEvent(void *owner,
		     int delay, 	
		     void (*  on_complete)(void *owner, void *data, char *arg),
		     bool (* check_involvement)(void *thing, void *data),
		     void *data, const char *arg, bool requeue) {
  EVENT_DATA *event        = malloc(sizeof(EVENT_DATA));
  event->owner             = owner;
  event->on_complete       = on_complete;
  event->check_involvement = check_involvement;
  event->delay             = delay;
  event->tot_time          = delay;
  event->data              = data;
  event->arg               = strdupsafe(arg);
  event->requeue           = requeue;
  return event;
}

void deleteEvent(EVENT_DATA *event) {
  if(event->arg) free(event->arg);
  free(event);
}

void run_event(EVENT_DATA *event) {
  if(event->on_complete)
    event->on_complete(event->owner, event->data, event->arg);
}

void interrupt_events_obj_hook(const char *info) {
  OBJ_DATA *obj = NULL;
  hookParseInfo(info, &obj);
  interrupt_events_involving(obj);
}

void interrupt_events_char_hook(const char *info) {
  CHAR_DATA *ch = NULL;
  hookParseInfo(info, &ch);
  interrupt_events_involving(ch);
}

void interrupt_events_room_hook(const char *info) {
  ROOM_DATA *room = NULL;
  hookParseInfo(info, &room);
  interrupt_events_involving(room);
}



//*****************************************************************************
// event list handling
//*****************************************************************************
void init_events() {
  events = newList();

  // make sure all events involving the object/char are cancelled when
  // either is extracted from the game
  hookAdd("obj_from_game",  interrupt_events_obj_hook);
  hookAdd("char_from_game", interrupt_events_char_hook);
  hookAdd("room_from_game", interrupt_events_room_hook);
}

void interrupt_event(EVENT_DATA *event) {
  deleteEvent(event);
}

void interrupt_events_involving(void *thing) {
  LIST_ITERATOR *ev_i = newListIterator(events);
  EVENT_DATA   *event = NULL;

  ITERATE_LIST(event, ev_i) {
    // if we've found involvement, pop it on out
    if(event->owner == thing ||
       (event->check_involvement != NULL && 
	event->check_involvement(thing, event->data))) {
      listRemove(events, event);
      deleteEvent(event);
    }
  }
  deleteListIterator(ev_i);
}

void start_event(void *owner, 
		 int   delay,
		 void *on_complete,
		 void *check_involvement,
		 void *data,
		 const char *arg) {
  // some events might cause other events to activate. This is signaled by
  // providing a delay of 0. In this case, we queue events to the back of the
  // event list instead of push them on to the front
  EVENT_DATA *event = newEvent(owner, delay, on_complete, check_involvement,
			       data, arg, FALSE);
  if(delay == 0)
    listQueue(events, event);
  else
    listPut(events, event);
}

void start_update(void *owner, 
		  int   delay,
		  void *on_complete,
		  void *check_involvement,
		  void *data,
		  const char *arg) {
  // some events might cause other events to activate. This is signaled by
  // providing a delay of 0. In this case, we queue events to the back of the
  // event list instead of push them on to the front
  EVENT_DATA *event = newEvent(owner, delay, on_complete, check_involvement,
			       data, arg, TRUE);
  if(delay == 0)
    listQueue(events, event);
  else
    listPut(events, event);
}

void pulse_events(int time) {
  LIST_ITERATOR *ev_i = newListIterator(events);
  EVENT_DATA   *event = NULL;

  // go over all of the events
  ITERATE_LIST(event, ev_i) {
    // decrement the delay
    event->delay -= time;
    // pop the character from the list, and run the event
    if(event->delay <= 0) {
      listRemove(events, event);
      run_event(event);
      // if we need to requeue, reset the timer 
      // and put us back in the list
      if(event->requeue) {
	event->delay = event->tot_time;
	listPut(events, event);
      }
      // otherwise, just delete the event
      else
	deleteEvent(event);
    }
  } deleteListIterator(ev_i);
}
