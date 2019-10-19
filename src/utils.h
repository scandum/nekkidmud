#ifndef __UTILS_H
#define __UTILS_H
//*****************************************************************************
//
// utils.h
//
// macros and definitions of miscellaneous utilities
//
//*****************************************************************************



//*****************************************************************************
// Utilities for numbers
//*****************************************************************************
#define MAX_INT               214743647
#define PI                    3.14159265
#define MATH_E                2.71828182

#define UMIN(a, b)	      ((a) < (b) ? (a) : (b))
#ifndef MIN
#define MIN(a, b)             ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)             ((a) < (b) ? (b) : (a))
#endif

//
// return a random number pulled from a uniform distribution 
// between the two bounds
int rand_number(int min, int max);

//
// Returns a random number between 0 and 1. Numbers are evenly distributed
double rand_percent(void);

//
// Return a random number pulled from N(0, 1)
double rand_gaussian(void);

//
// Returns a sigmoid transformation of the specified number
double sigmoid(double val);

//
// return the rd, th, nd, for a number
const char *numth(int num);



//*****************************************************************************
// Utilities for characters
//*****************************************************************************
#define IS_ADMIN(ch)          (bitIsOneSet(charGetUserGroups(ch), "admin"))

#define HIMHER(ch)            (charGetSex(ch) == SEX_MALE ? "him" : \
			       (charGetSex(ch) == SEX_FEMALE ? "her" : "it"))

#define HISHER(ch)            (charGetSex(ch) == SEX_MALE ? "his" : \
			       (charGetSex(ch) == SEX_FEMALE ? "her" : "its"))

#define HESHE(ch)             (charGetSex(ch) == SEX_MALE ? "he" : \
			       (charGetSex(ch) == SEX_FEMALE ? "she" : "it"))

#define YESNO(val)            (val == FALSE ? "no" : "yes")

#define TOGGLE(val)           (val == FALSE ? TRUE : FALSE)

//
// register a check that's performed when can_see_char/obj/exit is called.
// Returns True if the target is visible, and False if it is not.
void  register_char_see   ( bool (* check)(CHAR_DATA *ch, CHAR_DATA *target));
void  register_obj_see    ( bool (* check)(CHAR_DATA *ch, OBJ_DATA *target));
void  register_exit_see   ( bool (* check)(CHAR_DATA *ch, EXIT_DATA *target));
bool  can_see_char        ( CHAR_DATA *ch, CHAR_DATA *target);
bool  can_see_obj         ( CHAR_DATA *ch, OBJ_DATA  *target);
bool  can_see_exit        ( CHAR_DATA *ch, EXIT_DATA *exit);
bool  try_enter_game      ( CHAR_DATA *ch);
int   can_see_hidden      ( CHAR_DATA *ch);
int   can_see_invis       ( CHAR_DATA *ch);

//
// returns the target's name if the ch can see the target,
// and returns SOMEONE/SOMETHING otherwise.
const char *see_char_as (CHAR_DATA *ch, CHAR_DATA *target);
const char *see_obj_as  (CHAR_DATA *ch, OBJ_DATA  *target);
const char *see_exit_as (CHAR_DATA *ch, EXIT_DATA *target);

void     show_prompt(SOCKET_DATA *socket);
const char *custom_prompt (CHAR_DATA *ch);



//*****************************************************************************
// Utilities for bit manipulations and bitvectors
//*****************************************************************************
#define BITS_PER_BITVECTOR                   32

#define IS_SET(flag,bit)       (((flag) &  (bit)) != 0)
#define SET_BIT(var,bit)        ((var) |=  (bit))
#define REMOVE_BIT(var,bit)     ((var) &= ~(bit))
#define TOGGLE_BIT(var,bit)     ((var) ^=  (bit))

bitvector_t parse_bits(const char *string);
const char *write_bits(bitvector_t bits);
void        print_bits(bitvector_t bits, const char **names, char *buf);



//*****************************************************************************
// String utilities.
//*****************************************************************************
#define CLEAR_SCREEN       "\033[H\033[J"
#define AN(string)         (strchr("AEIOU", toupper(*string)) ? "an" : "a")
#define strdupsafe(string) strdup(string ? string : "")

LIST *parse_strings       (const char *string, char delimeter);
LIST *parse_keywords      (const char *keywords);
bool dup_keywords_exist   (const char *keywords);
bool is_keyword           (const char *keywords, const char *word, 
			   bool abbrev_ok);
int  find_keyword         (const char *keywords, const char *string);
void add_keyword          (char **keywords_ptr, const char *word);
void remove_keyword       (char *keywords, const char *word);
void trim                 (char *string);
void strip_word           (char *string, const char *word);
void buf_tag_keywords     (BUFFER *buf, const char *keywords,
			   const char *start_tag, const char *end_tag);
int  count_letters        (const char *string, const char ch, const int strlen);
int  count_occurences     (const char *string, const char *word);
char *line_start          (char *string, int line);
int  fgetline             (FILE *file, char *p, int maxlen);
void center_string        (char *buf, const char *string, int linelen, 
			   int buflen, bool border);
int next_space_in         (const char *string);
int next_letter_in        (const char *string, char marker);
int is_paragraph_marker   (const char *string, int index);

//
// four unique hash functions for 8-bit hashing. They can be concatinated
// to do higher-order hashing as would be typically desired
unsigned long    pearson_hash8(const char *string, int *table);
unsigned long  pearson_hash8_1(const char *string);
unsigned long  pearson_hash8_2(const char *string);
unsigned long  pearson_hash8_3(const char *string);
unsigned long  pearson_hash8_4(const char *string);
unsigned long pearson_hash16_1(const char *string);
unsigned long pearson_hash16_2(const char *string);
unsigned long   pearson_hash32(const char *string);

//
// aliases pearson_hash32
unsigned long string_hash(const char *key);

bool endswith             (const char *string, const char *end);
bool startswith           (const char *string, const char *start);
const char *strcpyto      (char *to, const char *from, char end);



//*****************************************************************************
// utilities for game functioning
//*****************************************************************************

// remove the thing from the game, and delete it
void         extract_mobile(CHAR_DATA *ch);
void            extract_obj(OBJ_DATA  *obj);
void           extract_room(ROOM_DATA *room);

// don't call these. These are for use by gameloop.c only. Use the non-final
// versions, please!!
void   extract_mobile_final(CHAR_DATA *ch);
void      extract_obj_final(OBJ_DATA  *obj);
void     extract_room_final(ROOM_DATA *room);

char *get_time             ( void );
void  communicate          ( CHAR_DATA *dMob, char *txt, int range );
void  load_muddata         ( void );
CHAR_DATA  *check_reconnect( const char *player );

// transfers things from one room to another
void do_mass_transfer(ROOM_DATA *from, ROOM_DATA *to, bool chars, bool mobs, 
		      bool objs);

// handles relative exits. If an exit destination does not have a locale, 
// append the one for its room.
const char *exitGetToFull(EXIT_DATA *exit);

// find the root room of an object; traverse its containers and carriers
ROOM_DATA *objGetRootRoom(OBJ_DATA *obj);



//*****************************************************************************
// misc utils
//*****************************************************************************

//
// a function that breaks a type key into its name and locale (zone). Each key 
// must contain its name and zone, separated by an @. For instance:
//   sword@bobs_zone
//
// returns true if the name and locale were successfully found. It is assumed
// there will be enough memory in the name and locale buffers to copy data over
bool parse_worldkey(const char *key, char *name, char *locale);

//
// same as parse_worldkey, but if no locale is supplied, use the zone that the
// character is currently in
bool parse_worldkey_relative(CHAR_DATA *ch, const char *key, char *name, 
			     char *locale);

//
// returns the locale of a key, and \0 if none exists
const char *get_key_locale(const char *key);

//
// returns the name of a key, and \0 if none exists
const char *get_key_name(const char *key);

//
// returns the full key of something. A full key is a name and a locale (zone
// key) separated by an @ sign. Assumes name and locale are both valid (i.e.
// neither have an @ sign in them).
const char *get_fullkey(const char *name, const char *locale);

//
// returns the full key of something (name + locale), relative to the supplied
// locale. If the key doesn't already have a locale, this function will build
// the one supplied into the returned value.
const char *get_fullkey_relative(const char *key, const char *locale);

//
// Returns the shortest version of key, relative to the other key, 'to'.
// If they have separate locales, return the fullkey of key. If they have the
// same locale, just return the first portion of the key
const char *get_shortkey(const char *key, const char *to);

//
// return whether the key is malformed in some way. Keys can only contain
// letters, numbers, underscores, and a single @ sign
bool key_malformed(const char *key);

//
// returns whether a key locale is malformed. This is true if it contains any
// non-alphanumeric characters
bool locale_malformed(const char *locale);

//
// Returns whether or not the command matches the pattern. Patterns are just
// like commands, except they can be terminated with *'s to signify that
// "anything can follow at this point". Example matches might include:
//
//  PATTERN           MATCHES             NON-MATCHES
//  go*               goto, gossip, go    g
//  go                go                  g, goss, gossip
bool cmd_matches(const char *pattern, const char *cmd);

//
// returns TRUE if ch1 belongs to more user groups than ch2. By "more", we mean
// that ch2's user groups must be a subset of ch1's
bool charHasMoreUserGroups(CHAR_DATA *ch1, CHAR_DATA *ch2);

//
// returns TRUE if the character can edit the given zone
bool canEditZone(ZONE_DATA *zone, CHAR_DATA *ch);

//
// a function that returns the argument passed into it
void *identity_func(void *data);

//
// returns a copy of the list with everything in reverse order
LIST *reverse_list(LIST *list);

//
// checks to see if a file or directory exists
bool file_exists(const char *fname);
bool dir_exists (const char *dname);


//
// count how many objects are in the list, that meet our critereon.
// If name is not NULL, we search by name. Otherwise, we search by prototype.
// must_see TRUE, a looker must be supplied. Same thing goes for count_letters
int   count_objs   (CHAR_DATA *looker, LIST *list, const char *name,
		    const char *prototype, bool must_see);
int   count_chars  (CHAR_DATA *looker, LIST *list, const char *name,
		    const char *prototype, bool must_see);
CHAR_DATA *find_char(CHAR_DATA *looker, LIST *list, int num, const char *name,
		     const char *prototype, bool must_see);
OBJ_DATA *find_obj (CHAR_DATA *looker, LIST *list, int num, const char *name,
		    const char *prototype, bool must_see);
LIST *find_all_chars(CHAR_DATA *looker, LIST *list, const char *name,
		     const char *prototype, bool must_see);
LIST *find_all_objs(CHAR_DATA *looker, LIST *list, const char *name,
		    const char *prototype, bool must_see);

// in the various find() routines, it may sometimes arise that someone
// wants to find multiple things (e.g. all.cookies, all.women). This is
// the numeric marker to represent all.
#define COUNT_ALL       (-1)

//
// split a string into it's target and count number.
//   e.g. all.cookie splits into cookie and all
//        2.woman splits into woman and 2
void get_count(const char *buf, char *target, int *count);

//
// reverse of get_count
void print_count(char *buf, const char *target, int count);


//
// return a comma-separated list of the list elements, as represented
// by the descriptor function. If there is more than one element in the list,
// the last element is separated by an "and" as well. If there are just two
// elements, then the last comma isn't used. If one element has the same
// descriptor as another element, they are printed as a group - either by
// the description given from multi_descriptor, or with a number before the
// regular descriptor representing how many of the things are present.
//
//  e.g. a man, a woman, a dog, and a child.
//       a man and a woman
//       (5) a man
//       a group of 5 men
char *print_list(LIST *list, void *descriptor, void *multi_descriptor);


//
// descriptor is a pointer to the function that gets a copy of the
// thing's description.
//
// multi-descriptor is a pointer to the function that gets a copy of the
// thing's description if there is more than 1 copy of it in the list.
void show_list(CHAR_DATA *ch, LIST *list, void *descriptor, 
	       void *multi_descriptor);


//
// Return a list of all items in the current list that do not have
// people sitting on or at them. The returned list must be deleted
// after use.
LIST *get_unused_items(CHAR_DATA *ch, LIST *list, bool invis_ok);


//
// The opposite of get_unused_items
LIST *get_used_items(CHAR_DATA *ch, LIST *list, bool invis_ok);


//
// returns true if the character has an object that is an instance of the
// specified prototype in his inventory. Returns false otherwise.
bool has_obj(CHAR_DATA *ch, const char *prototype);


//
// Delete an object, room, mobile, etc... from the game. First remove it from
// the gameworld database, and then delete it and its contents. The onus is on
// the builder to make sure deleting a prototype won't screw anything up (e.g.
// people standing about in a room when it's deleted).
bool do_delete(CHAR_DATA *ch, const char *type, void *deleter, const char *arg);


//
// generic xxxlist for listing all the entries of one type for a zone (e.g.
// mprotos, oprotos, scripts...)
void do_list(CHAR_DATA *ch, const char *locale, const char *type, 
	     const char *header, void *informer);


//
// moves something of the given type with one name to the other name
bool do_rename(CHAR_DATA *ch,const char *type,const char *from,const char *to);

#endif // __UTILS_H
