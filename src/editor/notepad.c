//*****************************************************************************
//
// notepad.c
//
// Here is the proof of concept for the editor utility. This is a basic text
// editor that allows players to write/read information contained within a
// notepad.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../inform.h"
#include "../auxiliary.h"
#include "../socket.h"
#include "../character.h"
#include "editor.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pychar.h"
#include "../scripts/script_editor.h"



//*****************************************************************************
// auxiliary data for sockets
//*****************************************************************************
typedef struct notepad_data {
  BUFFER *notepad;
} NOTEPAD_DATA;

NOTEPAD_DATA *newNotepadData() {
  NOTEPAD_DATA *data = malloc(sizeof(NOTEPAD_DATA));
  data->notepad = newBuffer(1);
  return data;
}

void deleteNotepadData(NOTEPAD_DATA *data) {
  if(data->notepad) deleteBuffer(data->notepad);
  free(data);
}



//*****************************************************************************
// local datastructures, functions, and commands for players
//*****************************************************************************
void socketStartNotepad(SOCKET_DATA *sock, EDITOR *editor) {
  NOTEPAD_DATA *data = socketGetAuxiliaryData(sock, "notepad_data");
  socketStartEditor(sock, editor, data->notepad);
}

COMMAND(cmd_write) {
  if(!charGetSocket(ch))
    text_to_char(ch, "You need an attached socket for that!\r\n");
  else {
    message(ch, NULL, NULL, NULL, TRUE, TO_ROOM, 
	    "$n pulls out a pen and begins jotting notes down.");
    if(!strcasecmp(arg, "script"))
      socketStartNotepad(charGetSocket(ch), script_editor);
    else
      socketStartNotepad(charGetSocket(ch), text_editor);
  }
}

COMMAND(cmd_notepad) {
  if(!charGetSocket(ch))
    text_to_char(ch, "You need an attached socket for that!\r\n");
  else {
    SOCKET_DATA *sock = charGetSocket(ch);
    NOTEPAD_DATA *data = socketGetAuxiliaryData(sock, "notepad_data");
    if(!*bufferString(data->notepad))
      text_to_char(ch, "Your notepad is empty.\r\n");
    else
      text_to_char(ch, bufferString(data->notepad));
  }
}

BUFFER *socketGetNotepad(SOCKET_DATA *sock) {
  NOTEPAD_DATA *data = socketGetAuxiliaryData(sock, "notepad_data");
  return data->notepad;
}

void socketSetNotepad(SOCKET_DATA *sock, const char *txt) {
  NOTEPAD_DATA *data = socketGetAuxiliaryData(sock, "notepad_data");
  bufferClear(data->notepad);
  bufferCat(data->notepad, txt);
}

PyObject *PyChar_getnotepad(PyObject *self, void *closure) {
  CHAR_DATA *ch = PyChar_AsChar(self);
  if(ch != NULL && charGetSocket(ch) != NULL) 
    return Py_BuildValue("s", 
			 (charGetSocket(ch) ? 
			  bufferString(socketGetNotepad(charGetSocket(ch))) :
			  ""));
  else           
    return NULL;
}



//*****************************************************************************
// setting everything up...
//*****************************************************************************
void init_notepad() {
  // install the editor components
  auxiliariesInstall("notepad_data", 
		     newAuxiliaryFuncs(AUXILIARY_TYPE_SOCKET,
				       newNotepadData, deleteNotepadData,
				       NULL, NULL, NULL, NULL));

  // install our commands
  add_cmd("write",   NULL, cmd_write,   "player", TRUE);
  add_cmd("notepad", NULL, cmd_notepad, "player", TRUE);

  // add our python extensions
  PyChar_addGetSetter("notepad", PyChar_getnotepad, NULL,
		      "Returns the character's notepad, if any");
}
