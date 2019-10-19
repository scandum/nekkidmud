#ifndef __MUDTIME_H
#define __MUDTIME_H
//*****************************************************************************
//
// mudtime.h
//
// We can't use time.h, because it is a standard C header. A small module for
// handling time of day in the MUD.
//
//*****************************************************************************

//
// This must be put at the top of mud.h so the rest of the MUD knows that
// we've got the time module installed
// #define MODULE_TIME
//

#define MUD_HOUR       * 5 MINUTES
#define MUD_HOURS      * 5 MINUTES



//
// Set up the time module
//
void init_time();


//
// what day is it?
//
const char *get_day();


//
// what month is it?
//
const char *get_month();


//
// what hour is it
//
int get_hour();


//
// what year is it?
//
int get_year();


//
// What period of the day are we in?
//
bool is_morning();
bool is_afternoon();
bool is_evening();
bool is_night();

#endif // __MUDTIME_H
