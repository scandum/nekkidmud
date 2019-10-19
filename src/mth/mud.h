#ifndef MTH_MUD
#define MTH_MUD

#include <zlib.h>
#include <time.h>

/***************************************************************************
 * Mud Telopt Handler 1.5 by Igor van den Hoven                  2009-2019 *
 ***************************************************************************/

// Bitvectors

#define BV01            (1   <<  0)  
#define BV02            (1   <<  1) 
#define BV03            (1   <<  2) 
#define BV04            (1   <<  3)   
#define BV05            (1   <<  4)    
#define BV06            (1   <<  5)
#define BV07            (1   <<  6)
#define BV08            (1   <<  7)
#define BV09            (1   <<  8)
#define BV10            (1   <<  9)

#define HAS_BIT(var, bit)       ((var)  & (bit))
#define SET_BIT(var, bit)       ((var) |= (bit))
#define DEL_BIT(var, bit)       ((var) &= (~(bit)))
#define TOG_BIT(var, bit)       ((var) ^= (bit))

MUD_DATA *mud;

//	Mud data, structure containing global variables.

struct mud_data
{
	int                   boot_time;
	int                   port;
	int                   total_plr;
	int                   msdp_table_size;
	int                   mccp_len;
	unsigned char       * mccp_buf;
};

/*
	telopt.c
*/

void        announce_support         ( SOCKET_DATA *d );
int         translate_telopts        ( SOCKET_DATA *d, unsigned char *src, int srclen, unsigned char *out, int outlen );
int         write_mccp2              ( SOCKET_DATA *d, char *txt, int length );
void        end_mccp2                ( SOCKET_DATA *d );
void        end_mccp3                ( SOCKET_DATA *d );
void        send_echo_on             ( SOCKET_DATA *d );
void        send_echo_off            ( SOCKET_DATA *d );


/*
	mth.c
*/

void        init_mth(void);
void        init_mth_socket( SOCKET_DATA *d );
void        uninit_mth_socket( SOCKET_DATA *d );
int         total_players           ( void );
void        log_descriptor_printf ( SOCKET_DATA *d, char *fmt, ...);
int         cat_sprintf              ( char *dest, char *fmt, ... );
void        arachnos_devel           ( char *fmt, ... );
void        arachnos_mudlist         ( char *fmt, ... );

/*
	msdp.c
*/

void        init_msdp_table               ( void );
void        msdp_update_var               ( SOCKET_DATA *d, char *var, char *fmt, ... );
void        msdp_update_var_instant       ( SOCKET_DATA *d, char *var, char *fmt, ... );

#endif
