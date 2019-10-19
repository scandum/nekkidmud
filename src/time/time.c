//*****************************************************************************
//
// time.c
//
// A small module for handling time of day in the MUD.
//
//*****************************************************************************

#include <Python.h>        // to add Python hooks

#include "../mud.h"
#include "../utils.h"
#include "../inform.h"
#include "../character.h"
#include "../room.h"
#include "../event.h"
#include "../storage.h"
#include "../auxiliary.h"
#include "../hooks.h"

#include "mudtime.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pyroom.h"
#include "../scripts/pymud.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_SET_VAL
#include "../set_val/set_val.h"
#endif



//*****************************************************************************
// local defines, variables, and structs
//*****************************************************************************
#define TIME_FILE   "../lib/misc/time"  // where do we keep time data?
#define TIME_UPDATE_DELAY   1 MUD_HOUR  // how long is an in-game hour?
#define HOURS_PER_DAY               24  // how many hours are in a day?
#define NUM_MONTHS                   4  // how many months are in a year?
#define DAYS_PER_WEEK                3  // how many days are in a week?
#define USE_AMPM                  TRUE  // do we use the am/pm system?

COMMAND(cmd_time);                      // a player command for seeing the time


struct month_data {
  char *name;             // what is the name of the month?
  int   num_days;         // how many days are in the month?
  int   morning_starts;   // what time does morning start at?
  int   afternoon_starts; // what time does the afternoon start at?
  int   evening_starts;   // what times does evening start at?
  int   night_starts;     // what time does the night start at?
};

struct day_data {
  char *name;
};

const struct month_data month_info[NUM_MONTHS] = {
  // month name                 days   morning   afternoon    evening   night
  { "the month of dawn",          30,        6,         12,        18,     22 },
  { "the month of light",         30,        4,         10,        19,     23 },
  { "the month of the moon",      30,        6,         12,        17,     21 },
  { "the month of twilight",      30,        5,         13,        16,     20 },
};

const struct day_data day_info[DAYS_PER_WEEK] = {
  { "the day of work" },
  { "the day of play" },
  { "the day of rest" },
};


int curr_hour         = 0; // what hour is it in the day?
int curr_day_of_week  = 0; // what day of the week is it?
int curr_day_of_month = 0; // what day is it in the month?
int curr_month        = 0; // what month is it in the year?
int curr_year         = 0; // what year is it?



//*****************************************************************************
// Python methods
//*****************************************************************************
PyObject *PyMud_GetHour(PyObject *self) {
  return Py_BuildValue("i", get_hour());
}

PyObject *PyMud_GetTime(PyObject *self) {
  if(is_morning())
    return Py_BuildValue("s", "morning");
  else if(is_afternoon())
    return Py_BuildValue("s", "afternoon");
  else if(is_evening())
    return Py_BuildValue("s", "evening");
  else
    return Py_BuildValue("s", "night");
}

PyObject *PyMud_IsMorning(PyObject *self) {
  return Py_BuildValue("i", is_morning());
}

PyObject *PyMud_IsAfternoon(PyObject *self) {
  return Py_BuildValue("i", is_afternoon());
}

PyObject *PyMud_IsEvening(PyObject *self) {
  return Py_BuildValue("i", is_evening());
}

PyObject *PyMud_IsNight(PyObject *self) {
  return Py_BuildValue("i", is_night());
}



//*****************************************************************************
// time handling functions
//*****************************************************************************

//
// Handle the hourly update of our times
//
void handle_time_update(void *self, void *data, char *arg) {
  curr_hour++;

  if(curr_hour >= HOURS_PER_DAY) {
    curr_hour = 0;
    curr_day_of_week++;
    curr_day_of_month++;
  }

  if(curr_day_of_week >= DAYS_PER_WEEK)
    curr_day_of_week = 0;

  if(curr_day_of_month >= month_info[curr_month].num_days) {
    curr_day_of_month = 0;
    curr_month++;
  }

  if(curr_month >= NUM_MONTHS) {
    curr_month = 0;
    curr_year++;
  }


  // check to see if we've rolled over to a new time of day
  if(curr_hour == month_info[curr_month].morning_starts)
    send_outdoors("The morning sun slowly pokes its head up over the eastern horizon.\r\n");
  if(curr_hour == month_info[curr_month].afternoon_starts)
    send_outdoors("The sun hangs high overhead as afternoon approaches.\r\n");
  if(curr_hour == month_info[curr_month].evening_starts)
    send_outdoors("The sun slowly begins its descent as evening approaches.\r\n");
  if(curr_hour == month_info[curr_month].night_starts)
    send_outdoors("The sky goes dark as the sun passes over the western horizon.\r\n");

  // save our time data to file
  STORAGE_SET *set = new_storage_set();
  store_int(set, "hour",         curr_hour);
  store_int(set, "day_of_week",  curr_day_of_week);
  store_int(set, "day_of_month", curr_day_of_month);
  store_int(set, "month",        curr_month);
  store_int(set, "year",         curr_year);
  storage_write(set, TIME_FILE);
  storage_close(set);
}


void init_time() {
  STORAGE_SET *set  = storage_read(TIME_FILE);
  if(set != NULL) {
    curr_hour         = read_int(set, "hour");
    curr_day_of_week  = read_int(set, "day_of_week");
    curr_day_of_month = read_int(set, "day_of_month");
    curr_month        = read_int(set, "month");
    curr_year         = read_int(set, "year");
    storage_close(set);
  }
  else
    curr_hour = curr_day_of_week = curr_day_of_month = curr_month = curr_year = 0;

  // add our mud methods
  PyMud_addMethod("get_hour",     PyMud_GetHour,     METH_NOARGS, 
		  "get_hour()\n\n"
		  "Return the current in-game hour of day.");
  PyMud_addMethod("get_time",     PyMud_GetTime,     METH_NOARGS, 
		  "get_time()\n\n"
		  "Return time of day (morning, afternoon, evening, night).");
  PyMud_addMethod("is_morning",   PyMud_IsMorning,   METH_NOARGS, 
		  "True or False if it is morning.");
  PyMud_addMethod("is_afternoon", PyMud_IsAfternoon, METH_NOARGS, 
		  "True or False if it is afternoon.");
  PyMud_addMethod("is_evening",   PyMud_IsEvening,   METH_NOARGS, 
		  "True or False if it is evening.");
  PyMud_addMethod("is_night",     PyMud_IsNight,     METH_NOARGS, 
		  "True or False if it is night.");

  // add the time command
  add_cmd("time", NULL, cmd_time, "player", FALSE);

  // start our time updater
  start_update(NULL, TIME_UPDATE_DELAY, handle_time_update, NULL, NULL, NULL);
}


const char *get_day() {
  return day_info[curr_day_of_week].name;
}


const char *get_month() {
  return month_info[curr_month].name;
}


int get_year() {
  return curr_year;
}


int get_hour() {
  return curr_hour;
}


bool is_morning() {
  return (curr_hour >= month_info[curr_month].morning_starts &&
	  curr_hour <  month_info[curr_month].afternoon_starts);
}


bool is_afternoon() {
  return (curr_hour >= month_info[curr_month].afternoon_starts &&
	  curr_hour <  month_info[curr_month].evening_starts);
}


bool is_evening() {
  return (curr_hour >= month_info[curr_month].evening_starts &&
	  curr_hour <  month_info[curr_month].night_starts);
}


bool is_night() {
  return (curr_hour >= month_info[curr_month].night_starts ||
	  curr_hour <  month_info[curr_month].morning_starts);
}


//
// is it am or pm?
//
bool is_am() {
  return (curr_hour < HOURS_PER_DAY/2);
}


//
// what time is it, am/pm - wise?
//
int ampm_hour() {
  int ampm_time = curr_hour - (is_am() ? 0 : HOURS_PER_DAY/2);
  if(ampm_time == 0) ampm_time = HOURS_PER_DAY/2;
  return ampm_time;
}


COMMAND(cmd_time) {
  send_to_char(ch,
	       "It is %d o'clock%s during the %s of %s,\r\n"
	       "which is the %d%s day of %s, year %d.\r\n",
	       (USE_AMPM ? ampm_hour() : curr_hour+1),
	       (USE_AMPM ? (is_am() ? " a.m" : " p.m") : ""),
	       (is_morning() ? "morning" :
		(is_afternoon() ? "afternoon" :
		 (is_evening() ? "evening" : "night"))),
	       day_info[curr_day_of_week].name,
	       curr_day_of_month+1, numth(curr_day_of_month+1),
	       month_info[curr_month].name, curr_year);
}
