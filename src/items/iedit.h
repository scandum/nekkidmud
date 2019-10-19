#ifndef IEDIT_H
#define IEDIT_H
//*****************************************************************************
//
// iedit.h
//
// Although it is not core to the functioning of items, we will more likely
// than not want to be able to edit their type information in OLC. This will
// require a parser, a chooser, and a menu for editing the type information.
// Item types can add in their OLC functions via this framework. iedit uses
// the OLC2 framework. For more detailed instructions on how to set up an OLC,
// users should refer to the OLC2 source headers.
//
// NOTE: the menu, chooser, and parser should only take the item-specific data,
//       and not the WHOLE object! It has been set up this way to encourage
//       people to localize all of the data associated with one item type.
//
//*****************************************************************************

// called by items.c
void init_item_olc(void);

//
// menu, chooser, and parser are standard OLC functions of the same name that
// are typically supplied (see ../olc2/olc.h). from_proto and to_proto are
// functions that aid in the conversion between item types and python scripts
// (without actually running the script). If we want to have a non-scripting
// interface for builders, we have to be able to do this. It's pretty simple.
// For examples on how to go about doing this, see portal.c, container.c, and
// worn.c. from_proto should clean up any erroneous stuff that might happen
// to strings after an object has been parsed into an OLC-editable state.
//   void from_proto(type *item_type_data)
// to_proto should do the reverse: given type data, append it to a buffer that
// is an object prototype (python script). It should also be of the form:
//   void to_proto(type *item_type_data, BUFFER *script)
void item_add_olc(const char *type, void *menu, void *chooser, void *parser,
		  void *from_proto, void *to_proto);

//
// these functions will be needed by oedit
void iedit_menu(SOCKET_DATA *sock, OBJ_DATA *obj);
int  iedit_chooser(SOCKET_DATA *sock, OBJ_DATA *obj, const char *option);
bool iedit_parser(SOCKET_DATA *sock, OBJ_DATA *obj,int choice, const char *arg);

#endif // IEDIT_H
