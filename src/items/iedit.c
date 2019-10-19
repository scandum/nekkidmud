//*****************************************************************************
//
// iedit.c
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

#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "items.h"
#include "iedit.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../olc2/olc.h"
#include "../olc2/olc_extender.h"



//*****************************************************************************
// local functions, variables, and datastructures
//*****************************************************************************

// used to store the different sub-olc functions for different item types
HASHTABLE *item_olc_table = NULL;

typedef struct item_olc_data {
  void       (* menu)(SOCKET_DATA *sock, void *data);
  int     (* chooser)(SOCKET_DATA *sock, void *data, const char *option);
  bool     (* parser)(SOCKET_DATA *sock, void *data,int choice,const char *arg);
  void (* from_proto)(void *data);
  void   (* to_proto)(void *data, BUFFER *buf);
} ITEM_OLC_DATA;

ITEM_OLC_DATA *newItemOLC(void *menu, void *chooser, void *parser, 
			  void *from_proto, void *to_proto) {
  ITEM_OLC_DATA *data = calloc(1, sizeof(ITEM_OLC_DATA));
  data->menu       = menu;
  data->chooser    = chooser;
  data->parser     = parser;
  data->from_proto = from_proto;
  data->to_proto   = to_proto;
  return data;
}

void deleteItemOLC(ITEM_OLC_DATA *data) {
  free(data);
}



//*****************************************************************************
// implementation of iedit.h
//*****************************************************************************
#define IEDIT_EDIT        1
#define IEDIT_DELETE      2


//
// list off all the item types available for editing. Print the type in green
// if the item currently is not of that type. Print the type in yellow if the
// item is of that type.
void iedit_show_types(SOCKET_DATA *sock, OBJ_DATA *obj) {
  LIST *list = itemTypeList();
  LIST_ITERATOR *list_i = newListIterator(list);
  char *type = NULL;
  int col = 0;

  text_to_buffer(sock, "{wEditable item types:{g\r\n");
  ITERATE_LIST(type, list_i) {
    col++;
    send_to_socket(sock, "  %s%-14s%s", (objIsType(obj, type) ? "{y" : "{g"),
		   type, ((col != 0 && col % 4 == 0) ? "\r\n": "   "));
  }
  deleteListIterator(list_i);
  deleteListWith(list, free);

  if(col % 4 != 0) text_to_buffer(sock, "\r\n");
}

void item_add_olc(const char *type, void *menu, void *chooser, void *parser,
		  void *from_proto, void *to_proto) {
  ITEM_OLC_DATA *data = NULL;
  // check to see if we already have functions for this item type installed
  if( (data = hashRemove(item_olc_table, type)) != NULL)
    deleteItemOLC(data);
  data = newItemOLC(menu, chooser, parser, from_proto, to_proto);
  hashPut(item_olc_table, type, data);
}

void iedit_menu(SOCKET_DATA *sock, OBJ_DATA *obj) {
  iedit_show_types(sock, obj);
  text_to_buffer(sock, 
		 "\r\n"
		 "    {gE) edit type\r\n"
		 "    D) delete type\r\n");
}

int  iedit_chooser(SOCKET_DATA *sock, OBJ_DATA *obj, const char *option) {
  switch(toupper(*option)) {
  case 'E':
    text_to_buffer(sock, "Which item type would you like to edit: ");
    return IEDIT_EDIT;
  case 'D':
    text_to_buffer(sock, "Which item type would you like to delete: ");
    return IEDIT_DELETE;
  default: return MENU_CHOICE_INVALID;
  }
}

bool iedit_parser(SOCKET_DATA *sock, OBJ_DATA *obj,int choice, const char *arg){
  switch(choice) {
  case IEDIT_EDIT: {
    ITEM_OLC_DATA *funcs = NULL;
    if(!objIsType(obj, arg))
      objSetType(obj, arg);
    // pull up the OLC functions and see if we can edit the type
    if((funcs = hashGet(item_olc_table, arg)) != NULL) {
      void *item_data = objGetTypeData(obj, arg);
      do_olc(sock, funcs->menu, funcs->chooser, funcs->parser,
	     NULL, NULL, NULL, NULL, item_data);
    }
    return TRUE;
  }
  case IEDIT_DELETE:
    objDeleteType(obj, arg);
    return TRUE;
  default: return FALSE;
  }
}

void item_from_proto(const char *type, void *data) {
  ITEM_OLC_DATA *funcs = hashGet(item_olc_table, type);
  if(funcs != NULL && funcs->from_proto != NULL)
    funcs->from_proto(data);
}

void item_to_proto(const char *type, void *data, BUFFER *buf) {
  ITEM_OLC_DATA *funcs = hashGet(item_olc_table, type);
  if(funcs != NULL && funcs->to_proto != NULL)
    return funcs->to_proto(data, buf);
}



//*****************************************************************************
// functions for extending oedit
//*****************************************************************************
void oedit_item_menu(SOCKET_DATA *sock, OBJ_DATA *obj) {
  send_to_socket(sock, "Edit item types: {c%s\r\n", objGetTypes(obj));
}

int oedit_item_choose(SOCKET_DATA *sock, OBJ_DATA *obj) {
  do_olc(sock, iedit_menu, iedit_chooser, iedit_parser, NULL, NULL, NULL,
	 NULL, obj);
  return MENU_NOCHOICE;
}

void oedit_item_to_proto(OBJ_DATA *obj, BUFFER *buf) {
  // item types
  LIST      *item_types = itemTypeList();
  LIST_ITERATOR *type_i = newListIterator(item_types);
  char            *type = NULL;

  ITERATE_LIST(type, type_i) {
    if(objIsType(obj, type)) {
      void *data = objGetTypeData(obj, type);
      bprintf(buf, "\n### item type: %s\n", type);
      bprintf(buf, "me.settype(\"%s\")\n", type);
      item_to_proto(type, data, buf);
      //item_to_proto_func(type)(data, buf);      
      bprintf(buf, "### end type\n");
    }
  } deleteListIterator(type_i);
  deleteListWith(item_types, free);
}

void oedit_item_from_proto(OBJ_DATA *obj) {
  // item types
  LIST      *item_types = itemTypeList();
  LIST_ITERATOR *type_i = newListIterator(item_types);
  char            *type = NULL;

  ITERATE_LIST(type, type_i) {
    if(objIsType(obj, type))
      item_from_proto(type, obj);
  } deleteListIterator(type_i);
  deleteListWith(item_types, free);
}



//*****************************************************************************
// initialization of item olc
//*****************************************************************************
void init_item_olc() {
  item_olc_table = newHashtable();

  // add our OLC extension
  extenderRegisterOpt(oedit_extend, 'I', 
		      oedit_item_menu, oedit_item_choose, NULL, 
		      oedit_item_from_proto, oedit_item_to_proto);
}
