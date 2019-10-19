#ifndef OLC_SUBMENUS_H
#define OLC_SUBMENUS_H
//*****************************************************************************
//
// olc_submenus.h
//
// There's some OLC editors that will only ever be called from within another
// olc editor (e.g. the extra description editor inside the room editor). They
// may need to be called by multiple editors (e.g. extra descs in rooms, and
// for objects), so we have to give them access to all of the olc-related
// functions. That's what this header provides.
//
//*****************************************************************************

// extra description sets
void edesc_set_menu   (SOCKET_DATA *sock, EDESC_SET *set);
bool edesc_set_parser (SOCKET_DATA *sock, EDESC_SET *set, int choice,
		       const char *arg);
int  edesc_set_chooser(SOCKET_DATA *sock, EDESC_SET *set, const char *option);

// for bitvectors
void bedit_menu   (SOCKET_DATA *sock, BITVECTOR *v);
bool bedit_parser (SOCKET_DATA *sock, BITVECTOR *v, int choice,const char *arg);
int  bedit_chooser(SOCKET_DATA *sock, BITVECTOR *v, const char *option);

#endif // OLC_SUBMENUS_H
