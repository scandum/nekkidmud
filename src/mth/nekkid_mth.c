/***************************************************************************
 * NekkidMUD 1.6 by Igor van den Hoven and David Bloom           2019-2019 *
 ***************************************************************************/

#include "../mud.h"
#include "../socket.h"
#include "../event.h"
#include "../character.h"
#include "../editor/editor.h"
#include "mth.h"

void init_mth(void)
{
        mud = calloc(1, sizeof(MUD_DATA));

        mud->mccp_len  = COMPRESS_BUF_SIZE;
        mud->mccp_buf  = calloc(COMPRESS_BUF_SIZE, sizeof(unsigned char));
        mud->port      = mudport;
        mud->boot_time = time(NULL);

        init_msdp_table();

//        editorAddCommand(text_editor, "t", "        Update the user's MSDP buffer", editorMSDPUpdateBuffer);
}

void init_mth_socket(SOCKET_DATA *d)
{
        d->mth = calloc(1, sizeof(MTH_DATA));
        d->mth->proxy = (char *) strdup("");
        d->mth->terminal_type = (char *) strdup("");
}

void uninit_mth_socket(SOCKET_DATA *d)
{
        free(d->mth->terminal_type);
        free(d->mth->proxy);
        free(d->mth);
}

void editorMSDPUpdateBuffer(SOCKET_DATA *sock, char *arg, BUFFER *buf)
{
	if (!msdp_update_var_instant(sock, "EDIT_BUFFER", "%s", bufferString(buf)))
	{
		send_to_socket(sock, "msdp_update_var_instant: EDIT_BUFFER has not been reported.\r\n");
	}
}

void msdp_handler(void)
{
	SOCKET_DATA *dsock;
	LIST_ITERATOR *socket_i = newListIterator(socket_list);

	ITERATE_LIST(dsock, socket_i)
	{
		if (!socketGetChar(dsock) || !compares(socketGetState(dsock), "playing"))
		{
			continue;
		}

		CHAR_DATA *ch = socketGetChar(dsock);

		msdp_update_var(dsock, "POSITION", "%d", charGetPos(ch));

		if (HAS_BIT(dsock->mth->comm_flags, COMM_FLAG_MSDPUPDATE))
		{
			msdp_send_update(dsock);
		}
	}
	deleteListIterator(socket_i);
}
