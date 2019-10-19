#ifndef HELP2_H
#define HELP2_H
//*****************************************************************************
//
// help.h
//
// Contained within is the new help module for NakedMud, instituted at v3.6.
// It allows for help files to exist both on disc, and to be built on the fly
// by other game modules (but not saved to disc). 
//
//*****************************************************************************

//
// This must be put at the top of mud.h so the rest of the MUD knows that
// we've got the help module installed
// #define MODULE_HELP2
//

// the structure our help info is held in
typedef struct help_data HELP_DATA;

//
// prepare helpfiles for use
void init_help();

//
// Builds the output of the help query. If no help exists on the topic, NULL
// is returned. Returned buffer must be deleted after use.
BUFFER *build_help(const char *keyword);

//
// Adds a helpfile to the game. Keywords are the list of queries that reference
// the helpfile. info is the content of the helpfile. user_groups are the list
// of of groups that can view the content of the helpfile by using the "help"
// command. If user_goups is an empty string or NULL, anyone can view the
// helpfile. related is a list of other queries that are suggested at the end
// of a help buffer after build_help is called. This can be an empty string or
// NULL if none are related. If persistent is TRUE, this helpfile will be saved
// to disc and loaded anew each time the MUD is booted up. It will be saved with
// the first keyword in the keywords list as the main keyword.
void add_help(const char *keywords, const char *info, const char *user_groups,
	      const char *related, bool persistent);

//
// removes a help file with the given keyword from our records; if it is 
// persistent, delete it from disc as well
void remove_help(const char *keyword);

//
// returns the help data associated with the keyword. 
// Also, various get functions for it
HELP_DATA           *get_help(const char *keyword, bool abbrev_ok);
const char   *helpGetKeywords(HELP_DATA *data);
const char *helpGetUserGroups(HELP_DATA *data);
const char    *helpGetRelated(HELP_DATA *data);
const char       *helpGetInfo(HELP_DATA *data);

#endif // HELP2_H
