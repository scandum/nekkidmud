//*****************************************************************************
//
// socket.c
//
// This file contains the socket code, used for accepting new connections as 
// well as reading and writing to sockets, and closing down unused sockets.
//
//*****************************************************************************

#include "wrapsock.h"
#include <netdb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h> 
#include <zlib.h>
#include <pthread.h>

#include "mud.h"
#include "character.h"
#include "account.h"
#include "save.h"
#include "utils.h"
#include "socket.h"
#include "auxiliary.h"
#include "hooks.h"
#include "scripts/scripts.h"
#include "scripts/pyplugs.h"
#include "dyn_vars/dyn_vars.h"



//*****************************************************************************
// optional modules
//*****************************************************************************
#ifdef MODULE_ALIAS
#include "alias/alias.h"
#endif

#ifdef MODULE_MTH
#include "mth/mud.h"
#include "mth/mth.h"
#endif
#ifdef MODULE_COLOR
int substitute_color(const char *input, char *output, int colors);
#endif

// provides a unique identifier number to every socket that connects to the
// mud. Used mostly for referring to sockets in Python
#define START_SOCK_UID           1
int next_sock_uid = START_SOCK_UID;


#ifndef MODULE_MTH
//
// Here it is... the big ol' datastructure for sockets. Yum.
struct socket_data {
  CHAR_DATA     * player;
  ACCOUNT_DATA  * account;
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
#endif

//
// contains an input handler and the socket prompt in one structure, so they
// can be stored together in the socket_data. Allows for the option of Python
// or C input handlers and prompt pairs.
typedef struct input_handler_data {
  void *handler; // (* handler)(SOCKET_DATA *, char *);
  void  *prompt; // (*  prompt)(SOCKET_DATA *);
  bool   python;
  char   *state; // what state does this input handler represent?
} IH_PAIR;


//
// required for looking up a socket's IP in a new thread
typedef struct lookup_data {
  SOCKET_DATA    * dsock;   // the socket we wish to do a hostlookup on
  char           * buf;     // the buffer it should be stored in
} LOOKUP_DATA;



/* global variables */
fd_set        fSet;             /* the socket list for polling       */
fd_set        rFd;

/* mccp support */
const unsigned char compress_will   [] = { IAC, WILL, TELOPT_COMPRESS,  '\0' };
const unsigned char compress_will2  [] = { IAC, WILL, TELOPT_COMPRESS2, '\0' };

// used to delete an input handler pair
void deleteInputHandler(IH_PAIR *pair) {
  if(pair->python) {
    Py_XDECREF((PyObject *)pair->handler);
    Py_XDECREF((PyObject *)pair->prompt);
  }
  if(pair->state) free(pair->state);
  free(pair);
}

/*
 * Init_socket()
 *
 * Used at bootup to get a free
 * socket to run the server from.
 */
int init_socket()
{
  struct sockaddr_in my_addr;
  int sockfd, reuse = 1;

  /* let's grab a socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  /* setting the correct values */
  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = INADDR_ANY;
  my_addr.sin_port = htons(mudport);

  /* this actually fixes any problems with threads */
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) == -1)
  {
    perror("Error in setsockopt()");
    exit(1);
  } 

  /* bind the port */
  bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr));

  /* start listening already :) */
  listen(sockfd, 3);

  /* return the socket */
  return sockfd;
}


/* 
 * New_socket()
 *
 * Initializes a new socket, get's the hostname
 * and puts it in the active socket_list.
 */
SOCKET_DATA *new_socket(int sock)
{
  struct sockaddr_in   sock_addr;
  pthread_attr_t       attr;
  pthread_t            thread_lookup;
  LOOKUP_DATA        * lData;
  SOCKET_DATA        * sock_new;
  int                  argp = 1;
  socklen_t            size;

  /* initialize threads */
  pthread_attr_init(&attr);   
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  /* create and clear the socket */
  sock_new = calloc(1, sizeof(SOCKET_DATA));

  /* attach the new connection to the socket list */
  FD_SET(sock, &fSet);

  /* clear out the socket */
  clear_socket(sock_new, sock);
  sock_new->closed = FALSE;

  /* set the socket as non-blocking */
  ioctl(sock, FIONBIO, &argp);

  /* update the socket list and table */
  listPut(socket_list, sock_new);
  propertyTablePut(sock_table, sock_new);

  /* do a host lookup */
  size = sizeof(sock_addr);
  if (getpeername(sock, (struct sockaddr *) &sock_addr, &size) < 0)
  {
    perror("New_socket: getpeername");
    sock_new->hostname = strdup("unknown");
  }
  else
  {
    /* set the IP number as the temporary hostname */
    sock_new->hostname = strdup(inet_ntoa(sock_addr.sin_addr));

    if (!compares(sock_new->hostname, "127.0.0.1"))
    {
      /* allocate some memory for the lookup data */
      if ((lData = malloc(sizeof(*lData))) == NULL)
      {
        bug("New_socket: Cannot allocate memory for lookup data.");
        abort();
      }

      /* Set the lookup_data for use in lookup_address() */
      lData->buf    =  strdup((char *) &sock_addr.sin_addr);
      lData->dsock  =  sock_new;

      /* dispatch the lookup thread */
      pthread_create(&thread_lookup, &attr, &lookup_address, (void*) lData);
    }
    else sock_new->lookup_status++;
  }

  #ifdef MODULE_MTH
      init_mth_socket(sock_new);
      announce_support(sock_new);
  #endif
  /* negotiate compression */
  // text_to_buffer(sock_new, (char *) compress_will2);
  // text_to_buffer(sock_new, (char *) compress_will);

  /* send the greeting */
  // text_to_buffer(sock_new, bufferString(greeting));

  /* everything went as it was supposed to */
  return sock_new;
}


/*
 * Close_socket()
 *
 * Will close one socket directly, freeing all
 * resources and making the socket availably on
 * the socket free_list.
 */
void close_socket(SOCKET_DATA *dsock, bool reconnect)
{
  if (dsock->lookup_status > TSTATE_DONE) return;
  dsock->lookup_status += 2;

  /* remove the socket from the polling list */
  FD_CLR(dsock->control, &fSet);

  /* remove ourself from the list */
  //
  // NO! We don't want to remove ourself from the list just yet.
  // We will do that the next time we show up in the game_loop.
  // removing now will not close the socket completely. Why, I'm not
  // entirely sure... but it doesn't ;)
  //   - Geoff, Dec26/04
  //
  //  listRemove(socket_list, dsock);
  //

  // we have a character, and it's one that's
  // not in the process of being created
  if (dsock->player && charGetUID(dsock->player) != NOBODY) {
    if (reconnect)
      text_to_socket(dsock, "This connection has been taken over.\r\n", 0);
    else {
      charSetSocket(dsock->player, NULL);
      log_string("Closing link to %s", charGetName(dsock->player));
    }
  }
  else if(dsock->player) {
    charSetSocket(dsock->player, NULL);
    extract_mobile(dsock->player);
  }
  
  if(dsock->account) {
    if(account_exists(accountGetName(dsock->account)))
      unreference_account(dsock->account);
    else
      deleteAccount(dsock->account);
  }

  /* set the closed state */
  dsock->closed = TRUE;
  //  dsock->state = STATE_CLOSED;
}


/* 
 * Read_from_socket()
 *
 * Reads one line from the socket, storing it
 * in a buffer for later use. Will also close
 * the socket if it tries a buffer overflow.
 */
bool read_from_socket(SOCKET_DATA *dsock)
{
  int size;
  extern int errno;

  /* check for buffer overflows, and drop connection in that case */
  size = strlen(dsock->inbuf);
  if (size >= sizeof(dsock->inbuf) - 2)
  {
    text_to_socket(dsock, "\n\r!!!! Input Overflow !!!!\n\r", 0);
    return FALSE;
  }

  #ifdef MODULE_MTH
  {
    unsigned char input[MAX_INPUT_LEN];
    int sInput, wanted;

    wanted = sizeof(dsock->inbuf) - 2 - size;
    sInput = read(dsock->control, input, wanted);
    
    if (sInput <= 0)
    {
      perror("Read_from_socket");
      return FALSE;
    }
    input[sInput] = 0;

    size = translate_telopts(dsock, input, sInput, (unsigned char *) dsock->inbuf, size);

    return TRUE;
  }
  #else
  /* start reading from the socket */
  for (;;)
  {
    int sInput;
    int wanted = sizeof(dsock->inbuf) - 2 - size;

    sInput = read(dsock->control, dsock->inbuf + size, wanted);

    if (sInput > 0)
    {
      size += sInput;

      if (dsock->inbuf[size-1] == '\n' || dsock->inbuf[size-1] == '\r')
        break;
    }
    else if (sInput == 0)
    {
      log_string("Read_from_socket: EOF");
      return FALSE;
    }
    else if (errno == EAGAIN || sInput == wanted)
      break;
    else
    {
      perror("Read_from_socket");
      return FALSE;
    }
  }
  dsock->inbuf[size] = '\0';
  return TRUE;
  #endif
}


/*
 * Text_to_socket()
 *
 * Sends text directly to the socket,
 * will compress the data if needed.
 */
bool text_to_socket(SOCKET_DATA *dsock, const char *txt, int length)
{
  int iBlck, iPtr, iWrt = 0, control = dsock->control;

  if (length == 0)
  {
    length = strlen(txt);
  }

  #ifdef MODULE_MTH
  if (dsock && dsock->mth->mccp2)
  {
    write_mccp2(dsock, (char *) txt, length);

    if (HAS_BIT(dsock->mth->comm_flags, COMM_FLAG_DISCONNECT))
    {
      return FALSE;
    }
    else
    {
      return TRUE;
    }
  }
  #endif
  /* write compressed */
  if (dsock && dsock->out_compress)
  {
    dsock->out_compress->next_in  = (unsigned char *) txt;
    dsock->out_compress->avail_in = length;

    while (dsock->out_compress->avail_in)
    {
      dsock->out_compress->avail_out = COMPRESS_BUF_SIZE - (dsock->out_compress->next_out - dsock->out_compress_buf);

      if (dsock->out_compress->avail_out)
      {
        int status = deflate(dsock->out_compress, Z_SYNC_FLUSH);

        if (status != Z_OK)
        return FALSE;
      }

      length = dsock->out_compress->next_out - dsock->out_compress_buf;
      if (length > 0)
      {
        for (iPtr = 0; iPtr < length; iPtr += iWrt)
        {
          iBlck = UMIN(length - iPtr, 4096);
          if ((iWrt = write(control, dsock->out_compress_buf + iPtr, iBlck)) < 0)
          {
            perror("Text_to_socket (compressed):");
            return FALSE;
          }
        }
        if (iWrt <= 0) break;
        if (iPtr > 0)
        {
          if (iPtr < length)
            memmove(dsock->out_compress_buf, dsock->out_compress_buf + iPtr, length - iPtr);

          dsock->out_compress->next_out = dsock->out_compress_buf + length - iPtr;
        }
      }
    }
    return TRUE;
  }

  /* write uncompressed */
  for (iPtr = 0; iPtr < length; iPtr += iWrt)
  {
    iBlck = UMIN(length - iPtr, 4096);
    if ((iWrt = write(control, txt + iPtr, iBlck)) < 0)
    {
      perror("Text_to_socket:");
      return FALSE;
    }
  }

  return TRUE;
}


void  send_to_socket( SOCKET_DATA *dsock, const char *format, ...) {
  if(format && *format) {
    static char buf[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, MAX_BUFFER, format, args);
    va_end(args);
    text_to_buffer(dsock, buf);
  }
}


/*
 * Text_to_buffer()
 *
 * Stores outbound text in a buffer, where it will
 * stay untill it is flushed in the gameloop.
 *
 * Will also parse ANSI colors and other tags.
 */
void text_to_buffer(SOCKET_DATA *dsock, const char *txt)
{
  // if we're at the head of the outbuf and haven't entered a command, 
  // also copy a newline so we're not printing in front of the prompt
  if(bufferLength(dsock->outbuf) == 0 && !dsock->bust_prompt)
    bufferCat(dsock->outbuf, "\r\n");

  #ifdef MODULE_COLOR
  {
    char *str = malloc(100 + strlen(txt) * 6);

    // It's set to 256 colors until a configuration system is added.

    substitute_color(txt, str, 256);

    bufferCat(dsock->outbuf, str);

    free(str);
  }
  #else
  {
    /* add data to buffer */
    bufferCat(dsock->outbuf, txt);
  }
  #endif
}

//
// read an IAC sequence from the socket's input buffer. When we hit a
// termination point, send a hook and clear he buffer. Return how many
// characters to skip ahead in the input handler
int read_iac_sequence(SOCKET_DATA *dsock, int start) {
  // are we doing subnegotiation?
  bool subneg = strchr(bufferString(dsock->iac_sequence), SB) != NULL;
  int       i = start;
  bool   done = FALSE;

  for(; dsock->inbuf[i] != '\0'; i++) {
    // are we looking for IAC?
    if(bufferLength(dsock->iac_sequence) == 0) {
      // Oops! Something is broken
      if(dsock->inbuf[i] != (signed char) IAC)
	return 0;
      else
	bufferCatCh(dsock->iac_sequence, IAC);
    }

    // are we looking for a command?
    else if(bufferLength(dsock->iac_sequence) == 1) {
      // did we find subnegotiation?
      if(dsock->inbuf[i] == (signed char) SB) {
	bufferCatCh(dsock->iac_sequence, dsock->inbuf[i]);
	subneg = TRUE;
      }

      // basic three-character command
      else if(dsock->inbuf[i] == (signed char) WILL || 
	      dsock->inbuf[i] == (signed char) WONT || 
	      dsock->inbuf[i] == (signed char) DO   || 
	      dsock->inbuf[i] == (signed char) DONT)
	bufferCatCh(dsock->iac_sequence, dsock->inbuf[i]);

      // something went wrong
      else {
	bufferClear(dsock->iac_sequence);
	return 2;
      }
    }
    
    // are we doing subnegotiation?
    else if(subneg) {
      int       last_i = bufferLength(dsock->iac_sequence) - 2;
      signed char last = bufferString(dsock->iac_sequence)[last_i];
      bufferCatCh(dsock->iac_sequence, dsock->inbuf[i]);

      // have hit the end of the subnegotiation? Break out if so
      if(last == (signed char) IAC && dsock->inbuf[i] == (signed char) SE) {
	done = TRUE;
	break;
      }
    }

    // this is the end
    else {
      bufferCatCh(dsock->iac_sequence, dsock->inbuf[i]);
      done = TRUE;
      break;
    }
  }
  
  int len = i - start + (dsock->inbuf[i] == '\0' ? 0 : 1);

  // broadcast the message we parsed, and prepare for the next sequence
  if(done == TRUE) {
    hookRun("receive_iac", 
	    hookBuildInfo("sk str", dsock,bufferString(dsock->iac_sequence)));
    bufferClear(dsock->iac_sequence);
  }
  return len;
}

void next_cmd_from_buffer(SOCKET_DATA *dsock) {
  // do we have stuff in our input list? If so, use that instead of inbuf
  dsock->cmd_read = FALSE;
  if(listSize(dsock->input) > 0) {
    char *cmd = listPop(dsock->input);
    bufferClear(dsock->next_command);
    bufferCat(dsock->next_command, cmd);
    dsock->cmd_read    = TRUE;
    dsock->bust_prompt = TRUE;
    free(cmd);
  }
  else {
    int i = 0, cmd_end = -1;

    // are we building an IAC command? Try to continue it
    if(bufferLength(dsock->iac_sequence) > 0)
      i += read_iac_sequence(dsock, 0);

    // copy over characters until we hit a newline, an IAC command, or \0
    for(; dsock->inbuf[i] != '\0' && cmd_end < 0; i++) {
      switch(dsock->inbuf[i]) {
      default:
	// append us to the command
	bufferCatCh(dsock->next_command, dsock->inbuf[i]);
	break;
      case '\n':
	// command end found
	cmd_end = ++i;
      case '\r':
	// ignore \r ... only pay attention to \n
	break;
      case (signed char) IAC:
	i += read_iac_sequence(dsock, i) - 1;
	break;
      }
      if(cmd_end >= 0)
	break;
    }

    // move the context of inbuf down
    int begin = 0;
    while(dsock->inbuf[i] != '\0')
      dsock->inbuf[begin++] = dsock->inbuf[i++];
    dsock->inbuf[begin] = '\0';
    
    // did we find a command?
    if(cmd_end >= 0) {
      dsock->cmd_read    = TRUE;
      dsock->bust_prompt = TRUE;
    }
  }
}

bool flush_output(SOCKET_DATA *dsock) {
  bool  success = TRUE;
  BUFFER   *buf = NULL;

  // run any hooks prior to flushing our text
  hookRun("flush", hookBuildInfo("sk", dsock));

  // quit if we have no output and don't need/can't have a prompt
  if(bufferLength(dsock->outbuf) <= 0 && 
     (!dsock->bust_prompt || !socketHasPrompt(dsock)))
    return success;

  buf = newBuffer(1);

  // send our outbound text
  if(bufferLength(dsock->outbuf) > 0) {
    hookRun("process_outbound_text",  hookBuildInfo("sk", dsock));
    hookRun("finalize_outbound_text", hookBuildInfo("sk", dsock));
    //success = text_to_socket(dsock, bufferString(dsock->outbuf), 0);
    bufferCat(buf, bufferString(dsock->outbuf));
    bufferClear(dsock->outbuf);
  }

  // send our prompt
  if(dsock->bust_prompt && success) {
    socketShowPrompt(dsock);
    hookRun("process_outbound_prompt",  hookBuildInfo("sk", dsock));
    hookRun("finalize_outbound_prompt", hookBuildInfo("sk", dsock));
    //success = text_to_socket(dsock, bufferString(dsock->outbuf), 0);
    bufferCat(buf, bufferString(dsock->outbuf));
    bufferClear(dsock->outbuf);
    dsock->bust_prompt = FALSE;
  }

  success = text_to_socket(dsock, bufferString(buf), bufferLength(buf));
  deleteBuffer(buf);

  // return our success
  return success;
}



//*****************************************************************************
//
// SOCKET MAINTENANCE
//
// Functions below this point are mainly concerned with the upkeep and
// maintenance of sockets (making sure they are initialized, garbage collected, 
// getting their addresses, etc...)
//
//*****************************************************************************
void deleteSocket(SOCKET_DATA *sock) {
  if(sock->hostname)      free(sock->hostname);
  if(sock->page_string)   free(sock->page_string);
  if(sock->text_editor)   deleteBuffer(sock->text_editor);
  if(sock->outbuf)        deleteBuffer(sock->outbuf);
  if(sock->next_command)  deleteBuffer(sock->next_command);
  if(sock->iac_sequence)  deleteBuffer(sock->iac_sequence);
  if(sock->input_handlers)deleteListWith(sock->input_handlers,deleteInputHandler);
  if(sock->input)         deleteListWith(sock->input, free);
  if(sock->command_hist)  deleteListWith(sock->command_hist, free);
  if(sock->auxiliary)     deleteAuxiliaryData(sock->auxiliary);
  free(sock);
}

void clear_socket(SOCKET_DATA *sock_new, int sock)
{
  if(sock_new->page_string)    free(sock_new->page_string);
  if(sock_new->text_editor)    deleteBuffer(sock_new->text_editor);
  if(sock_new->outbuf)         deleteBuffer(sock_new->outbuf);
  if(sock_new->next_command)   deleteBuffer(sock_new->next_command);
  if(sock_new->iac_sequence)   deleteBuffer(sock_new->iac_sequence);
  if(sock_new->input_handlers) deleteListWith(sock_new->input_handlers, deleteInputHandler);
  if(sock_new->auxiliary)      deleteAuxiliaryData(sock_new->auxiliary);
  if(sock_new->input)          deleteListWith(sock_new->input, free);
  if(sock_new->command_hist)   deleteListWith(sock_new->command_hist, free);

  bzero(sock_new, sizeof(*sock_new));
  sock_new->auxiliary = newAuxiliaryData(AUXILIARY_TYPE_SOCKET);
  sock_new->input_handlers = newList();
  sock_new->input          = newList();
  sock_new->command_hist   = newList();
  sock_new->control        = sock;
  sock_new->lookup_status  = TSTATE_LOOKUP;
  sock_new->uid            = next_sock_uid++;

  sock_new->text_editor    = newBuffer(1);
  sock_new->outbuf         = newBuffer(MAX_OUTPUT);
  sock_new->next_command   = newBuffer(1);
  sock_new->iac_sequence   = newBuffer(1);
}


/* does the lookup, changes the hostname, and dies */
void *lookup_address(void *arg)
{
  LOOKUP_DATA *lData = (LOOKUP_DATA *) arg;
  struct hostent *from = 0;
  //  struct hostent ent;
  //  char buf[16384];
  //  int err;

  /* do the lookup and store the result at &from */
  from = gethostbyaddr(lData->buf, sizeof(lData->buf), AF_INET);

  /* did we get anything ? */
  if (from && from->h_name)
  {
    free(lData->dsock->hostname);
    lData->dsock->hostname = strdup(from->h_name);
  }

  /* set it ready to be closed or used */
  lData->dsock->lookup_status++;

  /* free the lookup data */
  free(lData->buf);
  free(lData);

  /* and kill the thread */
  pthread_exit(0);
  return NULL;
}


void recycle_sockets()
{
  SOCKET_DATA *dsock;
  LIST_ITERATOR *sock_i = newListIterator(socket_list);

  ITERATE_LIST(dsock, sock_i) {
    if (dsock->lookup_status != TSTATE_CLOSED) 
      continue;

    /* remove the socket from the main list */
    listRemove(socket_list, dsock);
    propertyTableRemove(sock_table, dsock->uid);

    /* stop compression */
    #ifdef MODULE_MTH
      end_mccp2(dsock);
      end_mccp3(dsock);
    #else
      compressEnd(dsock, dsock->compressing, TRUE);
    #endif

    /* close the socket */
    close(dsock->control);

    /* delete the socket from memory */
    deleteSocket(dsock);
  } deleteListIterator(sock_i);
}


/* reset all of the sockets' control values */
void reconnect_copyover_sockets() {
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL; 
  ITERATE_LIST(sock, sock_i)
    FD_SET(sock->control, &fSet);
  deleteListIterator(sock_i);
}


/* Recover from a copyover - load players */
void copyover_recover() {     
  CHAR_DATA    *dMob;
  ACCOUNT_DATA *account;
  SOCKET_DATA  *dsock;
  FILE *fp;
  char acct[100];
  char name[100];
  char host[MAX_BUFFER];
  int desc;
      
  log_string("Copyover recovery initiated");

  if ((fp = fopen(COPYOVER_FILE, "r")) == NULL) {  
    log_string("Copyover file not found. Exitting.");
    exit (1);
  }
      
  /* In case something crashes - doesn't prevent reading */
  unlink(COPYOVER_FILE);

  for (;;) {  
    fscanf(fp, "%d %s %s %s\n", &desc, acct, name, host);
    if (desc == -1)
      break;

    // Many thanks to Rhaelar for the help in finding this bug; clear_socket
    // does not like receiving freshly malloc'd data. We have to make sure
    // everything is zeroed before we pass it to clear_socket
    //    dsock = malloc(sizeof(*dsock));
    dsock = calloc(1, sizeof(*dsock));
    clear_socket(dsock, desc);

    dsock->hostname = strdup(host);
    listPut(socket_list, dsock);
    propertyTablePut(sock_table, dsock);

    // load account data
    if((account = get_account(acct)) != NULL)
      socketSetAccount(dsock, account);
    // no luck!
    else {
      close_socket(dsock, FALSE);
      continue;
    }

    // load player data
    if ((dMob = get_player(name)) != NULL) {
      // attach to socket
      charSetSocket(dMob, dsock);
      socketSetChar(dsock, dMob);

      // try putting the character into the game
      // close the socket if we fail.
      if(!try_enter_game(dMob)) {
	// do not bother extracting, since we haven't entered the game yet
	unreference_player(socketGetChar(dsock));
	socketSetChar(dsock, NULL);
	close_socket(dsock, FALSE);
	continue;
      }
    }
    // no luck
    else {
      close_socket(dsock, FALSE);
      continue;
    }
   
    // Write something, and check if it goes error-free
    if (!text_to_socket(dsock, "\n\r <*>  And before you know it, everything has changed  <*>\n\r", 0)) { 
      close_socket(dsock, FALSE);
      continue;
    }
  
    // make sure the socket can be used
    dsock->bust_prompt    =  TRUE;
    dsock->lookup_status  =  TSTATE_DONE;

    // let our modules know we've finished copying over a socket
    hookRun("copyover_complete", hookBuildInfo("sk", dsock));

    #ifdef MODULE_MTH
      init_mth_socket(dsock);
      announce_support(dsock);
    #else
      // negotiate compression
      text_to_buffer(dsock, (char *) compress_will2);
      text_to_buffer(dsock, (char *) compress_will);
    #endif
  }
  fclose(fp);

  // now, set all of the sockets' control to the new fSet
  reconnect_copyover_sockets();
}     

void output_handler() {
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL; 

  ITERATE_LIST(sock, sock_i) {
    /* if the player quits or get's disconnected */
    if(sock->closed)
      continue;
    
    /* Send all new data to the socket and close it if any errors occour */
    if (!flush_output(sock))
      close_socket(sock, FALSE);
  } deleteListIterator(sock_i);
}

void input_handler() {
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL; 

  ITERATE_LIST(sock, sock_i) {
    // Close sockects we are unable to read from, or if we have no handler
    // to take in input
    if ((FD_ISSET(sock->control, &rFd) && !read_from_socket(sock)) ||
	listSize(sock->input_handlers) == 0) {
      close_socket(sock, FALSE);
      continue;
    }

    /* Ok, check for a new command */
    next_cmd_from_buffer(sock);
    
    // are we idling?
    if(!sock->cmd_read)
      sock->idle +=  1.0 / PULSES_PER_SECOND;
    /* Is there a new command pending ? */
    else if (sock->cmd_read) {
      sock->idle = 0.0;
      IH_PAIR *pair = listGet(sock->input_handlers, 0);
      if(pair->python == FALSE) {
	void (* handler)(SOCKET_DATA *, char *) = pair->handler;
	char *cmddup = strdup(bufferString(sock->next_command));
	handler(sock, cmddup);
	free(cmddup);
      }
      else {
	PyObject *arglist = Py_BuildValue("Os", socketGetPyFormBorrowed(sock),
					  bufferString(sock->next_command));
	PyObject *retval  = PyEval_CallObject(pair->handler, arglist);

	// check for an error:
	if(retval == NULL)
	  log_pyerr("Error with a Python input handler");
	
	// garbage collection
	Py_XDECREF(retval);
	Py_XDECREF(arglist);
      }

      // append our last command to the command history. History buffer is
      // 100 commands, so pop off the earliest command if we're going over
      listPut(sock->command_hist, strdup(bufferString(sock->next_command)));
      if(listSize(sock->command_hist) > 100)
	free(listRemoveNum(sock->command_hist, 100));
      bufferClear(sock->next_command);

      // we save whether or not we read a command until our next call to
      // input_handler(), at which time it is reset to FALSE if we didn't read
      // sock->cmd_read = FALSE;
    }

#ifdef MODULE_ALIAS
    // ACK!! this is so yucky, but I can't think of a better way to do it...
    // if this command was put in place by an alias, decrement the alias_queue
    // counter by one. This counter is in place mainly so aliases do not end
    // up calling eachother and making us get stuck in an infinite loop.
    if(sock->player) {
      int alias_queue = charGetAliasesQueued(sock->player);
      if(alias_queue > 0)
	charSetAliasesQueued(sock->player, --alias_queue);
    }
#endif
  } deleteListIterator(sock_i);
}


int count_pages(const char *string) {
  int num_newlines = count_letters(string, '\n', strlen(string));
  
  // if we just have one extra line, ignore the paging prompt and just send
  // the entire thing
  if(num_newlines <= NUM_LINES_PER_PAGE + 1)
    return 1;

  return ((num_newlines / NUM_LINES_PER_PAGE) + 
	  (num_newlines % NUM_LINES_PER_PAGE != 0));
}

void show_page(SOCKET_DATA *dsock, int page_num) {
  static char buf[MAX_BUFFER];
  char *page = dsock->page_string;
  *buf = '\0';
  int buf_i = 0, newline_count = 0, curr_page = 1;

  // skip to our page
  while(*page && curr_page < page_num) {
    if(*page == '\n')
      newline_count++;
    if(newline_count >= NUM_LINES_PER_PAGE) {
      curr_page++;
      newline_count = 0;
    }
    page++;
  }

  // copy our page to the buf
  while(*page && newline_count < NUM_LINES_PER_PAGE) {
    if(*page == '\n')
      newline_count++;
    buf[buf_i] = *page;
    page++;
    buf_i++;
  }

  buf[buf_i] = '\0';
  text_to_buffer(dsock, buf);
  if(dsock->tot_pages != 1)
    send_to_socket(dsock, "{c[{ypage %d of %d. Use MORE to view next page or BACK to view previous{c]{n\r\n",
		   page_num, dsock->tot_pages);
}


void delete_page(SOCKET_DATA *dsock) {
  if(dsock->page_string) free(dsock->page_string);
  dsock->page_string = NULL;
  dsock->tot_pages = 0;
  dsock->curr_page = 0;
}

void  page_string(SOCKET_DATA *dsock, const char *string) {
  if(dsock->page_string) 
    free(dsock->page_string);
  dsock->page_string = strdup(string);
  dsock->tot_pages   = count_pages(string);
  
  if(dsock->tot_pages == 1) {
    text_to_buffer(dsock, dsock->page_string);
    delete_page(dsock);
  }
  else {
    dsock->curr_page = 1;
    show_page(dsock, 1);
  }
}

void page_back(SOCKET_DATA *dsock) {
  dsock->curr_page--;
  if(dsock->tot_pages > 0 && dsock->curr_page <= dsock->tot_pages &&
     dsock->curr_page > 0)
    show_page(dsock, dsock->curr_page);
  else {
    dsock->curr_page = MIN(dsock->tot_pages, 1);
    text_to_buffer(dsock, "There is no more text in your page buffer.\r\n");
  }
  /*
  if(dsock->curr_page >= dsock->tot_pages)
    delete_page(dsock);
  */
}

void  page_continue(SOCKET_DATA *dsock) {
  dsock->curr_page++;
  if(dsock->tot_pages > 0 && dsock->curr_page > 0 && 
     dsock->curr_page <= dsock->tot_pages)
    show_page(dsock, dsock->curr_page);
  else {
    dsock->curr_page = dsock->tot_pages;
    text_to_buffer(dsock, "There is no more text in your page buffer.\r\n");
  }
}


//
// the command handler for the reader
void read_handler(SOCKET_DATA *sock, char *input) {
  if(!strncasecmp(input, "more", strlen(input)))
    page_continue(sock);
  else if(!strncasecmp(input, "back", strlen(input)))
    page_back(sock);
  else if(!strncasecmp(input, "quit", strlen(input)))
    socketPopInputHandler(sock);
  else
    text_to_buffer(sock, "Invalid choice!\r\n");
}


//
// the prompt for reading text
void read_prompt(SOCKET_DATA *sock) {
  text_to_buffer(sock, "\r\nQ to stop reading> ");
}

//
// a new handler that allows people to read long bits of text
void  start_reader(SOCKET_DATA *dsock, const char *text) {
  // add a new input handler to control the reading
  socketPushInputHandler(dsock, read_handler, read_prompt, "read text");
  
  // page the string
  page_string(dsock, text);
}


void do_copyover(void) {
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL;
  FILE *fp;
  char buf[100];
  char control_buf[20];
  char port_buf[20];

  if ((fp = fopen(COPYOVER_FILE, "w+")) == NULL)
    return;

  sprintf(buf, "\n\r <*>            The world starts spinning             <*>\n\r");

  // For each playing descriptor, save its character and account
  ITERATE_LIST(sock, sock_i) {
    #ifdef MODULE_MTH
      end_mccp2(sock);
      end_mccp3(sock);
    #else
      compressEnd(sock, sock->compressing, FALSE);
    #endif
    // kick off anyone who hasn't yet logged in a character
    if (!socketGetChar(sock) || !socketGetAccount(sock) || 
	!charGetRoom(socketGetChar(sock))) {
      text_to_socket(sock, "\r\nSorry, we are rebooting. Come back in a few minutes.\r\n", 0);
      close_socket(sock, FALSE);
    }
    // save account and player info to file
    else {
      fprintf(fp, "%d %s %s %s\n",
	      sock->control, accountGetName(sock->account), 
	      charGetName(sock->player), sock->hostname);
      // save the player
      save_player(sock->player);
      save_account(sock->account);
      text_to_socket(sock, buf, 0);
    }
  } deleteListIterator(sock_i);
  
  fprintf (fp, "-1\n");
  fclose (fp);

  // close any pending sockets
  recycle_sockets();

#ifdef MODULE_WEBSERVER
  // if we have a webserver set up, finalize that
  finalize_webserver();
#endif
  
  // exec - descriptors are inherited
  sprintf(control_buf, "%d", control);
  sprintf(port_buf, "%d", mudport);
  execl(EXE_FILE, "NakedMud", "-copyover", control_buf, port_buf, NULL);
}



//*****************************************************************************
// get and set functions
//*****************************************************************************
CHAR_DATA *socketGetChar     ( SOCKET_DATA *dsock) {
  return dsock->player;
}

void       socketSetChar     ( SOCKET_DATA *dsock, CHAR_DATA *ch) {
  dsock->player = ch;
}

ACCOUNT_DATA *socketGetAccount ( SOCKET_DATA *dsock) {
  return dsock->account;
}

void socketSetAccount (SOCKET_DATA *dsock, ACCOUNT_DATA *account) {
  dsock->account = account;
}

BUFFER *socketGetTextEditor   ( SOCKET_DATA *sock) {
  return sock->text_editor;
}

BUFFER *socketGetOutbound    ( SOCKET_DATA *sock) {
  return sock->outbuf;
}

void socketPushInputHandler  ( SOCKET_DATA *socket, 
			       void handler(SOCKET_DATA *socket, char *input),
			       void prompt (SOCKET_DATA *socket),
			       const char *state) {
  IH_PAIR *pair = malloc(sizeof(IH_PAIR));
  pair->handler = handler;
  pair->prompt  = prompt;
  pair->python  = FALSE;
  pair->state   = strdupsafe(state);
  listPush(socket->input_handlers, pair);
}

void socketPushPyInputHandler(SOCKET_DATA *sock, void *handler,void *prompt,
			      const char *state) {
  IH_PAIR *pair = malloc(sizeof(IH_PAIR));
  pair->handler = handler;
  pair->prompt  = prompt;
  pair->python  = TRUE;
  pair->state   = strdupsafe(state);
  Py_XINCREF((PyObject *)handler);
  Py_XINCREF((PyObject *)prompt);
  listPush(sock->input_handlers, pair);
}

void socketReplacePyInputHandler(SOCKET_DATA *sock, void *handler,void *prompt,
				 const char *state){
  socketPopInputHandler(sock);
  socketPushPyInputHandler(sock, handler, prompt, state);
}

const char *socketGetLastCmd(SOCKET_DATA *sock) {
  if(listSize(sock->command_hist) == 0)
    return "";
  else
    return listHead(sock->command_hist);
}

void socketPopInputHandler   ( SOCKET_DATA *socket) {
  IH_PAIR *pair = listPop(socket->input_handlers);
  if(pair != NULL)
    deleteInputHandler(pair);
}

void socketReplaceInputHandler( SOCKET_DATA *socket,
				void handler(SOCKET_DATA *socket, char *input),
				void prompt (SOCKET_DATA *socket),
				const char *state) {
  socketPopInputHandler(socket);
  socketPushInputHandler(socket, handler, prompt, state);
}

void socketQueueCommand( SOCKET_DATA *sock, const char *cmd) {
  listQueue(sock->input, strdup(cmd));
}

bool socketHasCommand(SOCKET_DATA *sock) {
  return sock->cmd_read;
}

int socketGetUID( SOCKET_DATA *dsock) {
  return dsock->uid;
}

bool socketHasPrompt(SOCKET_DATA *sock) {
  IH_PAIR *pair = listGet(sock->input_handlers, 0);
  return (pair != NULL && pair->prompt != NULL);
}

void socketShowPrompt( SOCKET_DATA *sock) {
  IH_PAIR *pair = listGet(sock->input_handlers, 0);
  if(pair == NULL || pair->prompt == NULL)
    return;
  else if(pair->python == FALSE) {
    ((void (*)(SOCKET_DATA *))pair->prompt)(sock);
  }
  else {
    PyObject *arglist = Py_BuildValue("(O)", socketGetPyFormBorrowed(sock));
    PyObject *retval  = PyEval_CallObject(pair->prompt, arglist);
    // check for an error:
    if(retval == NULL)
      log_pyerr("Error with a Python prompt");
    
    // garbage collection
    Py_XDECREF(retval);
    Py_XDECREF(arglist);
  }
}

void *socketGetAuxiliaryData  ( SOCKET_DATA *sock, const char *name) {
  return auxiliaryGet(sock->auxiliary, name);
}

const char *socketGetHostname(SOCKET_DATA *sock) {
  return sock->hostname;
}

int socketGetDNSLookupStatus(SOCKET_DATA *sock) {
  return sock->lookup_status;
}

void socketBustPrompt(SOCKET_DATA *sock) {
  sock->bust_prompt = TRUE;
}

const char *socketGetState(SOCKET_DATA *sock) {
  IH_PAIR *pair = listGet(sock->input_handlers, 0);
  if(pair == NULL) 
    return "";
  else
    return pair->state;
}

double socketGetIdleTime(SOCKET_DATA *sock) {
  return sock->idle;
}



//*****************************************************************************
// MCCP SUPPORT IS BELOW THIS LINE. NOTHING BUT MCCP SUPPORT SHOULD GO BELOW
// THIS LINE.
//*****************************************************************************
/*
 * mccp.c - support functions for the Mud Client Compression Protocol
 *
 * see http://www.randomly.org/projects/MCCP/
 *
 * Copyright (c) 1999, Oliver Jowett <oliver@randomly.org>
 *
 * This code may be freely distributed and used if this copyright
 * notice is retained intact.
 */
/*
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "mud.h"
#include "socket.h"
#include "utils.h"
*/

/* local functions */
bool  processCompressed       ( SOCKET_DATA *dsock );
#ifndef MODULE_MTH
const unsigned char enable_compress  [] = { IAC, SB, TELOPT_COMPRESS, WILL, SE, 0 };
const unsigned char enable_compress2 [] = { IAC, SB, TELOPT_COMPRESS2, IAC, SE, 0 };

/*
 * Memory management - zlib uses these hooks to allocate and free memory
 * it needs
 */
void *zlib_alloc(void *opaque, unsigned int items, unsigned int size)
{
  return calloc(items, size);
}

void zlib_free(void *opaque, void *address)
{
  free(address);
}

/*
 * Begin compressing data on `desc'
 */
bool compressStart(SOCKET_DATA *dsock, unsigned char teleopt)
{
  z_stream *s;

  /* already compressing */
  if (dsock->out_compress)
    return TRUE;

  /* allocate and init stream, buffer */
  s = (z_stream *) malloc(sizeof(*s));
  dsock->out_compress_buf = (unsigned char *) malloc(COMPRESS_BUF_SIZE);

  s->next_in    =  NULL;
  s->avail_in   =  0;
  s->next_out   =  dsock->out_compress_buf;
  s->avail_out  =  COMPRESS_BUF_SIZE;
  s->zalloc     =  zlib_alloc;
  s->zfree      =  zlib_free;
  s->opaque     =  NULL;

  if (deflateInit(s, 9) != Z_OK)
  {
    free(dsock->out_compress_buf);
    free(s);
    return FALSE;
  }

  /* version 1 or 2 support */
  if (teleopt == TELOPT_COMPRESS)
    text_to_socket(dsock, (char *) enable_compress, 0);
  else if (teleopt == TELOPT_COMPRESS2)
    text_to_socket(dsock, (char *) enable_compress2, 0);
  else
  {
    bug("Bad teleoption %d passed", teleopt);
    free(dsock->out_compress_buf);
    free(s);
    return FALSE;
  }

  /* now we're compressing */
  dsock->compressing = teleopt;
  dsock->out_compress = s;

  /* success */
  return TRUE;
}

/* Cleanly shut down compression on `desc' */
bool compressEnd(SOCKET_DATA *dsock, unsigned char teleopt, bool forced)
{
  unsigned char dummy[1];

  if (!dsock->out_compress)
    return TRUE;

  if (dsock->compressing != teleopt)
    return FALSE;

  dsock->out_compress->avail_in = 0;
  dsock->out_compress->next_in = dummy;

  /* No terminating signature is needed - receiver will get Z_STREAM_END */
  if (deflate(dsock->out_compress, Z_FINISH) != Z_STREAM_END && !forced)
    return FALSE;

  /* try to send any residual data */
  if (!processCompressed(dsock) && !forced)
    return FALSE;

  /* reset compression values */
  deflateEnd(dsock->out_compress);
  free(dsock->out_compress_buf);
  free(dsock->out_compress);
  dsock->compressing      = 0;
  dsock->out_compress     = NULL;
  dsock->out_compress_buf = NULL;

  /* success */
  return TRUE;
}

/* Try to send any pending compressed-but-not-sent data in `desc' */
bool processCompressed(SOCKET_DATA *dsock)
{
  int iStart, nBlock, nWrite, len;

  if (!dsock->out_compress)
    return TRUE;
    
  len = dsock->out_compress->next_out - dsock->out_compress_buf;
  if (len > 0)
  {
    for (iStart = 0; iStart < len; iStart += nWrite)
    {
      nBlock = UMIN (len - iStart, 4096);
      if ((nWrite = write(dsock->control, dsock->out_compress_buf + iStart, nBlock)) < 0)
      {
        if (errno == EAGAIN/* || errno == ENOSR*/)
          break;

        /* write error */
        return FALSE;
      }
      if (nWrite <= 0)
        break;
    }

    if (iStart)
    {
      if (iStart < len)
        memmove(dsock->out_compress_buf, dsock->out_compress_buf+iStart, len - iStart);

      dsock->out_compress->next_out = dsock->out_compress_buf + len - iStart;
    }
  }

  /* success */
  return TRUE;
}

//
// compress output
//
COMMAND(cmd_compress)
{
  /* no socket, no compression */
  if (!charGetSocket(ch))
    return;

  /* enable compression */
  if (!charGetSocket(ch)->out_compress) {
    text_to_char(ch, "Trying compression.\n\r");
    text_to_buffer(charGetSocket(ch), (char *) compress_will2);
    text_to_buffer(charGetSocket(ch), (char *) compress_will);
  }
  else /* disable compression */ {
    if (!compressEnd(charGetSocket(ch), charGetSocket(ch)->compressing, FALSE)){
      text_to_char(ch, "Failed.\n\r");
      return;
    }
    text_to_char(ch, "Compression disabled.\n\r");
  }
}
#endif
//*****************************************************************************
// IF YOU ARE PUTTING ANYTHING BELOW THIS LINE, YOU ARE PUTTING IT IN THE WRONG
// PLACE!! ALL SOCKET-RELATED STUFF SHOULD GO UP ABOVE THE MCCP SUPPORT STUFF!
//*****************************************************************************
