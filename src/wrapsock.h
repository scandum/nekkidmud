/* wrapsock.h */
/* Functions wrapper headers */
/* To be used with tcpcli.c and tcpserv.c */
/* Author: Michelle Garoche */
/* Date: February 26th 2003 */
/* Feel free to do what you want with it */

#ifndef __wrapsock_h
#define __wrapsock_h

/* Includes */
/* fprintf, snprintf, strerror, fflush, fputs, sscanf,
fgets, ferror, printf, vsnprintf, fileno, stdout, stderr, FILE, EOF  */
#include <stdio.h>
/* exit, EXIT_SUCCESS, EXIT_FAILURE */
#include <stdlib.h>
/* accept, connect, bind, listen, socket, shutdown, SHUT_WR */
#include <sys/socket.h>
/* strcpy, bzero, strlen, strcat, strerror */
#include <string.h> 
/* getpid, getopt, fork, read, close, write, select */
#include <unistd.h> 
/* errno, EINTR */
#include <sys/errno.h>
/* htons, htonl, inet_pton */
//#include <arpa/inet.h> 
/* storage size of struct address, INADDR_ANY */
#include <netinet/in.h> 
/* va_list, va_start, va_end */
#include <stdarg.h> 
/* syslog, LOG_ERR */
#include <syslog.h> 
/* waitpid, WNOHANG */
#include <sys/wait.h> 
/* pow */
#include <math.h> 
/* isupper, isdigit, tolower */
#include <ctype.h> 
        
/* Not included, maybe should be on other systems */
/*#include <sys/types.h> in_addr_t, in_port_t, pid_t,
ssize_t, fd_set, FD_ZERO, FD_ISSET, FD_CLR normally included by stdio */
/*#include <signal.h> sigaction normally included by unistd */
/*#include <sys/syslimits.h> normally included by limits */
/*#include <limits.h> normally included by socket*/
/*#include <sys/time.h> timeval normally included by types */

/* Extern globals */
/* Used by geopt to check the options supplied by the user */
extern char *optarg; /* option argument */
extern int optind; /* index of the next argument on return */
extern int optopt; /* last known option on return */
extern int opterr; /* error on return */
extern int optreset; /* not used here */

/* Defines */
/* defined before the right type get a chance to be defined */
#ifndef socklen_t
#define socklen_t unsigned int
#endif
/* port to handle all requests */
#define SERV_PORT1 9877 
/* port to handle numeric requests */
#define SERV_PORT2 9878 
/* port to handle word conversion */
#define SERV_PORT3 9879 
/* length of a line */
#define MAXLINE 4096 
/* maximum number of pending connections */
#define LISTENQ 128 
/* convenience to make it easier to read the code */
#define SA struct sockaddr
/* returns the maximum of a and b */
#define MGMAX(a,b) (((a) > (b)) ? (a) : (b)) 

/* Typedefs */
/* Convenient definition to make it easier to read the code */
typedef void Sigfunc(int);

/* Functions protocols */

/* Used by client */
void cliusage(char *);
void Inet_pton(int, const char *, void *);
void Connect(int, const struct sockaddr *, socklen_t);
void str_cli(FILE *, int);

/* Used by server */
void servusage(char *);
void Bind(int, const struct sockaddr *, socklen_t);
void Listen(int, int);
Sigfunc *Signal(int, Sigfunc *);
pid_t Fork(void);
void Close(int);
void str_serv1(int);
void str_serv2(int);
void str_serv3(int);

/* Used by client and server */
int Socket(int, int, int);

/* Used to read a line */
char * Fgets(char *, int, FILE *);
ssize_t mg_readline(int, void *, size_t);
ssize_t Readline(int, void *, size_t);

/* Used to write a line */
void Fputs(const char *, FILE *);
ssize_t writen(int, const void *, size_t);
void Writen(int, void *, size_t);

/* Used to handle SIGCHLD */
Sigfunc * mg_signal(int, Sigfunc *);

/* Used to wait on requests */
int Select(int, fd_set *, fd_set *, fd_set *, struct timeval *);

/* Used to close the socket in  child process */
void Shutdown(int, int);

/* Used to handle errors */
void err_sys(const char *fmt, ...);
void err_quit(const char *fmt, ...);

#endif
/* end of wrapsock.h */
