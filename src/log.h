#ifndef __LOG_H
#define __LOG_H
//*****************************************************************************
//
// log.h
//
// A set of utilities for logging string with specified keywords in them to
// files.
//
//*****************************************************************************

#define LOG_DIR              "../lib/logs"


//
// prepare logs for use
//
void init_logs();

//
// Logs any strings with the specified keywords to the file
//
void log_keywords(const char *file, const char *keywords);

//
// Log the string if it contains any of the keywords assocciated with
// the the file
//
void try_log(const char *file, const char *string);


#endif // __LOG_H
