#ifndef __MTH_H
#define __MTH_H
//***************************************************************************
//* Mud Telopt Handler 1.5 by Igor van den Hoven 2009-2019 *
//***************************************************************************


//    telnet protocol


#define     IAC                 255
#define     DONT                254
#define     DO                  253
#define     WONT                252
#define     WILL                251
#define     SB                  250
#define     GA                  249   /* used for prompt marking */
#define     EL                  248
#define     EC                  247
#define     AYT                 246
#define     AO                  245
#define     IP                  244
#define     BREAK               243
#define     DM                  242
#define     NOP                 241   /* used for keep alive messages */
#define     SE                  240
#define     EOR                 239   /* used for prompt marking */
#define     ABORT               238
#define     SUSP                237
#define     xEOF                236

/*
	telnet options
*/

#define     TELOPT_ECHO           1   /* used to toggle local echo */
#define     TELOPT_SGA            3   /* used to toggle character mode */
#define     TELOPT_TTYPE         24   /* used to send client terminal type */
#define     TELOPT_EOR           25   /* used to toggle eor mode */
#define     TELOPT_NAWS          31   /* used to send client window size */
#define     TELOPT_NEW_ENVIRON   39   /* used to send information */
#define     TELOPT_CHARSET       42   /* used to detect UTF-8 support */
#define     TELOPT_MSDP          69   /* used to send mud server data */
#define     TELOPT_MSSP          70   /* used to send mud server information */
#define     TELOPT_MCCP2         86   /* used to toggle Mud Client Compression Protocol v2 */
#define     TELOPT_MCCP3         87   /* used to toggle Mud Client Compression Protocol v3 */
#define     TELOPT_MSP           90   /* used to toggle Mud Sound Protocol */
#define     TELOPT_MXP           91   /* used to toggle Mud Extention Protocol */
#define     TELOPT_GMCP         201

#define	    ENV_IS                0   /* option is... */
#define	    ENV_SEND              1   /* send option */
#define	    ENV_INFO              2   /* not sure */

#define	    ENV_VAR               0
#define	    ENV_VAL               1
#define	    ENV_ESC               2
#define     ENV_USR               3

#define     CHARSET_REQUEST       1
#define     CHARSET_ACCEPTED      2
#define     CHARSET_REJECTED      3

#define     MSSP_VAR              1
#define     MSSP_VAL              2

#define     MSDP_VAR              1
#define     MSDP_VAL              2
#define     MSDP_TABLE_OPEN       3
#define     MSDP_TABLE_CLOSE      4
#define     MSDP_ARRAY_OPEN       5
#define     MSDP_ARRAY_CLOSE      6

#define     TELCMD_OK(c)    ((unsigned char) (c) >= xEOF)
#define     TELCMD(c)       (telcmds[(unsigned char) (c) - xEOF])
#define     TELOPT(c)       (telnet_table[(unsigned char) (c)].name)

#define     ANNOUNCE_WILL       BV01
#define     ANNOUNCE_DO         BV02

#define COMM_FLAG_DISCONNECT    BV01
#define COMM_FLAG_PASSWORD      BV02
#define COMM_FLAG_REMOTEECHO    BV03
#define COMM_FLAG_EOR           BV04
#define COMM_FLAG_MSDPUPDATE    BV05
#define COMM_FLAG_256COLORS     BV06
#define COMM_FLAG_UTF8          BV07
#define COMM_FLAG_GMCP          BV08
#define COMM_FLAG_TRUECOLOR     BV09

#define MSDP_FLAG_COMMAND       BV01
#define MSDP_FLAG_LIST          BV02
#define MSDP_FLAG_SENDABLE      BV03
#define MSDP_FLAG_REPORTABLE    BV04
#define MSDP_FLAG_CONFIGURABLE  BV05
#define MSDP_FLAG_REPORTED      BV06
#define MSDP_FLAG_UPDATED       BV07

// As per the MTTS standard

#define MTTS_FLAG_ANSI          BV01
#define MTTS_FLAG_VT100         BV02
#define MTTS_FLAG_UTF8          BV03
#define MTTS_FLAG_256COLORS     BV04
#define MTTS_FLAG_MOUSETRACKING BV05
#define MTTS_FLAG_COLORPALETTE  BV06
#define MTTS_FLAG_SCREENREADER  BV07
#define MTTS_FLAG_PROXY         BV08
#define MTTS_FLAG_TRUECOLOR     BV09

#define RESTRING(var, val)            \
{                                     \
  free(var);                          \
  var = strdup(val);                  \
}

// telopt.c

//#define COMPRESS_BUF_SIZE  10000


// msdp.c

int         json2msdp                     ( unsigned char *src, int srclen, char *out );
void        process_msdp_varval           ( SOCKET_DATA *d, char *var, char *val );

// tables.c

#define ANNOUNCE_WILL   BV01
#define ANNOUNCE_DO     BV02

extern char *telcmds[];

struct telnet_type
{
        char      *name;
        int       flags;
};

extern struct telnet_type telnet_table[];

typedef void MSDP_FUN (SOCKET_DATA *d, int index);

struct msdp_type
{
        char     *name;
        int       flags;
        MSDP_FUN *fun;
};
/*
struct msdp_data
{
        char     *value;
        int      flags;
};
*/
extern struct msdp_type msdp_table[];

#endif
