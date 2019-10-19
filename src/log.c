//*****************************************************************************
//
// log.c
//
// A set of utilities for logging strings with specified keywords in them to
// files. If you would like to log someone's output, you can use the format:
//   log [name] [keywords to log]
//
//   e.g. log jim give, set, drop
//
// You can log everything by, instead of putting keywords, using "all"
// You can turn off logging for a specific character by not supplying a
// keyword list.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "character.h"
#include "storage.h"
#include "log.h"


#define LOG_LIST LOG_DIR"/logs"


// a map from filenames to the keywords we try to log
HASHTABLE *logkeys = NULL;



//
// Begin logging the appearance of specific keywords when they come up
// on a certain character's output. Supplying no keywords turns off logging
//   usage: log [person] [keywords]
//
//   examples:
//     log jim get, give, set       every time the words "get", "give", or "set"
//                                  appear in Jim's output, log the lines that
//                                  those words appeared in
//     log jim all                  log all of jim's output
//     log jim                      turn off logging for jim, if he is logged
//
COMMAND(cmd_log) {
  char name[MAX_BUFFER];
  char *keywords = NULL;

  if(!arg || !*arg)
    send_to_char(ch, "Log who?\r\n");
  else if(!(keywords = one_arg(arg, name)))
    send_to_char(ch, "What keywords did you want to log on %s?\r\n", name);
  else {
    *name = toupper(*name);
    if(*keywords)
      send_to_char(ch, "You set %s's log keywords to '%s'\r\n", name, keywords);
    else
      send_to_char(ch, "You no longer log output to %s.\r\n", name);
    log_keywords(name, keywords);
  }
}


//
// write the names of all the logfiles and their keywords to disk
//
void save_logkeys() {
  STORAGE_SET           *set = new_storage_set();
  STORAGE_SET_LIST *log_list = new_storage_list();
  HASH_ITERATOR      *hash_i = newHashIterator(logkeys);
  const char *log   = NULL;
  const char *words = NULL;

  store_list(set, "logs", log_list);
  ITERATE_HASH(log, words, hash_i) {
    STORAGE_SET *one_log = new_storage_set();
    store_string(one_log, "log",      log);
    store_string(one_log, "keywords", words);
    storage_list_put(log_list, one_log);
  }
  deleteHashIterator(hash_i);

  // write the logs to disk
  storage_write(set, LOG_LIST);
  storage_close(set);
}


void init_logs() {
  logkeys = newHashtable();

  STORAGE_SET       *set = storage_read(LOG_LIST);
  if(set != NULL) {
    STORAGE_SET_LIST *list = read_list(set, "logs");
    STORAGE_SET *one_log = NULL;
    while( (one_log = storage_list_next(list)) != NULL)
      hashPut(logkeys, strdup(read_string(one_log, "log")),
	               strdup(read_string(one_log, "keywords")));
    storage_close(set);
  }

  add_cmd("log", NULL, cmd_log, "admin", FALSE);
}


void log_keywords(const char *file, const char *keywords) {
  // remove the old keywords
  char *keys = hashRemove(logkeys, file);
  if(keys) free(keys);

  keys = strdupsafe(keywords);
  trim(keys);

  // put the new keywords in
  if(*keys)
    hashPut(logkeys, file, keys);

  // save the changes
  save_logkeys();
}


void try_log(const char *file, const char *string) {
  char *keywords = hashGet(logkeys, file);
  // didn't find anything
  if(!keywords || !*keywords)
    return;
  else {
    bool found = FALSE;

    if(is_keyword(keywords, "all", FALSE))
      found = TRUE;
    // for each key, see if it exists in the string
    else {
      LIST           *keys = parse_keywords(keywords);
      LIST_ITERATOR *key_i = newListIterator(keys);
      char            *key = NULL;

      ITERATE_LIST(key, key_i) {
	// found one
	if(strstr(string, key)) {
	  found = TRUE;
	  break;
	}
      } deleteListIterator(key_i);
      deleteListWith(keys, free);
    }

    if(found) {
      char fname[MAX_BUFFER];
      FILE *fl = NULL;
      sprintf(fname, "%s/%s", LOG_DIR, file);
      
      // couldn't open the file for appending
      if((fl = fopen(fname, "a+")) == NULL)
	return;
      
      // write the string and close the file
      fprintf(fl, "%s: %s", get_time(), string);
      fclose(fl);
    }
  }
}
