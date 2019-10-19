#ifndef OLC_EXTENDER_H
#define OLC_EXTENDER_H
//*****************************************************************************
//
// olc_extender.h
//
// A structure for helping in the extension of OLC functions. Allows for the
// registration of new commands, menu displays, parsers, and execution 
// functions.
//
//*****************************************************************************

typedef struct olc_extender OLC_EXTENDER;

// various extenders we might want to add to
extern OLC_EXTENDER *medit_extend;
extern OLC_EXTENDER *redit_extend;
extern OLC_EXTENDER *oedit_extend;

//
// Create a new OLC extender
OLC_EXTENDER *newExtender(void);

//
// Set the function that convert's the OLC's datatype to a borrowed Python 
// object, so Python extensions can also be registered
void extenderSetPyFunc(OLC_EXTENDER *ext, void *borrow_py);

//
// shows all of the menu options in the extender
void extenderDoMenu(SOCKET_DATA *sock, OLC_EXTENDER *ext, void *data);

//
// If the option does not exist, return MENU_CHOICE_INVALID.
// If the option leads to a submenu or a toggle, return MENU_NOCHOICE.
// Otherwise, return a unique identifier in the range of 10000 and 11000
int extenderDoOptChoice(SOCKET_DATA *sock, OLC_EXTENDER *ext, void *data, 
			char opt);

//
// Run the parse function for the extender wit hthe specified choice ID.
// Return TRUE if it is successful, or FALSE otherwise
bool extenderDoParse(SOCKET_DATA *sock, OLC_EXTENDER *ext, void *data, 
		     int choice, const char *arg);

//
// run all of our toProto functions
void extenderToProto(OLC_EXTENDER *ext, void *data, BUFFER *buf);

//
// run all of our from proto functions
void extenderFromProto(OLC_EXTENDER *ext, void *data);

//
// Set up a new OLC extension. Allows for extensions to the menu, functions
// that execute on making a menu choice, and functions that parse arguments
// for menu choices:
//
// menu: This is the function that displays the OLC menu to the socket:
//       void menu(SOCKET_DATA *sock, datatype *to_display)
//
// choice_exec: This function executes when the extension is chosen from the
//       OLC menu. Return MENU_NOCHOICE if this is a toggle, or if a new
//       OLC or input handler is pushed. Return MENU_CHOICE_OK otherwise.
//       int choice_exec(SOCKET_DATA *sock, datatype *to_edit)
//
// parse_exec: This function executes when the extension has been chosen, and
//       an argument needs to be parsed. Return TRUE if is was parsed, and
//       FALSE otherwise
//       bool parse_exec(SOCKET_DATA *sock, datatype *to_edit, const char *arg)
//
// from_proto: This function does neccessary stuff to the data itself when a
//       when an OLC object has just been created from a prototype. Mainly, this
//       is for properly formatting buffered descriptions. Not neccessary if
//       the extender is not for something that comes from a prototype
//       (i.e., mobs, objs, and rooms)
//       void from_proto(datatype *to_edit)
//
//  to_proto: This function converts the data to a Python script for generating
//       it. Not neccesary if the extender is not for something that comes from
//       a prototype.
//       void to_proto(datatype *to_convert, BUFFER *destination)
void extenderRegisterOpt(OLC_EXTENDER *ext, char opt, 
			 void *menu, void *choice, void *parse,
			 void *from_proto, void *to_proto);

//
// The same, but takes Python functions instead of C function. to_proto will
// return a string instead of concatting it to a buffer.
void extenderRegisterPyOpt(OLC_EXTENDER *ext, char opt, 
			   void *menu, void *choice, void *parse,
			   void *from_proto, void *to_proto);

#endif // OLC_EXTENDER_H
