//*****************************************************************************
//
// help.c
//
// Contained within is the new help module for NakedMud, instituted at v3.6.
// It allows for help files to exist both on disc, and to be built on the fly
// by other game modules (but not saved to disc). 
//
//*****************************************************************************

#include <dirent.h>

#include "../mud.h"
#include "../utils.h"
#include "../storage.h"
#include "../character.h"
#include "help.h"
#include "hedit.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pymudsys.h"



//*****************************************************************************
// local variables, data structures, and functions
//*****************************************************************************

// the folder where we save persistent helpfiles to disc
#define HELP_DIR   "../lib/help"

// the old file we used to store all help files in
#define OLD_HELP_FILE "../lib/misc/help"

// where we store all of our helpfiles
NEAR_MAP *help_table;

struct help_data {
  char *keywords;    // words that bring up this helpfile
  char *info;        // the information in the helpfile
  char *user_groups; // the user group the helpfile belongs to, if any
  char *related;     // a list of other helpfiles that are related
};

HELP_DATA *newHelp(const char *keywords, const char *info, 
		   const char *user_groups, const char *related) {
  HELP_DATA *data   = malloc(sizeof(HELP_DATA));
  data->keywords    = strdupsafe(keywords);
  data->info        = strdupsafe(info);
  data->user_groups = strdupsafe(user_groups);
  data->related     = strdupsafe(related);
  return data;
}

void deleteHelp(HELP_DATA *data) {
  if(data->keywords)    free(data->keywords);
  if(data->info)        free(data->info);
  if(data->user_groups) free(data->user_groups);
  if(data->related)     free(data->related);
  free(data);
}

HELP_DATA *helpRead(STORAGE_SET *set) {
  return newHelp(read_string(set, "keywords"),
		 read_string(set, "info"),
		 read_string(set, "user_group"),
		 read_string(set, "related"));
}

STORAGE_SET *helpStore(HELP_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "keywords",   data->keywords);
  store_string(set, "info",       data->info);
  store_string(set, "user_group", data->user_groups);
  store_string(set, "related",    data->related);
  return set;
}


//*****************************************************************************
// local functions
//*****************************************************************************

//
// returns a list of all the help files that match the keyword in our help
// table, in the event that what we are trying to look up is ambiguous.
LIST *get_help_matches(const char *keyword) {
  return nearMapGetAllMatches(help_table, keyword);
}

//
// returns a list of all help keywords
LIST *get_all_help(void) {
  LIST             *all = newList();
  NEAR_ITERATOR *help_i = newNearIterator(help_table);
  const char      *name = NULL;
  HELP_DATA       *data = NULL;
  ITERATE_NEARMAP(name, data, help_i) {
    listQueue(all, strdup(name));
  } deleteNearIterator(help_i);
  return all;
}

//
// returns where the help file would be stored on disc if it exists
const char *get_help_file(const char *keyword) {
  static BUFFER *buf = NULL;
  char        subdir = 'Z';   // Z is the default subdir for non-alpha kwds
  if(buf == NULL)
    buf = newBuffer(1);
  bufferClear(buf);
  if(isalpha(*keyword))
    subdir = toupper(*keyword);
  bprintf(buf, "%s/%c/%s", HELP_DIR, subdir, keyword);
  return bufferString(buf);
}



//*****************************************************************************
// in game commands
//*****************************************************************************
COMMAND(cmd_help) {
  char       *kwd = NULL;
  HELP_DATA *data = NULL;
  if(*arg == '\'' || *arg == '"')
    one_arg(arg, arg);

  if(!parse_args(ch, TRUE, cmd, arg, "| string", &kwd))
    return;

  // did we supply an argument, or do we want to see a list of all help topics?
  if(kwd == NULL) {
    BUFFER           *buf = newBuffer(1);
    NEAR_ITERATOR *near_i = newNearIterator(help_table);
    const char       *kwd = NULL;
    HELP_DATA       *data = NULL;
    LIST        *viewable = newList();
    LIST_ITERATOR *view_i = NULL;
    int             count = 0;
    
    ITERATE_NEARMAP(kwd, data, near_i) {
      // if we can view the help file, list it
      if(!*data->user_groups || bitIsOneSet(charGetUserGroups(ch), 
					    data->user_groups))
	// add it to our list of helps we can view
	listPut(viewable, strdupsafe(kwd));
    } deleteNearIterator(near_i);

    // sort our list of helps
    listSortWith(viewable, strcasecmp);

    // build up our buffer of help topics
    view_i = newListIterator(viewable);
    ITERATE_LIST(kwd, view_i) {
      bprintf(buf, "%-20s", kwd);
      count++;
      // only 4 entries per row
      if((count % 4) == 0) 
	bufferCat(buf, "\r\n");
    } deleteListIterator(view_i);
    bufferCat(buf, "\r\n");

    // show our list
    if(charGetSocket(ch))
      page_string(charGetSocket(ch), bufferString(buf));

    // garbage collection
    deleteBuffer(buf);
    deleteListWith(viewable, free);
  }

  // do we have a match?
  else if( (data = get_help(kwd, TRUE)) == NULL)
    send_to_char(ch, "No help exists on that topic.\r\n");
  else if(*data->user_groups && !bitIsOneSet(charGetUserGroups(ch), 
					     data->user_groups))
    send_to_char(ch, "You may not view that help file.\r\n");
  else {
    BUFFER *buf = build_help(kwd);
    if(charGetSocket(ch))
      page_string(charGetSocket(ch), bufferString(buf));
    deleteBuffer(buf);
  }
}



//*****************************************************************************
// implementation of help.h
//*****************************************************************************
BUFFER *build_help(const char *keyword) {
  HELP_DATA *data = get_help(keyword, TRUE);

  // no match
  if(data == NULL)
    return NULL;
  // build the info for our match
  else {
    BUFFER     *buf = newBuffer(1);
    char      header[128]; // +2 for \r\n, +1 for \0
    center_string(header, data->keywords, 79, 128, TRUE);

    // build the header and the info
    bprintf(buf, "%s{n%s", header, data->info);

    // do we have a reference list?
    if(*data->related)
      bprintf(buf, "\r\nsee also: %s\r\n", data->related);
    return buf;
  }
}

void add_help(const char *keywords, const char *info, const char *user_groups,
	      const char *related, bool persistent) {
  // build our help data
  HELP_DATA *data = newHelp(keywords, info, user_groups, related);
  
  // add it to the help table for all of our keywords
  LIST           *kwds = parse_keywords(keywords);
  LIST_ITERATOR *kwd_i = newListIterator(kwds);
  char            *kwd = NULL;
  ITERATE_LIST(kwd, kwd_i) {
    // remove any old copy we might of had
    // HELP_DATA *old = nearMapRemove(help_table, kwd);
    // if(old != NULL)
    //   deleteHelp(old);
    nearMapRemove(help_table, kwd);
    
    // put our new entry
    nearMapPut(help_table, kwd, NULL, data);
  } deleteListIterator(kwd_i);

  // is this meant to be persistent?
  if(persistent && listSize(kwds) > 0) {
    STORAGE_SET *set = helpStore(data);
    char    *primary = listGet(kwds, 0); 
    storage_write(set, get_help_file(primary));
    storage_close(set);
  }

  // garbage collection
  deleteListWith(kwds, free);
}

void remove_help(const char *keyword) {
  HELP_DATA *help = get_help(keyword, FALSE);

  // does the helpfile exist?
  if(help != NULL) {
    // build up a list of all the keywords that reference this helpfile. Also,
    // figure out what the main topic is so we can delete its from disc if it
    // is persistent
    LIST           *kwds = parse_keywords(help->keywords);
    LIST_ITERATOR *kwd_i = newListIterator(kwds);
    char            *kwd = NULL;
    char        *primary = listGet(kwds, 0);

    // remove all of our data from the help table
    ITERATE_LIST(kwd, kwd_i) {
      nearMapRemove(help_table, kwd);
    } deleteListIterator(kwd_i);

    // delete our primary file
    if(file_exists(get_help_file(primary)))
      unlink(get_help_file(primary));

    // garbage collection
    deleteListWith(kwds, free);
    deleteHelp(help);
  }
}

//
// returns the help file in our help table corresponding to the keyword
HELP_DATA *get_help(const char *keyword, bool abbrev_ok) {
  return nearMapGet(help_table, keyword, abbrev_ok);
}

const char *helpGetKeywords(HELP_DATA *data) {
  return data->keywords;
}

const char *helpGetUserGroups(HELP_DATA *data) {
  return data->user_groups;
}

const char *helpGetRelated(HELP_DATA *data) {
  return data->related;
}

const char *helpGetInfo(HELP_DATA *data) {
  return data->info;
}



//*****************************************************************************
// python extensions
//*****************************************************************************
PyObject *PyMudSys_add_help(PyObject *self, PyObject *args) {
  char *keywords    = NULL;
  char *info        = NULL;
  char *user_groups = NULL;
  char *related     = NULL;

  if(!PyArg_ParseTuple(args,"ss|ss", &keywords, &info, &user_groups, &related)){
    PyErr_Format(PyExc_TypeError, "Invalid arguments supplied to add_help");
    return NULL;
  }

  add_help(keywords, info, user_groups, related, FALSE);
  return Py_BuildValue("i", 1);
}

PyObject *PyMudSys_get_help(PyObject *self, PyObject *args) {
  char     *name = NULL;
  bool abbrev_ok = FALSE;

  if(!PyArg_ParseTuple(args, "s|b", &name, &abbrev_ok)) {
    PyErr_Format(PyExc_TypeError, "Invalid arguments supplied to get_help");
    return NULL;
  }

  // find the helpfile we're looking for
  HELP_DATA *help = get_help(name, abbrev_ok);
  
  // did we find it?
  if(help == NULL)
    return Py_BuildValue("OOOO", Py_None, Py_None, Py_None, Py_None);
  else
    return Py_BuildValue("ssss",
			 helpGetKeywords(help),
			 helpGetInfo(help),
			 helpGetUserGroups(help),
			 helpGetRelated(help));
}

PyObject *PyMudSys_list_help(PyObject *self, PyObject *args) {
  char  *keyword = NULL;

  if(!PyArg_ParseTuple(args, "|s", &keyword)) {
    PyErr_Format(PyExc_TypeError, "Invalid arguments supplied to get_help");
    return NULL;
  }

  // find our matches; all topics if there is no keyword
  LIST *matches = (keyword ? get_help_matches(keyword) : get_all_help());

  // get_help_matches likes to return NULL if it doesn't find anything.
  // kind of annoying.
  if(matches == NULL) 
    matches = newList();

  // convert the list of string to Python strings
  PyObject *pymatches = PyList_fromList(matches, PyString_FromString);

  // garbage collection
  deleteListWith(matches, free);
  return Py_BuildValue("O", pymatches);
}



//*****************************************************************************
// initialization
//*****************************************************************************

//
// reads in all of our helpfiles from disc
void read_help() {
  // directory info for reading in helpfiles
  char fname[SMALL_BUFFER];
  struct dirent *entry = NULL;
  DIR             *dir = NULL;
  char          subdir = '\0';
  
  // read in all of our help files from disc
  for(subdir = 'A'; subdir <= 'Z'; subdir++) {
    // build up our directory name
    sprintf(fname, "%s/%c", HELP_DIR, subdir);

    // open the directory
    dir = opendir(fname);

    // read in each file
    if(dir != NULL) {
      for(entry = readdir(dir); entry != NULL; entry = readdir(dir)) {
	if(*entry->d_name == '.')
	  continue;
	sprintf(fname, "%s/%c/%s", HELP_DIR, subdir, entry->d_name);
	if(file_exists(fname)) {
	  STORAGE_SET *set = storage_read(fname);
	  add_help(read_string(set, "keywords"),
		   read_string(set, "info"),
		   read_string(set, "user_group"),
		   read_string(set, "related"),
		   FALSE);
	  storage_close(set);
	}
      }
      closedir(dir);
    }
  }
}

//
// read all of our helpfiles that were created with the old help system. The
// master file is deleted after they are read in, since they are saved to the
// new format.
void read_old_help() {
  // if we still have the old help file, read in all of its contents and save
  // all of the entries in the new format. Then, delete the old help file
  if(file_exists(OLD_HELP_FILE)) {
    STORAGE_SET       *set = storage_read(OLD_HELP_FILE);
    STORAGE_SET_LIST *list = read_list(set, "helpfiles");
    STORAGE_SET     *entry = NULL;

    // parse all of the helpfiles
    while( (entry = storage_list_next(list)) != NULL)
      add_help(read_string(entry, "keywords"),
	       read_string(entry, "info"),
	       read_string(entry, "user_group"),
	       read_string(entry, "related"),
	       TRUE);

    // close the set
    storage_close(set);

    // delete the old file; we don't need it any more
    unlink(OLD_HELP_FILE);
  }
}

void init_help() {
  // create our help table
  help_table = newNearMap();

  // read in all of our helpfiles. Old helpfiles are read, merged into the
  // new system, and then deleted from disc as part of the old system
  read_help();
  read_old_help();

  // initialize our editing system
  init_hedit();

  // add our commands
  add_cmd("help", NULL, cmd_help, "player", FALSE);

  // add all of our Python hooks
  PyMudSys_addMethod("add_help", PyMudSys_add_help, METH_VARARGS, 
    "add_help(keywords, info, user_groups='', related='')\n\n"
    "Add a new, non-persistent helpfile to the mud's help database.");
  PyMudSys_addMethod("get_help", PyMudSys_get_help, METH_VARARGS,
    "get_help(keyword)\n\n"
    "Returns a tuple of a helpfile's keywords, info, user_groups, and related\n"
    "or None if the helpfile does not exist.");
  PyMudSys_addMethod("list_help", PyMudSys_list_help, METH_VARARGS,
    "list_help(keyword='')\n\n"
    "Returns a list of helpfiles that match the specified keyword. If no\n"
    "keywordi s supplied, return all helpfiles.");
}
