#ifndef MUD_H
#define MUD_H
//*****************************************************************************
//
// mud.h
//
// this is the main header file for the MUD. All MUD-related source files should
// include this header.
//
//*****************************************************************************

#include "wrapsock.h"



//*****************************************************************************
// if you've installed a new module, you  need to put a define in here to let
// the rest of the MUD know that you've installed the module.
//*****************************************************************************

// mandatory modules. These are modules that the NakedMud core REQUIRES to run.
// They have simply been made modules for organizational ease.
#define MODULE_ITEMS
#define MODULE_OLC2
#define MODULE_SCRIPTS
#define MODULE_DYN_VARS
#define MODULE_SET_VAL
#define MODULE_EDITOR

// here is where your optional modules will go
#define MODULE_ALIAS
#define MODULE_SOCIALS
#define MODULE_HELP2
#define MODULE_TIME
#define MODULE_COLOR // MTH 1.5 color module
#define MODULE_MTH   // MTH 1.5 telnet module

//*****************************************************************************
// To avoid having to write some bulky structure names, we've typedef'd a
// bunch of shortforms for commonly used datatypes. If you make a new datatype
// that is used lots, put a typedef for it in here.
//*****************************************************************************
typedef struct socket_data                SOCKET_DATA;
typedef struct account_data               ACCOUNT_DATA;
typedef struct char_data                  CHAR_DATA;  
typedef struct storage_set                STORAGE_SET;
typedef struct storage_set_list           STORAGE_SET_LIST;
typedef struct prototype_data             PROTO_DATA;

typedef struct script_set_data            SCRIPT_SET;
typedef struct edesc_data                 EDESC_DATA;
typedef struct edesc_set_data             EDESC_SET;
typedef struct response_data              RESPONSE_DATA;

typedef struct script_data                SCRIPT_DATA;
typedef struct world_data                 WORLD_DATA;
typedef struct zone_data                  ZONE_DATA;
typedef struct room_data                  ROOM_DATA;
typedef struct exit_data                  EXIT_DATA;
typedef struct object_data                OBJ_DATA;
typedef struct shop_data                  SHOP_DATA;
typedef struct body_data                  BODY_DATA;
typedef struct reset_data                 RESET_DATA;
typedef struct reset_list                 RESET_LIST;


typedef long                              bitvector_t;
typedef unsigned char                     bool;

#ifdef MODULE_MTH

  #include <zlib.h>
  #include "mth/mth.h"
#else
  #include <arpa/telnet.h>
#endif

// these appear in so many places, we might as well just add 'em to our header
// here so we don't have to include them everywhere else. Only catch is we
// have to include them after all of our typedefs so the header files can use
// the typedefs.
#include "numbers.h"
#include "property_table.h"
#include "list.h"
#include "map.h"
#include "near_map.h"
#include "hashtable.h"
#include "set.h"
#include "buffer.h"
#include "bitvector.h"
#include "parse.h"
#include "command.h"
#include "filebuf.h"
#include "auxiliary.h"

//*****************************************************************************
// Standard definitions
//*****************************************************************************

/* define TRUE and FALSE */
#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE    1
#endif

#define eTHIN   0
#define eBOLD   1

/* A few globals */
#define DFLT_PULSES_PER_SECOND 10
#define PULSES_PER_SECOND   mudsettingGetInt("pulses_per_second")
#define SECOND              * PULSES_PER_SECOND   /* used for figuring out how many pulses in a second*/
#define SECONDS             SECOND                /* same as above */
#define MINUTE              * 60 SECONDS          /* one minute */
#define MINUTES             MINUTE
#define MAX_INPUT_LEN      2048                   /* max length of a string someone can input */
#define SMALL_BUFFER       2048
#define MAX_BUFFER         8192                   /* seems like a decent amount         */
#define MAX_SCRIPT         16384                  /* max length of a script */
#define MAX_OUTPUT         8192                   /* well shoot me if it isn't enough   */
#define FILE_TERMINATOR    "EOF"                  /* end of file marker                 */
#define COPYOVER_FILE      "../.copyover.dat"     /* tempfile to store copyover data    */
#define EXE_FILE           "../src/NakedMud"      /* the name of the mud binary         */
#define DEFAULT_PORT       4000                   /* the default port we run on */
#define SCREEN_WIDTH       80                     // the width of a term screen
#define PARA_INDENT        0                      // num of spaces to start para

/* Thread States */
#define TSTATE_LOOKUP          0  /* Socket is in host_lookup        */
#define TSTATE_DONE            1  /* The lookup is done.             */
#define TSTATE_WAIT            2  /* Closed while in thread.         */
#define TSTATE_CLOSED          3  /* Closed, ready to be recycled.   */

/* Communication Ranges */
#define COMM_LOG              10  /* admins only                     */

// these are there UIDs for things that have not yet been created
#define NOBODY               (-1)
#define NOTHING              (-1)
#define NOWHERE              (-1)

#define SOMEWHERE       "somewhere"
#define SOMETHING       "something"
#define SOMEONE         "someone"
#define NOTHING_SPECIAL "you see nothing special."

// the room that new characters are dropped into
#define START_ROOM      mudsettingGetString("start_room")
#define DFLT_START_ROOM "tavern_entrance@examples"

#define WORLD_PATH     "../lib/world"


#ifdef MODULE_MTH

typedef struct mud_data                   MUD_DATA;
typedef struct mth_data                   MTH_DATA;

//
// Here it is... the big ol' datastructure for sockets. Yum.
struct socket_data {
  CHAR_DATA     * player;
  ACCOUNT_DATA  * account;
  MTH_DATA      * mth;
  char          * hostname;
  char            inbuf[MAX_INPUT_LEN];
  BUFFER        * next_command;
  BUFFER        * iac_sequence;
  // char            next_command[MAX_BUFFER];
  bool            cmd_read;
  bool            bust_prompt;
  bool            closed;
  int             lookup_status;
  int             control;
  int             uid;
  double          idle;          // how many pulses have we been idle for?

  char          * page_string;   // the string that has been paged to us
  int             curr_page;     // the current page we're on
  int             tot_pages;     // the total number of pages the string has
  
  BUFFER        * text_editor;   // where we do our actual work
  BUFFER        * outbuf;        // our buffer of pending output

  LIST          * input_handlers;// a stack of our input handlers and prompts
  LIST          * input;         // lines of input we have received
  LIST          * command_hist;  // the commands we've executed in the past

  unsigned char   compressing;                 /* MCCP support */
  z_stream      * out_compress;                /* MCCP support */
  unsigned char * out_compress_buf;            /* MCCP support */

  AUX_TABLE     * auxiliary;     // auxiliary data installed by other modules
};

struct mth_data
{
        struct msdp_data ** msdp_data;
        char              * proxy;
        char              * terminal_type;
        char                telbuf[MAX_INPUT_LEN];
        int                 teltop;
        long long           mtts;
        int                 comm_flags;
        short               cols;
        short               rows;
        z_stream          * mccp2;
        z_stream          * mccp3;
};

struct msdp_data
{
        char     *value;
        int      flags;
};

#endif

//*****************************************************************************
// core functions for working with new commands
//*****************************************************************************
void init_commands();
void show_commands(CHAR_DATA *ch, const char *user_groups);
CMD_DATA *remove_cmd(const char *cmd);
void add_cmd      (const char *cmd, const char *sort_by, COMMAND(func),
	           const char *user_group, bool interrupts);
void add_py_cmd   (const char *cmd, const char *sort_by, void *pyfunc,
		   const char *user_group, bool interrupts);
bool cmd_exists   (const char *cmd);
void add_cmd_check(const char *cmd, CMD_CHK(func));
void add_py_cmd_check(const char *cmd, void *pyfunc);



//*****************************************************************************
// functions for setting and retreiving values of various mud settings. It's
// probably going to be common for people to add new settings that change how
// their mud will run (e.g. wizlock, newlock, max_level). Forcing people to
// handle all of this stuff through modules is really giving them more work
// than is neccessary. Instead, here we have a set of utilities that makes 
// doing this sort of stuff really easy. Whenever a new value is set, the
// settings are automagically saved. New variables can be created on the fly.
// There is no need to define keys anywhere. Conversion between types is
// handled automatically.
//*****************************************************************************
void init_mud_settings();

void mudsettingSetString(const char *key, const char *val);
void mudsettingSetDouble(const char *key, double val);
void mudsettingSetInt   (const char *key, int val);
void mudsettingSetLong  (const char *key, long val);
void mudsettingSetBool  (const char *key, bool val);

const char *mudsettingGetString(const char *key);
double      mudsettingGetDouble(const char *key);
int         mudsettingGetInt   (const char *key);
long        mudsettingGetLong  (const char *key);
bool        mudsettingGetBool  (const char *key);

//
// returns the next available UID for mobs, objs, room, exits
#define START_UID      1000000
int next_uid(void);
int  top_uid(void);



//*****************************************************************************
// Global Variables
//*****************************************************************************
extern  LIST             *object_list; // all objects currently in the game
extern  LIST             *socket_list; // all sockets currently conencted
extern  LIST             *mobile_list; // all mobiles currently in the game
extern  LIST               *room_list; // all rooms currently in the game

extern  SET               *object_set; // objects, set form
extern  SET               *mobile_set; // mobiles, set form
extern  SET                 *room_set; // rooms, set form

extern  LIST          *mobs_to_delete; // mobs/objs/rooms that have had
extern  LIST          *objs_to_delete; // extraction and now need 
extern  LIST         *rooms_to_delete; // extract_final
extern  LIST          *strs_to_delete; // strings we didn't want deleted at 
                                       // the time, but do now. This is for
                                       // get_fullkey and see_xxx_as
extern  LIST          *bufs_to_delete; // same for buffers

extern  PROPERTY_TABLE     *mob_table; // a mapping between uid and mob
extern  PROPERTY_TABLE     *obj_table; // a mapping between uid and obj
extern  PROPERTY_TABLE    *room_table; // a mapping between uid and room
extern  PROPERTY_TABLE    *exit_table; // a mapping between uid and exit
extern  PROPERTY_TABLE    *sock_table; // a mapping between uid and socket

extern  bool                shut_down; // used for shutdown
extern  int                   mudport; // What port are we running on?
extern  BUFFER              *greeting; // the welcome greeting
extern  BUFFER                  *motd; // the MOTD message
extern  int                   control; // boot control socket thingy
extern  time_t           current_time; // let's cut down on calls to time()

extern  WORLD_DATA         *gameworld; // database and thing that holds rooms



//*****************************************************************************
// MCCP support
//*****************************************************************************

extern const unsigned char compress_will[];
extern const unsigned char compress_will2[];

#define TELOPT_COMPRESS       85
#define TELOPT_COMPRESS2      86
#define COMPRESS_BUF_SIZE   8192


//*****************************************************************************
// Prototype function declerations
//*****************************************************************************
char  *crypt                  ( const char *key, const char *salt );


/* interpret.c */
void  handle_cmd_input        ( SOCKET_DATA *dsock, char *arg );
void  do_cmd                  ( CHAR_DATA *ch, char *arg, bool aliases_ok);

/* io.c */
void    log_string            ( const char *txt, ... ) __attribute__ ((format (printf, 1, 2)));
void    bug                   ( const char *txt, ... ) __attribute__ ((format (printf, 1, 2)));
BUFFER *read_file             ( const char *file );

/* strings.c */
const char *one_arg_safe      ( const char *fStr, char *bStr );
char   *one_arg               ( char *fStr, char *bStr );
char   *two_args              ( char *from, char *arg1, char *arg2);
char   *three_args            ( char *from, char *arg1, char *arg2, char *arg3);
void    arg_num               ( const char *from, char *to, int num); 
bool    compares              ( const char *aStr, const char *bStr );
bool    is_prefix             ( const char *aStr, const char *bStr );
char   *capitalize            ( char *txt );
char   *strfind               (char *txt, char *sub);

/* mccp.c */
bool  compressStart     ( SOCKET_DATA *dsock, unsigned char teleopt );
bool  compressEnd       ( SOCKET_DATA *dsock, unsigned char teleopt, bool forced );

/* socket.c */
#define NUM_LINES_PER_PAGE  21
void  page_string           ( SOCKET_DATA *dsock, const char *string);
void  page_continue         ( SOCKET_DATA *dsock);
void  page_back             ( SOCKET_DATA *dsock);

// socket stuff
#define charHasSplitScreen(ch)   (charGetInt(ch, "splitscreen") == TRUE)
#define charGetPageLen(ch)       NUM_LINES_PER_PAGE
#define charGetPageWidth(ch)     80

//
// adds a new input handler onto the stack that allows a person to read 
// long pages of text (e.g. helpfiles in OLC)
void  start_reader          ( SOCKET_DATA *dsock, const char *text);

#endif  /* MUD_H */
