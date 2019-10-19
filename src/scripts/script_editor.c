//*****************************************************************************
//
// script_editor.c
//
// this is an extention of the basic script editor (/src/editor/editor.h) to
// be tailored for script editing. Provides syntax coloring and line number
// display, and auto indenting.
//
//*****************************************************************************

#include "../mud.h"
#include "../socket.h"
#include "../auxiliary.h"

#include "../editor/editor.h"
#include "script_editor.h"
#include "scripts.h"



//*****************************************************************************
// auxiliary data and functions
//*****************************************************************************
typedef struct script_editor_aux_data {
  int      indent; // how far are we indented for our next line of code?
  bool autoindent; // do we autoindent after encountering a colon?
} SCRIPT_EDITOR_AUX_DATA;

SCRIPT_EDITOR_AUX_DATA *
newScriptEditorAuxData() {
  SCRIPT_EDITOR_AUX_DATA *data = malloc(sizeof(SCRIPT_EDITOR_AUX_DATA));
  data->autoindent = TRUE;
  data->indent     = 0;
  return data;
}



//*****************************************************************************
// local functions and data structures
//*****************************************************************************
EDITOR *script_editor = NULL;


//
// toggle the socket's autoindent in script editning
void scriptEditorToggleAutoindent(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  SCRIPT_EDITOR_AUX_DATA *data = 
    socketGetAuxiliaryData(sock, "script_editor_aux_data");
  data->autoindent = (data->autoindent + 1) % 2;
  send_to_socket(sock, "Autoindent %s.\r\n", 
		 (data->autoindent ? "enabled" : "supressed"));
}

//
// increase the indentation in our script by 2
void scriptEditorIndent(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  SCRIPT_EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "script_editor_aux_data");
  data->indent += 2;
  send_to_socket(sock, "Editor indented to %d spaces.\r\n", data->indent);
}

//
// decrease the indentation in our script by 2
void scriptEditorUndent(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  SCRIPT_EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "script_editor_aux_data");
  data->indent -= 2;
  if(data->indent < 0)
    data->indent = 0;
  send_to_socket(sock, "Editor undented to %d spaces.\r\n", data->indent);
}

//
// return what our current level of indenting is
int socketGetScriptEditorIndent(SOCKET_DATA *sock) {
  SCRIPT_EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "script_editor_aux_data");
  return data->indent;
}

//
// append new text to the script editor. Make sure we add the appropriate amount
// of spaces before the addition, and increment/decrement our indent as needed
void scriptEditorAppend(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  SCRIPT_EDITOR_AUX_DATA *data = 
    socketGetAuxiliaryData(sock, "script_editor_aux_data");

  // if we're playing with else/elif/case, take down the input a notch
  if(data->autoindent) {
    if(!strncmp(arg, "except:", 7) ||
       !strncmp(arg, "except ", 7) ||
       !strncmp(arg, "else:",   5) ||
       !strncmp(arg, "elif ",   5) ||
       !strncmp(arg, "case ",   5))
      scriptEditorUndent(sock, NULL, NULL);

    // add in our indents if neccessary
    if(data->indent > 0) {
      char fmt[20];
      sprintf(fmt, "%%%ds", data->indent);
      bprintf(buf, fmt, " ");
    }
  }

  // cat the new line of code
  bufferCat(buf, arg);
  bufferCat(buf, "\n");

  // see if we need to change our indent
  if(data->autoindent) {
    int len = strlen(arg);
    if(len > 0 && arg[len-1] == ':')
      scriptEditorIndent(sock, NULL, NULL);
  }
}

//
// list the script buffer to the socket, but do all of our syntax highlighting
void scriptEditorList(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  script_display(sock, bufferString(buf), TRUE);
}

//
// \r really screws up python; this function makes sure they're all stripped out
void scriptEditorFormat(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  bufferReplace(buf, "\r", "", TRUE);
  text_to_buffer(sock, "Script formatted.\r\n");
}

//
// prepare our socket to use the script editor
void socketInitScriptEditor(SOCKET_DATA *sock) {
  SCRIPT_EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "script_editor_aux_data");
  data->indent = 0;
}

//
// bufferInsert appends a carriage return to the end of the argument, but this
// makes Python choke. Soooo... we have to do the append, and then remove the
// carriage return that was inserted.
void scriptEditorInsert(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  char tmp[SMALL_BUFFER];
  arg = one_arg(arg, tmp);
  int line = atoi(tmp);
  if(!isdigit(*tmp) || !bufferInsert(buf, arg, line))
    text_to_buffer(sock, "Insertion failed.\r\n");
  else {
    text_to_buffer(sock, "Line inserted.\r\n");
    bufferReplace(buf, "\r", "", TRUE);    
  }
}

//
// same deal as scriptEditorInsert
void scriptEditorEditLine(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  char tmp[SMALL_BUFFER];
  arg = one_arg(arg, tmp);
  int line = atoi(tmp);
  if(!isdigit(*tmp) || !bufferReplaceLine(buf, arg, line))
    text_to_buffer(sock, "Line does not exist.\r\n");
  else {
    text_to_buffer(sock, "Line replaced.\r\n");
    bufferReplace(buf, "\r", "", TRUE);
  }
}



//*****************************************************************************
// implementation of script_editor.h
//*****************************************************************************
void init_script_editor() {
  script_editor = newEditor();
  editorSetAppend(script_editor, scriptEditorAppend);
  editorSetInit(script_editor, socketInitScriptEditor);
  editorAddCommand(script_editor, "v", "        Indent down the script editor",
		   scriptEditorUndent);
  editorAddCommand(script_editor, "^", "        Indent up the script editor",
		   scriptEditorIndent);
  editorAddCommand(script_editor, "-", "        Toggle auto-indenting",
		   scriptEditorToggleAutoindent);
  editorAddCommand(script_editor, "l", "        List the current buffer contents",
		   scriptEditorList);
  editorAddCommand(script_editor, "f", "        Strips all bad characters out of the script",
		   scriptEditorFormat);
  editorAddCommand(script_editor, "i", "# <txt> Insert new text at the specified line number",
		   scriptEditorInsert);
  editorAddCommand(script_editor, "e", "# <txt> Sets the text at the specified line to the new text", 
		   scriptEditorEditLine);

  auxiliariesInstall("script_editor_aux_data", 
		     newAuxiliaryFuncs(AUXILIARY_TYPE_SOCKET,
				       newScriptEditorAuxData, free,
				       NULL, NULL, NULL, NULL));
}
