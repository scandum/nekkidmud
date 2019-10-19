//*****************************************************************************
//
// editor.c
//
// one of the problems that came up with the origional text editor what that
// it started becoming rather kludgy and hackish as we started adding more
// features to it, like script editing capabilities. It was difficult to
// coordinate when we wanted certain options and when we didn't. This is an
// attempt to make things a bit easier to scale up with.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../auxiliary.h"
#include "../socket.h"
#include "editor.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../scripts/scripts.h"
#include "../scripts/pyplugs.h"



//*****************************************************************************
// auxiliary data for sockets
//*****************************************************************************
typedef struct editor_aux_data {
  EDITOR   *editor;      // the editor we're using
  BUFFER   *buf;         // the buffer we're editing (if any)
  BUFFER   *working_buf; // the buffer where we do our work
  PyObject *py_complete; // python function to call on completion of editing
  void    (* on_complete)(SOCKET_DATA *sock, const char *str);
} EDITOR_AUX_DATA;

EDITOR_AUX_DATA *newEditorAuxData() {
  EDITOR_AUX_DATA *data = malloc(sizeof(EDITOR_AUX_DATA));
  bzero(data, sizeof(EDITOR_AUX_DATA));
  return data; // the buffer, working buffer, and editor are null 'til needed
}

void deleteEditorAuxData(EDITOR_AUX_DATA *data) {
  // if we have a working buf, free it. Don't touch anything else. that
  // stuff will be needed by the rest of the program
  if(data->working_buf) deleteBuffer(data->working_buf);
  if(data->py_complete) { Py_DECREF(data->py_complete); }
  free(data);
}

void clearEditorAuxData(EDITOR_AUX_DATA *data) {
  if(data->working_buf) deleteBuffer(data->working_buf);
  if(data->py_complete) { Py_DECREF(data->py_complete); }
  data->working_buf = NULL;
  data->editor      = NULL;
  data->buf         = NULL;
  data->on_complete = NULL;
  data->py_complete = NULL;
}



//*****************************************************************************
// local structures, functions, and defines
//*****************************************************************************
// a basic text editor for use by other modules
EDITOR *text_editor = NULL;

// an editor for dialog. Essentially, text without newlines
EDITOR *dialog_editor = NULL;

struct editor_data {
  HASHTABLE *cmds;  // mappings from commands to their functions and descs
  void (* prompt)(SOCKET_DATA *sock); // the prompt display
  void (*   init)(SOCKET_DATA *sock); // reset variables for using the editor
  void (* append)(SOCKET_DATA *sock, char *arg, BUFFER *buf); // append text to the buffer
};

// data for a command in the editor
typedef struct command_data {
  // the function this command calls
  void (* func)(SOCKET_DATA *sock, char *arg, BUFFER *buf);
  char *desc;    // the one-line helpfile description
  bool reserved; // is this command protected from being written over?
} ECMD_DATA;

ECMD_DATA *
newEditorCommand(const char *desc, 
		 void func(SOCKET_DATA *sock, char *arg, BUFFER *buf),
		 bool reserved) {
  ECMD_DATA *cmd = malloc(sizeof(ECMD_DATA));
  cmd->desc     = strdup(desc ? desc : "");
  cmd->func     = func;
  cmd->reserved = reserved;
  return cmd;
}

void deleteEditorCommand(ECMD_DATA *cmd) {
  if(cmd->desc) free(cmd->desc);
  free(cmd);
}


//
// the default prompt display for editors
//
void editorDefaultPrompt(SOCKET_DATA *sock) {
  text_to_buffer(sock, "] ");
}


//
// the default header display for editors
//
void editorDefaultHeader(SOCKET_DATA *sock) {
  text_to_buffer(sock, 
"================================================================================\r\n"
"Begin editing. /q on a new line to quit, /a to abort. /h for help         \r\n"
"================================================================================\r\n"
		 );
}


//
// the default function for appending text to the working buffer
//
void editorDefaultAppend(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  bufferCat(buf, arg);
  bufferCat(buf, "\r\n");
}


//
// the function for appending text to a dialog buffer. 
//
void editorDialogAppend(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  // if the buffer isn't empty, cat a space
  if(bufferLength(buf) > 0)
    bufferCat(buf, " ");
  bufferCat(buf, arg);
}


//
// The function that takes in a new command and figures out what to
// do with it
//
void editorInputHandler(SOCKET_DATA *sock, char *arg) {
  EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "editor_aux_data");

  // is it a command?
  if(*arg == '/') {
    // separate the command and the argument
    char buf[SMALL_BUFFER];
    arg = one_arg(arg, buf);

    // pull up the command
    ECMD_DATA *cmd = hashGet(data->editor->cmds, buf+1);
    if(cmd == NULL)
      text_to_buffer(sock, "Invalid command.\r\n");
    else
      cmd->func(sock, arg, data->working_buf);
  }
  else
    data->editor->append(sock, arg, data->working_buf);
}


//*****************************************************************************
// implementation of basic editor commands
//*****************************************************************************
void editorQuit(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  // save the current changes
  EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "editor_aux_data");
  if(data->on_complete)
    data->on_complete(sock, bufferString(buf));
  else if(data->py_complete) {
    PyObject *ret = PyObject_CallFunction(data->py_complete, "Os", 
					  socketGetPyFormBorrowed(sock),
					  bufferString(buf));
    if(ret == NULL)
      log_pyerr("Error quitting the buffer editor.");
    Py_XDECREF(ret);
  }

  //bufferCopyTo(buf, data->buf);
  clearEditorAuxData(data);
  text_to_buffer(sock, "Saved and quit.\r\n");
  // and then pop the input handler
  socketPopInputHandler(sock);
}

void editorAbort(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "editor_aux_data");
  clearEditorAuxData(data);
  text_to_buffer(sock, "Editor aborted.\r\n");
  socketPopInputHandler(sock);
}

void editorDisplayHelp(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  const char *key = NULL;
  ECMD_DATA   *val = NULL;
  HASH_ITERATOR *hash_i = newHashIterator(socketGetEditor(sock)->cmds);

  // print out all of the commands and their descriptions
  ITERATE_HASH(key, val, hash_i)
    send_to_socket(sock, "/%-3s %s\r\n", key, val->desc);
  deleteHashIterator(hash_i);
}

void editorDeleteLine(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  char tmp[SMALL_BUFFER];
  arg = one_arg(arg, tmp);
  int line = atoi(tmp);
  if(!isdigit(*tmp) || !bufferRemove(buf, line))
    text_to_buffer(sock, "Line does not exist.\r\n");
  else
    text_to_buffer(sock, "Line deleted.\r\n");
}

void editorEditLine(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  char tmp[SMALL_BUFFER];
  arg = one_arg(arg, tmp);
  int line = atoi(tmp);
  if(!isdigit(*tmp) || !bufferReplaceLine(buf, arg, line))
    text_to_buffer(sock, "Line does not exist.\r\n");
  else
    text_to_buffer(sock, "Line replaced.\r\n");
}

void editorInsertLine(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  char tmp[SMALL_BUFFER];
  arg = one_arg(arg, tmp);
  int line = atoi(tmp);
  if(!isdigit(*tmp) || !bufferInsert(buf, arg, line))
    text_to_buffer(sock, "Insertion failed.\r\n");
  else
    text_to_buffer(sock, "Line inserted.\r\n");
}

void editorListDialogBuffer(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  if(*bufferString(buf))
    send_to_socket(sock, "%s\r\n", bufferString(buf));
}

void editorListBuffer(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  if(*bufferString(buf))
    text_to_buffer(sock, bufferString(buf));
}

void editorReplace(SOCKET_DATA *sock, char *arg, BUFFER *buf, bool all) {
  char *a;
  char *b;

  if(count_letters(arg, '\'', strlen(arg)) != 4) {
    text_to_buffer(sock, "arguments must take the form: 'to replace' 'replacement'\r\n");
    return;
  }

  // pull the first argument
  a = strtok(arg, "\'");
  if(a == NULL) {
    text_to_buffer(sock, "format is: /r[a] 'to replace' 'replacement'\r\n");
    return;
  }

  // kill the leading the leading ' of b
  strtok(NULL, "\'");

  b = strtok(NULL, "\'");
  if(b == NULL)
    b = "\0"; // "\0" will get deleted at the end of this block

  int replaced = bufferReplace(buf, a, b, all);
  send_to_socket(sock, "%d occurence%s of '%s' replaced with '%s'.\r\n",
		 replaced, (replaced == 1 ? "" : "s"), a, b);
}

void editorReplaceString(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  editorReplace(sock, arg, buf, FALSE);
}

void editorReplaceAllString(SOCKET_DATA *sock, char *arg, BUFFER *buf) { 
  editorReplace(sock, arg, buf, TRUE);
}

void editorClear(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  bufferClear(buf);
  text_to_buffer(sock, "Buffer cleared.\r\n");
}

void editorFormatBuffer(SOCKET_DATA *sock, char *arg, BUFFER *buf) {
  bufferFormat(buf, SCREEN_WIDTH, PARA_INDENT);
  text_to_buffer(sock, "Buffer formatted.\r\n");
}



//*****************************************************************************
// implementation of editor.h
//*****************************************************************************
void init_editor() {
  // install the editor components
  auxiliariesInstall("editor_aux_data", 
		     newAuxiliaryFuncs(AUXILIARY_TYPE_SOCKET,
				       newEditorAuxData, deleteEditorAuxData,
				       NULL, NULL, NULL, NULL));
  text_editor   = newEditor();
  dialog_editor = newEditor();
  editorSetAppend(dialog_editor, editorDialogAppend);
  editorRemoveCommand(dialog_editor, "f");
  editorAddCommand(dialog_editor, "l", 
		   "        List the current buffer contents",
		   editorListDialogBuffer);
}

EDITOR *newEditor() {
  EDITOR *editor = malloc(sizeof(EDITOR));
  // set up the default commands
  editor->cmds = newHashtable();
  hashPut(editor->cmds, "q", 
	  newEditorCommand("        Quit editor and save changes",
			   editorQuit, TRUE));
  hashPut(editor->cmds, "a", 
	  newEditorCommand("        Quit editor and don't save",
			   editorAbort, TRUE));
  hashPut(editor->cmds, "h", 
	  newEditorCommand("        Display editor commands",
			   editorDisplayHelp, TRUE));
  hashPut(editor->cmds, "c", 
	  newEditorCommand("        Clear the contents of the buffer",
			   editorClear, TRUE));
  hashPut(editor->cmds, "l",
	  newEditorCommand("        List the current buffer contents",
			   editorListBuffer, FALSE));
  hashPut(editor->cmds, "d", 
	  newEditorCommand("#       Delete line with the specified number", 
			   editorDeleteLine, FALSE));
  hashPut(editor->cmds, "e", 
	  newEditorCommand("# <txt> Sets the text at the specified line to the new text",
			   editorEditLine, FALSE));
  hashPut(editor->cmds, "i", 
	  newEditorCommand("# <txt> Insert new text at the specified line number",
			   editorInsertLine, FALSE));
  hashPut(editor->cmds, "f",
	  newEditorCommand("        Formats your text into a paragraph",
			   editorFormatBuffer, FALSE));
  hashPut(editor->cmds, "r",
	  newEditorCommand("'a' 'b' replace first occurence of 'a' with 'b'",
			   editorReplaceString, FALSE));
  hashPut(editor->cmds, "ra",
	  newEditorCommand("'a' 'b' repalce all occurences of 'a' with 'b'",
			   editorReplaceAllString, FALSE));

  // set up the default prompt, header, and appending function
  editor->prompt = editorDefaultPrompt;
  editor->append = editorDefaultAppend;
  editor->init   = NULL;
  return editor;
}

void editorSetPrompt(EDITOR *editor, void prompt(SOCKET_DATA *sock)) {
  editor->prompt = prompt;
}

void editorSetInit(EDITOR *editor, void init(SOCKET_DATA *sock)) {
  editor->init = init;
}

void editorSetAppend(EDITOR *editor,
		     void append(SOCKET_DATA *sock, char *arg, BUFFER *buf)) {
  editor->append = append;
}

void editorAddCommand(EDITOR *editor, const char *cmd, const char *desc, 
		      void func(SOCKET_DATA *sock, char *arg, BUFFER *buf)) {
  ECMD_DATA *old_cmd = hashGet(editor->cmds, cmd);
  // make sure we're not trying to replace a reserved command
  if(!old_cmd || !old_cmd->reserved) {
    hashPut(editor->cmds, cmd, newEditorCommand(desc, func, FALSE));
    if(old_cmd) deleteEditorCommand(old_cmd);
  }
}

void editorRemoveCommand(EDITOR *editor, const char *cmd) {
  ECMD_DATA *old_cmd = hashGet(editor->cmds, cmd);
  // make sure the command isn't reserved
  if(old_cmd && !old_cmd->reserved) {
    hashRemove(editor->cmds, cmd);
    deleteEditorCommand(old_cmd);
  }
}

void dflt_editor_complete(SOCKET_DATA *sock, const char *str) {
  EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "editor_aux_data"); 
  bufferClear(data->buf);
  bufferCat(data->buf, str);
}

void begin_editor(SOCKET_DATA *sock) {
  EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "editor_aux_data"); 
  if(data->editor->init) data->editor->init(sock);

  editorDefaultHeader(sock);
  
  // if we have a "list" command, execute it. Otherwise, cat the buf
  ECMD_DATA *list = NULL;
  if((list = hashGet(data->editor->cmds, "l")) != NULL)
    list->func(sock, "", data->working_buf);
  else
    text_to_buffer(sock, bufferString(data->working_buf));

  socketPushInputHandler(sock, editorInputHandler, data->editor->prompt, 
			 "text editor");
}

void socketStartPyEditorFunc(SOCKET_DATA *sock, EDITOR *editor,const char *dflt,
			     void *py_complete) {
  EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "editor_aux_data"); 
  data->working_buf = newBuffer(1);
  bufferCat(data->working_buf, dflt);
  data->editor      = editor;
  data->py_complete = py_complete;
  Py_XINCREF(data->py_complete);
  begin_editor(sock);
}

void socketStartEditorFunc(SOCKET_DATA *sock, EDITOR *editor, const char *dflt,
			   void (* on_complete)(SOCKET_DATA *, const char *)) {
  EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "editor_aux_data"); 
  data->working_buf = newBuffer(1);
  bufferCat(data->working_buf, dflt);
  data->editor      = editor;
  data->on_complete = on_complete;
  if(editor->init) editor->init(sock);
  begin_editor(sock);
}

void socketStartEditor(SOCKET_DATA *sock, EDITOR *editor, BUFFER *buf) {
  EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "editor_aux_data"); 
  data->buf         = buf;
  socketStartEditorFunc(sock, editor, bufferString(buf), dflt_editor_complete);
}

EDITOR *socketGetEditor(SOCKET_DATA *sock) {
  EDITOR_AUX_DATA *data = socketGetAuxiliaryData(sock, "editor_aux_data");
  return data->editor;
}
