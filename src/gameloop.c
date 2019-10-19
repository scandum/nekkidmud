//*****************************************************************************
//
// gameloop.c
//
// contains the entrypoint for the MUD, plus various state-handling functions.
//
//*****************************************************************************
#include <sys/time.h>

#include "mud.h"
#include "utils.h"
#include "save.h"
#include "socket.h"
#include "world.h"
#include "room.h"
#include "room_reset.h"
#include "character.h"
#include "object.h"
#include "exit.h"
#include "log.h"
#include "action.h"
#include "event.h"
#include "auxiliary.h"
#include "storage.h"
#include "races.h"
#include "inform.h"
#include "hooks.h"

//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "editor/editor.h"
#include "dyn_vars/dyn_vars.h"
#include "items/items.h"
#include "olc2/olc.h"
#include "set_val/set_val.h"
#include "scripts/scripts.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_TIME
#include "time/mudtime.h"
#endif
#ifdef MODULE_SOCIALS
#include "socials/socials.h"
#endif
#ifdef MODULE_HELP2
#include "help2/help.h"
#endif
#ifdef MODULE_ALIAS
#include "alias/alias.h"
#endif
#ifdef MODULE_MTH
#include "mth/mud.h"
#include "mth/mth.h"
#endif

// local procedures
void game_loop    ( int control );
bool gameloop_end = FALSE;

// intialize shutdown state
bool shut_down    = FALSE;
int  control;

// what port are we running on?
int mudport       = -1;

// global variables
WORLD_DATA      *gameworld = NULL; // the gameworld, and ll the prototypes

LIST          *object_list = NULL; // the list of all existing objects
LIST          *socket_list = NULL; // the list of active sockets
LIST          *mobile_list = NULL; // the list of existing mobiles
LIST            *room_list = NULL; // the list of all existing rooms

SET            *object_set = NULL; // same things, stored in a set
SET            *mobile_set = NULL; // and mobiles
SET              *room_set = NULL; // amd rooms

LIST       *mobs_to_delete = NULL; // mobs pending final extraction
LIST       *objs_to_delete = NULL; // objs pending final extraction
LIST      *rooms_to_delete = NULL; // rooms pending final extraction
LIST       *strs_to_delete = NULL; // strings waiting to be freed
LIST       *bufs_to_delete = NULL; // buffers waiting to be freed
PROPERTY_TABLE  *mob_table = NULL; // a table of mobs by UID, for quick lookup
PROPERTY_TABLE  *obj_table = NULL; // a table of objs by UID, for quick lookup
PROPERTY_TABLE *room_table = NULL; // a table of rooms by UID, for quick lookup
PROPERTY_TABLE *exit_table = NULL; // a table of exits by UID, for quick lookup
PROPERTY_TABLE *sock_table = NULL; // a table of socks by UID, for quick lookup
BUFFER           *greeting = NULL; // message seen when a socket connects
BUFFER               *motd = NULL; // what characters see when they log on



//
// This is where it all starts, nothing special.
int main(int argc, char **argv)
{
  extern fd_set fSet;
  int i;
  bool fCopyOver = FALSE;

  /************************************************************/
  /*                      PARSE OPTIONS                       */
  /************************************************************/
  for(i = 1; i < argc-1; i++) {
    if(!strcasecmp(argv[i], "-copyover")) {
      fCopyOver = TRUE;
      control = atoi(argv[++i]);
    }
    else {
      char buf[SMALL_BUFFER];
      sprintf(buf, "Invalid argument, %s (#%d)\r\n", argv[i], i);
      perror(buf);
      return 1;
    }
  }

  // port number not supplied... just use default
  if(i > argc-1)
    mudport = DEFAULT_PORT;
  else if(i == argc-1)
    mudport = atoi(argv[argc-1]);
  else {
    perror("Arguments were not parsed properly. Could not start MUD.\r\n");
    return 1;
  }

  /* seed the random number generator */
  srand(time(0));

  /* get the current time */
  current_time = time(NULL);

  #ifdef MODULE_MTH
  {
    init_mth();
  }
  #endif

  /************************************************************/
  /*           INITIALIZE ALL OUR LISTS AND TABLES            */
  /************************************************************/

  // lists for storing objects, sockets, and mobiles that are
  // currently loaded into the game
  object_list     = newList();
  socket_list     = newList();
  mobile_list     = newList();
  room_list       = newList();

  object_set      = newSet();
  mobile_set      = newSet();
  room_set        = newSet();

  mobs_to_delete  = newList();
  objs_to_delete  = newList();
  rooms_to_delete = newList();
  strs_to_delete  = newList();
  bufs_to_delete  = newList();

  // tables for quick lookup of mobiles and objects by UID.
  // For optimal speed, the table sizes should be roughly
  // 25% larger than the number of mobs/objs you intend to have
  // loaded into the game at any given time.
  mob_table   = newPropertyTable(charGetUID,  3000);
  obj_table   = newPropertyTable(objGetUID,   3000);
  room_table  = newPropertyTable(roomGetUID,  3000);
  exit_table  = newPropertyTable(exitGetUID,  9000);
  sock_table  = newPropertyTable(socketGetUID,1000);

  // make a new world
  gameworld = newWorld();



  /************************************************************/
  /*        INITIALIZE OUR SETTINGS AND BASIC SYSTEMS         */
  /************************************************************/

  // change to the lib directory
  log_string("Changing to lib directory.");
  chdir("../lib");

  log_string("Initializing hooks.");
  init_hooks();

  log_string("Initializing bitvectors.");
  init_bitvectors();

  log_string("Initializing races and default bodies.");
  init_races();

  log_string("Initializing inform system.");
  init_inform();

  log_string("Initializing room resets.");
  init_room_reset();

  log_string("Initializing MUD settings.");
  init_mud_settings();

  log_string("Preparing auxiliary data for usage.");
  init_auxiliaries();

  log_string("Initializing command table.");
  init_commands();

  log_string("Initializing action handler.");
  init_actions();

  log_string("Initializing event handler.");
  init_events();

  log_string("Initializing logging system.");
  init_logs();

  log_string("Initializing account and player database.");
  init_save();



  /**********************************************************************/
  /*                START MANDATORY MODULE INSTALLATION                 */
  /**********************************************************************/
  log_string("Initializing OLC v2.0.");
  init_olc2();

  log_string("Initializing item types.");
  init_items();

  log_string("Initializing editor.");
  init_editor();
  init_notepad();

  log_string("Initializing dynamic variables.");
  init_dyn_vars();

  log_string("Initializing set utility.");
  init_set();


  
  /**********************************************************************/
  /*                 START OPTIONAL MODULE INSTALLATION                 */
  /**********************************************************************/
#ifdef MODULE_ALIAS
  log_string("Initializing aliases.");
  init_aliases();
#endif

#ifdef MODULE_TIME
  log_string("Initializing game time.");
  init_time();
#endif

#ifdef MODULE_SOCIALS
  log_string("Initializing socials.");
  init_socials();
#endif

#ifdef MODULE_HELP2
  log_string("Initializing helpfiles.");
  init_help();
#endif



  /**********************************************************************/
  /*                   SET UP ALL OF OUR PYTHON STUFF                   */
  /**********************************************************************/

  //
  // this HAS to be the last module initialized, to allow all the other
  // modules to add get/setters and methods to the Python incarnations of
  // our various datatypes.
  log_string("Initializing scripts.");
  init_scripts();



  /**********************************************************************/
  /*                          SPAWN THE WORLD                           */
  /**********************************************************************/
  /* load all game data */
  log_string("Loading gameworld.");
  load_muddata();

  // force-pulse everything once
  log_string("Force-resetting world");
  worldForceReset(gameworld);



  /**********************************************************************/
  /*                  HANDLE THE SOCKET STARTUP STUFF                   */
  /**********************************************************************/
  /* initialize the socket */
  if (!fCopyOver) {
    log_string("Initializing sockets.");
    control = init_socket();
  }

  /* clear out the file socket set */
  FD_ZERO(&fSet);

  /* add control to the set */
  FD_SET(control, &fSet);

  // attach our old sockets
  if(fCopyOver)
    copyover_recover();



  /**********************************************************************/
  /*             START THE GAME UP, AND HANDLE ITS SHUTDOWN             */
  /**********************************************************************/
  // main game loop
  log_string("Entering game loop");
  game_loop(control);

  // run our finalize hooks
  hookRun("shutdown", "");

  // close down the socket
  close(control);

  // terminated without errors
  log_string("Program terminated without errors.");

  return 0;
}


//
// let all of our actions and events know that time has gone by.
// Also, give a chance of doing some resetting in the game.
//
void update_handler()
{
  static int num_updates = 0;
  // increment the number of updates we've done
  num_updates++;

  // pulse actions and events -> one pulse
  pulse_actions(1);
  pulse_events(1);

  // pulse world
  // we don't want to be on the same schedule as
  // everything else, that updates every PULSES_PER_SECOND. 
  // We want to be on a schedule that updates every minute or so.
  if((num_updates % (1 MINUTE)) == 0)
    worldPulse(gameworld);

  // if we have final extractions pending, do them
  CHAR_DATA *ch = NULL;
  while((ch = (CHAR_DATA *)listPop(mobs_to_delete)) != NULL)
    extract_mobile_final(ch);
  OBJ_DATA *obj = NULL;
  while((obj = (OBJ_DATA *)listPop(objs_to_delete)) != NULL)
    extract_obj_final(obj);
  ROOM_DATA *room = NULL;
  while((room = (ROOM_DATA *)listPop(rooms_to_delete)) != NULL)
    extract_room_final(room);
  char *str = NULL;
  while((str = (char *)listPop(strs_to_delete)) != NULL)
    free(str);
  BUFFER *buf = NULL;
  while((buf = (BUFFER *)listPop(bufs_to_delete)) != NULL)
    deleteBuffer(buf);
}



void game_loop(int control)   
{
  static struct timeval tv;
  struct timeval last_time, new_time;
  extern fd_set fSet;
  extern fd_set rFd;
  long secs, usecs;

  /* set this for the first loop */
  gettimeofday(&last_time, NULL);

  /* do this untill the program is shutdown */
  while (!shut_down) {
    /* set current_time */
    current_time = time(NULL);

    /* copy the socket set */
    memcpy(&rFd, &fSet, sizeof(fd_set));

    /* wait for something to happen */
    if (select(FD_SETSIZE, &rFd, NULL, NULL, &tv) < 0)
      continue;

    /* check for new connections */
    if (FD_ISSET(control, &rFd)) {
      struct sockaddr_in sock;
      unsigned int socksize;
      int newConnection;

      socksize = sizeof(sock);
      if ((newConnection = accept(control, (struct sockaddr*) &sock, &socksize)) >=0) {
        SOCKET_DATA *newsock = new_socket(newConnection);
	if(newsock != NULL) {
	  hookRun("receive_connection", hookBuildInfo("sk", newsock));
	  socketBustPrompt(newsock);
	}
      }
    }


    /* check all of the sockets for input */
    input_handler();

    /* call the top-level update handler for events and actions */
    update_handler();

    /* send socket output */
    output_handler();

    /*
     * Here we sleep out the rest of the pulse, thus forcing
     * SocketMud(tm) (NakedMud) to run at PULSES_PER_SECOND pulses each second.
     */
    gettimeofday(&new_time, NULL);

    // get the time right now, and calculate how long we should sleep
    usecs = (int) (last_time.tv_usec -  new_time.tv_usec) + 1000000 / PULSES_PER_SECOND;
    secs  = (int) (last_time.tv_sec  -  new_time.tv_sec);

    //
    // Now we make sure that 0 <= usecs < 1.000.000
    //
    while (usecs < 0)
    {
      usecs += 1000000;
      secs  -= 1;
    }
    while (usecs >= 1000000)
    {
      usecs -= 1000000;
      secs  += 1;
    }

    // if secs < 0 we don't sleep, since we have encountered a laghole
    if (secs > 0 || (secs == 0 && usecs > 0))
    {
      struct timeval sleep_time;

      sleep_time.tv_usec = usecs;
      sleep_time.tv_sec  = secs;

      if (select(0, NULL, NULL, NULL, &sleep_time) < 0)
        continue;
    }

    /* reset the last time we where sleeping */
    gettimeofday(&last_time, NULL);

    /* recycle sockets */
    recycle_sockets();
  }
}
