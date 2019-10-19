#ifndef EDITOR_H
#define EDITOR_H
//*****************************************************************************
//
// editor.h
//
// one of the problems that came up with the origional text editor what that
// it started becoming rather kludgy and hackish as we started adding more
// features to it, like script editing capabilities. It was difficult to
// coordinate when we wanted certain options and when we didn't. This is an
// attempt to make things a bit easier to scale up with.
//
//*****************************************************************************

typedef struct editor_data EDITOR;

// a basic text editor that can be used by other modules
extern EDITOR *text_editor;

// an editor for dialog. Essentially, text without newlines
extern EDITOR *dialog_editor;


// prepare the editing system for use
void init_editor();

// sets up a little notepad that the character can write/post stuff from.
// Notepads are erased when the socket disconnects.
void        init_notepad();
BUFFER *socketGetNotepad(SOCKET_DATA *sock);
void    socketSetNotepad(SOCKET_DATA *sock, const char *txt);
void  socketStartNotepad(SOCKET_DATA *sock, EDITOR *editor);


// editors are intended to be permenant and, thus, have no delete function.
// Modules that need an editing tool tailored to them (e.g. scripts) should make
// one copy of the editor when the module is initialized, and that should be the
// only copy ever used.
EDITOR *newEditor();

//
// Change the function that displays the editor prompt. By default, the editor
// prompt displays this:
// ] 
void editorSetPrompt(EDITOR *editor, void prompt(SOCKET_DATA *sock));

//
// When an editor is first entered, the socket may need some auxiliary data
// reset (e.g.: script editing will need indents set to zero). This is where
// all of that stuff is performed. The default editor does not have an init()
// and can be written over at will.
void editorSetInit(EDITOR *editor, void init(SOCKET_DATA *sock));

//
// change the function that appends data to the buffer. This may be useful in
// some cases where previous input determines how the next input is appended.
// So, for instance, if/else blocks for scripts.
void editorSetAppend(EDITOR *editor,
		     void append(SOCKET_DATA *sock, char *arg, BUFFER *buf));

//
// add a new command to the text editor, and map it to an editor function. The
// desc is the line that will be displayed in the help menu. The following 
// commands are reserved, and cannot be added over or removed:
//   /s, /q      quit and save changes
//   /a          quit but do not save changes
//   /h, /?      display the help menu
void editorAddCommand(EDITOR *editor, const char *cmd, const char *desc, 
		      void func(SOCKET_DATA *sock, char *arg, BUFFER *buf));

//
// Remove a command from the text editor. None of the reserved commands can be
// removed. This is basically intended to allow programmers to remove
// functionality from the default editor to customize it.
void editorRemoveCommand(EDITOR *editor, const char *cmd);

//
// Make the socket start editing the specified buffer with the supplied
// text editor.
void socketStartEditor(SOCKET_DATA *sock, EDITOR *editor, BUFFER *buf);

//
// begin editing text. Instead of copying the results to a buffer after
// completion, call a function which will pass in the resulting text.
void socketStartEditorFunc(SOCKET_DATA *sock, EDITOR *editor, const char *dflt,
			   void (* on_complete)(SOCKET_DATA *, const char *));
void socketStartPyEditorFunc(SOCKET_DATA *sock, EDITOR *editor,const char *dflt,
			     void *py_complete);

//
// get a pointer to the current editor the socket is using, if any
EDITOR *socketGetEditor(SOCKET_DATA *sock);

#endif // EDITOR_H
