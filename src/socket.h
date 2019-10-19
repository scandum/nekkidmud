#ifndef SOCKET_H
#define SOCKET_H
//*****************************************************************************
// socket.h
//
// all of the functions needed for working with character sockets
//*****************************************************************************

int   init_socket           ( void );
SOCKET_DATA  *new_socket    ( int sock );
void  close_socket          ( SOCKET_DATA *dsock, bool reconnect );
bool  read_from_socket      ( SOCKET_DATA *dsock );
void  input_handler         ( void );
void  output_handler        ( void );
void  copyover_recover      ( void );
void  do_copyover           ( void );

/* sends the output directly */
bool  text_to_socket        ( SOCKET_DATA *dsock, const char *txt, int length );
void  send_to_socket        ( SOCKET_DATA *dsock, const char *format, ...) __attribute__ ((format (printf, 2, 3)));

/* buffers the output        */
void  text_to_buffer        ( SOCKET_DATA *dsock, const char *txt );
void  next_cmd_from_buffer  ( SOCKET_DATA *dsock );
bool  flush_output          ( SOCKET_DATA *dsock );
void  handle_new_connections( SOCKET_DATA *dsock, char *arg );
void  clear_socket          ( SOCKET_DATA *sock_new, int sock );
void  recycle_sockets       ( void );
void *lookup_address        ( void *arg );



//*****************************************************************************
// set and get functions
//*****************************************************************************
int socketGetDNSLookupStatus( SOCKET_DATA *sock);

CHAR_DATA *socketGetChar      ( SOCKET_DATA *dsock);
void       socketSetChar      ( SOCKET_DATA *dsock, CHAR_DATA *ch);

ACCOUNT_DATA *socketGetAccount( SOCKET_DATA *dsock);
void          socketSetAccount( SOCKET_DATA *dsock, ACCOUNT_DATA *account);

void socketPushInputHandler   ( SOCKET_DATA *socket, 
			        void handler(SOCKET_DATA *socket, char *input),
				void prompt (SOCKET_DATA *socket),
				const char *state);
void socketReplaceInputHandler( SOCKET_DATA *socket,
				void handler(SOCKET_DATA *socket, char *input),
				void prompt (SOCKET_DATA *socket),
				const char *state);
void socketPushPyInputHandler   (SOCKET_DATA *sock, void *handler,void *prompt,
				 const char *state);
void socketReplacePyInputHandler(SOCKET_DATA *sock, void *handler,void *prompt,
				 const char *state);
void socketPopInputHandler    ( SOCKET_DATA *socket);
void *socketGetAuxiliaryData  ( SOCKET_DATA *sock, const char *name);
const char *socketGetHostname ( SOCKET_DATA *sock);
BUFFER *socketGetTextEditor   ( SOCKET_DATA *sock);
BUFFER *socketGetOutbound     ( SOCKET_DATA *sock);
void socketQueueCommand       ( SOCKET_DATA *sock, const char *cmd);
int               socketGetUID( SOCKET_DATA *sock);

bool socketHasPrompt          ( SOCKET_DATA *sock);
void socketBustPrompt         ( SOCKET_DATA *sock);
void socketShowPrompt         ( SOCKET_DATA *sock);
bool socketHasCommand         ( SOCKET_DATA *sock);
const char *socketGetLastCmd  ( SOCKET_DATA *sock);
const char *socketGetState    ( SOCKET_DATA *sock);
double socketGetIdleTime      ( SOCKET_DATA *sock);

#endif // SOCKET_H
