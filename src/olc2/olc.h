#ifndef OLC2_H
#define OLC2_H
//*****************************************************************************
//
// olc.h
//
// My first go at OLC was during the pre-module time, and it really wasn't all
// that well put together in the first place. This is the second go at it...
// this time, OLC is more like some general functions that make it easy to do
// online editing, rather than a full system. It will be amenable to adding
// new extentions to the general OLC framework from other modules, and it will
// essentially just be a bunch spiffier.
//
//*****************************************************************************

//
// prepare the OLC framework for use
void init_olc2();

//
// Set up a new OLC. OLCs can be stacked on top of eachother (e.g. room edit
// could call extra description edit. After the extra description edit is
// exited, room edit will continue). If this is the only OLC in use, OLC mode
// will automatically be entered. Below are descriptions of the paramaters the
// following function takes, and the form they should be:
//
// menu: This is a function that displays the current OLC menu to the socket.
//       void menu(SOCKET_DATA *sock, datatype *to_display)
//
// chooser: This is a function that takes in the menu option that the socket 
//       wishes to select. The values q and Q are reserved by the OLC handler
//       for quitting. The chooser is responsible for prompting the socket for 
//       appropriate arguments after a menu option has been selected. There are
//       three types of values that can be returned:
//          -1     invalid option
//           0     Valid choice, but we do not need to enter the parser. This
//                 would be returned, for instance, if a new do_olc was called,
//                 or if a toggle on/off menu option was selected.
//         >= 1    a numeric value representing the field we are editing. This
//                 will be passed to the modifier() function to perform changes
//                 after an argument is passed in.
//       the chooser function is of the form:
//       int chooser(SOCKET_DATA *sock, datatype *to_edit, const char *option)
//
// parser: This function is used to parse an argument, given a choice from the
//       menu. The choice is provided by the chooser() function, and an argument
//       is supplied afterwards. TRUE should be returned if the argument was
//       valid and the command was a success, and FALSE if it was not. 
//       The parser takes the form:
//       bool parser(SOCKET_DATA *sock, datatype *to_edit, int choice, 
//                   const char *arg)
//
// copier: This is a function that returns a deep copy of the data we are
//       editing. The data is not worked on, directly. Rather, a copy of it
//       is made and edited. If the changes are accepted at the end of OLC, they
//       are copied back over to the thing we were editing. This parameter can
//       be NULL if the data is to be worked on directly. The function is of
//       the form: void copier(datatype *thing_to_copy)
//
// copyto: This copies the data of one thing over to another datastructure of
//       the same type. This parameter can be NULL if the data is to be worked
//       on directly. This function takes the form:
//       void copyto(datatype *thing_to_copy_from, datatype *thing_to_copy_to)
//
// saver: After OLC is exited, we'll typically need to save the changes to disk.
//       this is the function that should be called to save all of the changes
//       that have been made. Saver can be null if changes are not to be saved
//       to disk after they have been made. This function takes the form:
//       void saver(datatype *working_copy)
//
// deleter: this is a function that will delete the working copy of our data.
//       This parameter can be NULL if the data is to be worked on directly.
//       If it is not NULL, it will be called at the completion of OLC, even if
//       the data is being worked on directly. This is simply to allow for
//       wrapper structures (e.g., for Python). This function takes the form:
//       void delete(datatype *working_copy)
#define MENU_CHOICE_CONFIRM_SAVE   (-3) // this define for internal use only!
#define MENU_CHOICE_OK             (-2) // used for olc extenders
#define MENU_CHOICE_INVALID        (-1)
#define MENU_NOCHOICE               (0)

void do_olc(SOCKET_DATA *sock,
	    void *menu,
	    void *chooser,
	    void *parser,
	    void *copier,
	    void *copyto,
	    void *deleter,
	    void *saver,
	    void *data);



#define ECODE_BEGIN "### begin extra code"
#define ECODE_END   "### end extra code"

//
// Pre v3.6, we'd manually parse out OLC-editable structures from Python code.
// Now, we just run the relevant code to generate something editable. This is
// the function that does all the work of parsing out the 'extra' code and not
// running it, but running everything else.
void olc_from_proto(PROTO_DATA *proto,BUFFER *extra,void *me,void *aspy);



//*****************************************************************************
// To keep a general "look and feel" to OLCs, a couple functions have been
// provided to display some features of an OLC in a standard manner.
//*****************************************************************************

//
// Display a bunch of options in a tabular form. The table will have num_vals
// elements, and num_cols of those elements will be displayed per row.
void olc_display_table(SOCKET_DATA *sock, const char *getName(int val),
		       int num_vals, int num_cols);

//
// Same deal as olc_display_table, except that it takes in a list of strings
void olc_display_list(SOCKET_DATA *sock, LIST *list, int num_cols);

#endif // OLC2_H
