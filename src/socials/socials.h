#ifndef __SOCIALS_H
#define __SOCIALS_H
//*****************************************************************************
//
// socials.h
//
// socials are commonly used emotes (e.g. smiling, grinning, laughing). Instead
// making people have to write out an entire emote every time they would like
// to express such an emote, they can simply use one of these simple commands
// to perform a pre-made emote.
//
//*****************************************************************************

//
// This must be put at the top of mud.h so the rest of the MUD knows that
// we've got the socials module installed
// #define MODULE_SOCIALS
//


//
// prepare socials for use
//
void init_socials();


//
// cmds are the list of commands that trigger the social. More than one cmd 
// can be specified (comma-separated). Assumes cmds != NULL. All other
// arguments can be NULL.
//
// to_char_notgt is the message sent to the character if no target for
// the social is provided.
//
// to_room_notgt is the message sent to the room if no target for the
// social is provided.
//
// to_char_self is the message sent to ourself when the target provided
// was ourself. If to_char_self is not provided, the message will default
// to the same one used for to_char_notgt.
//
// to_room_self is the message sent to the room when the target provided
// was ourself. If to_room_self is not provided, the message will default
// to the same one used for to_char_notgt.
//
// to_char_tgt is the message sent to the character when a target is
// provided.
//
// to_vict_tgt is the message sent to the target when a target is provided
//
// to_room_tgt is the message sent to the room when a target is provided
//
// min_pos and max_pos are the minimum and maximum positions the socials can
// per performed from, respectively.
//
typedef struct social_data SOCIAL_DATA; 
SOCIAL_DATA *newSocial(const char *cmds,
		       const char *to_char_notgt,
		       const char *to_room_notgt,
		       const char *to_char_self,
		       const char *to_room_self,
		       const char *to_char_tgt,
		       const char *to_vict_tgt,
		       const char *to_room_tgt,
		       int min_pos, int max_pos);

void         deleteSocial(SOCIAL_DATA *data);
STORAGE_SET *socialStore (SOCIAL_DATA *social);
SOCIAL_DATA *socialRead  (STORAGE_SET *set);
SOCIAL_DATA *socialCopy  (SOCIAL_DATA *social);
void         socialCopyTo(SOCIAL_DATA *from, SOCIAL_DATA *to);


// This stuff is mainly intended for use with OLC, so socials can be displayed
const char *socialGetCmds     (SOCIAL_DATA *social);
const char *socialGetCharNotgt(SOCIAL_DATA *social);
const char *socialGetRoomNotgt(SOCIAL_DATA *social);
const char *socialGetCharSelf (SOCIAL_DATA *social);
const char *socialGetRoomSelf (SOCIAL_DATA *social);
const char *socialGetCharTgt  (SOCIAL_DATA *social);
const char *socialGetVictTgt  (SOCIAL_DATA *social);
const char *socialGetRoomTgt  (SOCIAL_DATA *social);
int         socialGetMinPos   (SOCIAL_DATA *social);
int         socialGetMaxPos   (SOCIAL_DATA *social);

//
// Again, mostly intended for use with OLC
//
// socialSetCmds is no longer in use. Try using the link_social() instead
// void         socialSetCmds     (SOCIAL_DATA *social, const char *cmds);
//
void         socialSetCharNotgt(SOCIAL_DATA *social, const char *mssg);
void         socialSetRoomNotgt(SOCIAL_DATA *social, const char *mssg);
void         socialSetCharSelf (SOCIAL_DATA *social, const char *mssg);
void         socialSetRoomSelf (SOCIAL_DATA *social, const char *mssg);
void         socialSetCharTgt  (SOCIAL_DATA *social, const char *mssg);
void         socialSetVictTgt  (SOCIAL_DATA *social, const char *mssg);
void         socialSetRoomTgt  (SOCIAL_DATA *social, const char *mssg);
void         socialSetMinPos   (SOCIAL_DATA *social, int pos);
void         socialSetMaxPos   (SOCIAL_DATA *social, int pos);


//
// save any changes that have been made to socials. OLC will need this.
//
void save_socials();


//
// Return the social assocciated with the command. If no social exists,
// return NULL
//
SOCIAL_DATA *get_social(const char *cmd);


//
// add in a new social. If any of the commands already exist as socials,
// they are unlinked first.
//
void add_social(SOCIAL_DATA *social);


//
// link a new command to a pre-existing social. Change will be saved to file
//
void link_social(const char *new_cmd, const char *old_cmd);


//
// unlink a command from it's social messages. If there are no
// more commands linked to the social, delete the social.
//
void unlink_social(const char *cmd);


#endif // __SOCIALS_H
