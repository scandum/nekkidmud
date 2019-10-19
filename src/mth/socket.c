/*
 * This file contains the socket code, used for accepting
 * new connections as well as reading and writing to
 * sockets, and closing down unused sockets.
 */

#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/ioctl.h>
#include <errno.h>

/* including main header file */
#include "mud.h"

/* global variables */
fd_set     fSet;                  /* the socket list for polling       */
STACK    * dsock_free = NULL;     /* the socket free list              */
LIST     * dsock_list = NULL;     /* the linked list of active sockets */
STACK    * dmobile_free = NULL;   /* the mobile free list              */
LIST     * dmobile_list = NULL;   /* the mobile list of active mobiles */

/* local procedures */
void GameLoop         ( int control );

/* intialize shutdown state */
bool shut_down = FALSE;
int  control;

/*
 * This is where it all starts, nothing special.
 */
int main(int argc, char **argv)
{
  bool fCopyOver;

  /* get the current time */
  current_time = time(NULL);

  /* allocate memory for socket and mobile lists'n'stacks */
  dsock_free = AllocStack();
  dsock_list = AllocList();
  dmobile_free = AllocStack();
  dmobile_list = AllocList();

  /* note that we are booting up */
  log_string("Program starting.");

  init_mth();

  /* initialize the event queue - part 1 */
  init_event_queue(1);

  if (argc > 2 && !strcmp(argv[argc-1], "copyover") && atoi(argv[argc-2]) > 0)
  {
    fCopyOver = TRUE;
    control = atoi(argv[argc-2]);
  }
  else fCopyOver = FALSE;

  /* initialize the socket */
  if (!fCopyOver)
    control = init_socket();

  /* load all external data */
  load_muddata(fCopyOver);

  /* initialize the event queue - part 2*/
  init_event_queue(2);

  /* main game loop */
  GameLoop(control);

  /* close down the socket */
  close(control);

  /* terminated without errors */
  log_string("Program terminated without errors.");

  /* and we are done */
  return 0;
}

void GameLoop(int control)   
{
  D_SOCKET *dsock;
  ITERATOR Iter;
  static struct timeval tv;
  struct timeval last_time, new_time;
  extern fd_set fSet;
  fd_set rFd;
  long secs, usecs;

  /* set this for the first loop */
  gettimeofday(&last_time, NULL);

  /* clear out the file socket set */
  FD_ZERO(&fSet);

  /* add control to the set */
  FD_SET(control, &fSet);

  /* copyover recovery */
  AttachIterator(&Iter, dsock_list);
  while ((dsock = (D_SOCKET *) NextInList(&Iter)) != NULL)
    FD_SET(dsock->control, &fSet);
  DetachIterator(&Iter);

  /* do this untill the program is shutdown */
  while (!shut_down)
  {
    /* set current_time */
    current_time = time(NULL);

    /* copy the socket set */
    memcpy(&rFd, &fSet, sizeof(fd_set));

    /* wait for something to happen */
    if (select(FD_SETSIZE, &rFd, NULL, NULL, &tv) < 0)
      continue;

    /* check for new connections */
    if (FD_ISSET(control, &rFd))
    {
      struct sockaddr_in sock;
      int socksize;
      int newConnection;

      socksize = sizeof(sock);
      if ((newConnection = accept(control, (struct sockaddr*) &sock, &socksize)) >=0)
        new_socket(newConnection);
    }

    /* poll sockets in the socket list */
    AttachIterator(&Iter ,dsock_list);
    while ((dsock = (D_SOCKET *) NextInList(&Iter)) != NULL)
    {
      /*
       * Close sockects we are unable to read from.
       */
      if (FD_ISSET(dsock->control, &rFd) && !read_from_socket(dsock))
      {
        close_socket(dsock, FALSE);
        continue;
      }

      /* Ok, check for a new command */
      next_cmd_from_buffer(dsock);

      /* Is there a new command pending ? */
      if (dsock->next_command[0] != '\0')
      {
        /* figure out how to deal with the incoming command */
        switch(dsock->state)
        {
          default:
            bug("Descriptor in bad state.");
            break;
          case STATE_NEW_NAME:
          case STATE_NEW_PASSWORD:
          case STATE_VERIFY_PASSWORD:
          case STATE_ASK_PASSWORD:
            handle_new_connections(dsock, dsock->next_command);
            break;
          case STATE_PLAYING:
            handle_cmd_input(dsock, dsock->next_command);
            break;
        }

        dsock->next_command[0] = '\0';
      }

      /* if the player quits or get's disconnected */
      if (dsock->state == STATE_CLOSED) continue;

      /* Send all new data to the socket and close it if any errors occour */
      if (!flush_output(dsock))
        close_socket(dsock, FALSE);
    }
    DetachIterator(&Iter);

    /* call the event queue */
    heartbeat();

    /*
     * Here we sleep out the rest of the pulse, thus forcing
     * SocketMud(tm) to run at PULSES_PER_SECOND pulses each second.
     */
    gettimeofday(&new_time, NULL);

    /* get the time right now, and calculate how long we should sleep */
    usecs = (int) (last_time.tv_usec -  new_time.tv_usec) + 1000000 / PULSES_PER_SECOND;
    secs  = (int) (last_time.tv_sec  -  new_time.tv_sec);

    /*
     * Now we make sure that 0 <= usecs < 1.000.000
     */
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

    /* if secs < 0 we don't sleep, since we have encountered a laghole */
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
  my_addr.sin_port = htons(MUDPORT);

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
bool new_socket(int sock)
{
  struct sockaddr_in   sock_addr;
  D_SOCKET           * sock_new;
  int                  argp = 1;
  socklen_t            size;

  /*
   * allocate some memory for a new socket if
   * there is no free socket in the free_list
   */
  if (StackSize(dsock_free) <= 0)
  {
    if ((sock_new = malloc(sizeof(*sock_new))) == NULL)
    {
      bug("New_socket: Cannot allocate memory for socket.");
      abort();
    }
  }
  else
  {
    sock_new = (D_SOCKET *) PopStack(dsock_free);
  }

  /* attach the new connection to the socket list */
  FD_SET(sock, &fSet);

  /* clear out the socket */
  clear_socket(sock_new, sock);

  /* set the socket as non-blocking */
  ioctl(sock, FIONBIO, &argp);

  /* update the linked list of sockets */
  AttachToList(sock_new, dsock_list);

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

    sock_new->lookup_status++;
  }

  init_mth_socket(sock_new);

  announce_support(sock_new);

  /* send the greeting */
  text_to_buffer(sock_new, greeting);
  text_to_buffer(sock_new, "What is your name? ");

  /* initialize socket events */
  init_events_socket(sock_new);

  /* everything went as it was supposed to */
  return TRUE;
}

/*
 * Close_socket()
 *
 * Will close one socket directly, freeing all
 * resources and making the socket availably on
 * the socket free_list.
 */
void close_socket(D_SOCKET *dsock, bool reconnect)
{
  EVENT_DATA *pEvent;
  ITERATOR Iter;

  if (dsock->lookup_status > TSTATE_DONE) return;
  dsock->lookup_status += 2;

  /* remove the socket from the polling list */
  FD_CLR(dsock->control, &fSet);

  if (dsock->state == STATE_PLAYING)
  {
    if (reconnect)
      text_to_socket(dsock, "This connection has been taken over.\n\r", 0);
    else if (dsock->player)
    {
      dsock->player->socket = NULL;
      log_string("Closing link to %s", dsock->player->name);
    }
  }
  else if (dsock->player)
    free_mobile(dsock->player);

  /* dequeue all events for this socket */
  AttachIterator(&Iter, dsock->events);
  while ((pEvent = (EVENT_DATA *) NextInList(&Iter)) != NULL)
    dequeue_event(pEvent);
  DetachIterator(&Iter);

  /* set the closed state */
  dsock->state = STATE_CLOSED;
}

/* 
 * Read_from_socket()
 *
 * Reads one line from the socket, storing it
 * in a buffer for later use. Will also close
 * the socket if it tries a buffer overflow.
 */

bool read_from_socket(D_SOCKET *dsock)
{
  unsigned char input[MAX_INPUT_LEN];
  int size;
  extern int errno;

  /* check for buffer overflows, and drop connection in that case */

  size = strlen(dsock->inbuf);

  if (size >= sizeof(dsock->inbuf) - 2)
  {
    text_to_socket(dsock, "\n\r!!!! Input Overflow !!!!\n\r", 0);
    return FALSE;
  }

  size = read(dsock->control, input, MAX_INPUT_LEN - 1);

  if (size <= 0)
  {
    return FALSE;
  }

  input[size] = 0;

  size = translate_telopts(dsock, input, size, (unsigned char *) dsock->inbuf, 0);

  return TRUE;
/*
  mth 1.5 size can be 0 on pure telnet data.

  if (size)
  {
    return TRUE;
  }
  return FALSE;
*/
}

/*
 * Text_to_socket()
 *
 * Sends text directly to the socket,
 * will compress the data if needed.
 */
bool text_to_socket(D_SOCKET *dsock, const char *txt, int length)
{
  if (length == 0)
  {
    length = strlen(txt);
  }

  if (length == 0)
  {
    log_string("sending empty string.");
    return TRUE;
  }

  /* write compressed */
  if (dsock->mth->mccp2)
  {
    memcpy(dsock->outbuf + dsock->top_output, txt, length);
    dsock->top_output += length;

    write_mccp2(dsock, dsock->outbuf, dsock->top_output);

    dsock->top_output = 0;

    return TRUE;
  }

  if (write(dsock->control, txt, length) < 0)
  {
    perror("Text_to_socket:");
    return FALSE;
  }
  return TRUE;
}

/*
 * Text_to_buffer()
 *
 * Stores outbound text in a buffer, where it will
 * stay untill it is flushed in the gameloop.
 *
 * Will also parse MTH color codes.
 */

void text_to_buffer(D_SOCKET *dsock, const char *txt)
{
  int color = 256;

  dsock->top_output += substitute_color((char *) txt, dsock->outbuf + dsock->top_output, color);

  dsock->top_output += sprintf(dsock->outbuf + dsock->top_output, "\e[0m");

  return;
}

/*
 * Text_to_mobile()
 *
 * If the mobile has a socket, then the data will
 * be send to text_to_buffer().
 */
void text_to_mobile(D_MOBILE *dMob, const char *txt)
{
  if (dMob->socket)
  {
    text_to_buffer(dMob->socket, txt);
    dMob->socket->bust_prompt = TRUE;
  }
}

void next_cmd_from_buffer(D_SOCKET *dsock)
{
  int size = 0, i = 0, j = 0;

  /* if theres already a command ready, we return */
  if (dsock->next_command[0] != '\0')
    return;

  /* if there is nothing pending, then return */
  if (dsock->inbuf[0] == '\0')
    return;

  /* check how long the next command is */
  while (dsock->inbuf[size] != '\0' && dsock->inbuf[size] != '\n')
    size++;

  /* we only deal with real commands */
  if (dsock->inbuf[size] == '\0')
    return;

  /* copy the next command into next_command */
  for ( ; i < size; i++)
  {
    if (isprint(dsock->inbuf[i]) && isascii(dsock->inbuf[i]))
    {
      dsock->next_command[j++] = dsock->inbuf[i];
    }
  }
  dsock->next_command[j] = '\0';

  /* skip forward to the next line */
  while (dsock->inbuf[size] == '\n')
  {
    dsock->bust_prompt = TRUE;   /* seems like a good place to check */
    size++;
  }

  /* use i as a static pointer */
  i = size;

  /* move the context of inbuf down */
  while (dsock->inbuf[size] != '\0')
  {
    dsock->inbuf[size - i] = dsock->inbuf[size];
    size++;
  }
  dsock->inbuf[size - i] = '\0';
}

bool flush_output(D_SOCKET *dsock)
{
  /* nothing to send */
  if (dsock->top_output <= 0 && !(dsock->bust_prompt && dsock->state == STATE_PLAYING))
    return TRUE;

  /* bust a prompt */
  if (dsock->state == STATE_PLAYING && dsock->bust_prompt)
  {
    text_to_buffer(dsock, "\n\rSocketMud:> ");
    dsock->bust_prompt = FALSE;
  }

  /* reset the top pointer */
  dsock->top_output = 0;

  /*
   * Send the buffer, and return FALSE
   * if the write fails.
   */
  if (!text_to_socket(dsock, dsock->outbuf, 0))
    return FALSE;

  /* Success */
  return TRUE;
}

void handle_new_connections(D_SOCKET *dsock, char *arg)
{
  D_MOBILE *p_new;
  int i;

  switch(dsock->state)
  {
    default:
      bug("Handle_new_connections: Bad state.");
      break;
    case STATE_NEW_NAME:
      if (dsock->lookup_status != TSTATE_DONE)
      {
        text_to_buffer(dsock, "Making a dns lookup, please have patience.\n\rWhat is your name? ");
        return;
      }
      if (!check_name(arg)) /* check for a legal name */
      {
        text_to_buffer(dsock, "Sorry, that's not a legal name, please pick another.\n\rWhat is your name? ");
        break;
      }
      arg[0] = toupper(arg[0]);
      log_string("%s is trying to connect.", arg);

      /* Check for a new Player */
      if ((p_new = load_profile(arg)) == NULL)
      {
        if (StackSize(dmobile_free) <= 0)
        {
          if ((p_new = malloc(sizeof(*p_new))) == NULL)
          {
            bug("Handle_new_connection: Cannot allocate memory.");
            abort();
          }
        }
        else
        {
          p_new = (D_MOBILE *) PopStack(dmobile_free);
        }
        clear_mobile(p_new);

        /* give the player it's name */
        p_new->name = strdup(arg);

        /* prepare for next step */
        send_echo_off(dsock);
        text_to_buffer(dsock, "Please enter a new password: ");
        dsock->state = STATE_NEW_PASSWORD;
      }
      else /* old player */
      {
        /* prepare for next step */
        send_echo_off(dsock);
        text_to_buffer(dsock, "What is your password? ");
        dsock->state = STATE_ASK_PASSWORD;
      }
/*    text_to_buffer(dsock, (char *) dont_echo); */

      /* socket <-> player */
      p_new->socket = dsock;
      dsock->player = p_new;
      break;
    case STATE_NEW_PASSWORD:
      if (strlen(arg) < 5 || strlen(arg) > 12)
      {
        text_to_buffer(dsock, "Between 5 and 12 chars please!\n\rPlease enter a new password: ");
        return;
      }

      free(dsock->player->password);
      dsock->player->password = strdup(crypt(arg, dsock->player->name));

      for (i = 0; dsock->player->password[i] != '\0'; i++)
      {
	if (dsock->player->password[i] == '~')
	{
	  text_to_buffer(dsock, "Illegal password!\n\rPlease enter a new password: ");
	  return;
	}
      }

      text_to_buffer(dsock, "Please verify the password: ");
      dsock->state = STATE_VERIFY_PASSWORD;
      break;
    case STATE_VERIFY_PASSWORD:
      if (!strcmp(crypt(arg, dsock->player->name), dsock->player->password))
      {
        send_echo_on(dsock);

        /* put him in the list */
        AttachToList(dsock->player, dmobile_list);
  
        log_string("New player: %s has entered the game.", dsock->player->name);

        /* and into the game */
        dsock->state = STATE_PLAYING;
        text_to_buffer(dsock, motd);

        /* initialize events on the player */
        init_events_player(dsock->player);

        /* strip the idle event from this socket */
        strip_event_socket(dsock, EVENT_SOCKET_IDLE);
      }
      else
      {
        free(dsock->player->password);
        dsock->player->password = NULL;
        text_to_buffer(dsock, "Password mismatch!\n\rPlease enter a new password: ");
        dsock->state = STATE_NEW_PASSWORD;
      }
      break;
    case STATE_ASK_PASSWORD:
/*      text_to_buffer(dsock, (char *) do_echo); */
      if (!strcmp(crypt(arg, dsock->player->name), dsock->player->password))
      {
        send_echo_on(dsock);

        if ((p_new = check_reconnect(dsock->player->name)) != NULL)
        {
          /* attach the new player */
          free_mobile(dsock->player);
          dsock->player = p_new;
          p_new->socket = dsock;

          log_string("%s has reconnected.", dsock->player->name);

          /* and let him enter the game */
          dsock->state = STATE_PLAYING;
          text_to_buffer(dsock, "You take over a body already in use.\n\r");

          /* strip the idle event from this socket */
          strip_event_socket(dsock, EVENT_SOCKET_IDLE);
        }
        else if ((p_new = load_player(dsock->player->name)) == NULL)
        {
          text_to_socket(dsock, "ERROR: Your pfile is missing!\n\r", 0);
          free_mobile(dsock->player);
          dsock->player = NULL;
          close_socket(dsock, FALSE);
          return;
        }
        else
        {
          /* attach the new player */
          free_mobile(dsock->player);
          dsock->player = p_new;
          p_new->socket = dsock;

          /* put him in the active list */
          AttachToList(p_new, dmobile_list);

          log_string("%s has entered the game.", dsock->player->name);

          /* and let him enter the game */
          dsock->state = STATE_PLAYING;
          text_to_buffer(dsock, motd);

	  /* initialize events on the player */
	  init_events_player(dsock->player);

	  /* strip the idle event from this socket */
	  strip_event_socket(dsock, EVENT_SOCKET_IDLE);
        }
      }
      else
      {
        text_to_socket(dsock, "Bad password!\n\r", 0);
        free_mobile(dsock->player);
        dsock->player = NULL;
        close_socket(dsock, FALSE);
      }
      break;
  }
}

void clear_socket(D_SOCKET *sock_new, int sock)
{
  memset(sock_new, 0, sizeof(*sock_new));

  sock_new->control        =  sock;
  sock_new->state          =  STATE_NEW_NAME;
  sock_new->lookup_status  =  TSTATE_LOOKUP;
  sock_new->player         =  NULL;
  sock_new->top_output     =  0;
  sock_new->events         =  AllocList();
//  sock_new->terminal_type  =  strdup("");
}


void recycle_sockets()
{
  D_SOCKET *dsock;
  ITERATOR Iter;

  AttachIterator(&Iter, dsock_list);
  while ((dsock = (D_SOCKET *) NextInList(&Iter)) != NULL)
  {
    if (dsock->lookup_status != TSTATE_CLOSED) continue;

    /* remove the socket from the socket list */
    DetachFromList(dsock, dsock_list);

    /* stop compression */
    end_mccp2( dsock );
    end_mccp3( dsock ); // mth 1.5

    /* close the socket */
    close(dsock->control);


    /* free the memory */
    free(dsock->hostname);
//    free(dsock->terminal_type);

    uninit_mth_socket(dsock);

    /* free the list of events */
    FreeList(dsock->events);

    /* put the socket in the free stack */
    PushStack(dsock, dsock_free);
  }
  DetachIterator(&Iter);
}
