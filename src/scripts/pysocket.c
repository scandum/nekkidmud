//*****************************************************************************
//
// pysocket.c
//
// Contains a python socket module, and an Socket class that is a python 
// wrapper for NakedMud sockets.
//
//*****************************************************************************

#include "../mud.h"
#include "../utils.h"
#include "../socket.h"
#include "../character.h"

#include "scripts.h"
#include "pyplugs.h"
#include "pyauxiliary.h"
#include "pysocket.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "../editor/editor.h"
#include "script_editor.h"



//*****************************************************************************
// local structures and defines
//*****************************************************************************

// a list of the get/setters on the Exit class
LIST *pysocket_getsetters = NULL;

// a list of the methods on the Exit class
LIST *pysocket_methods = NULL;

typedef struct {
  PyObject_HEAD
  int uid;
} PySocket;



//*****************************************************************************
// allocation, deallocation, initialization, and comparison
//*****************************************************************************
void PySocket_dealloc(PySocket *self) {
  self->ob_type->tp_free((PyObject*)self);
}

PyObject *PySocket_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PySocket *self;
    self = (PySocket *)type->tp_alloc(type, 0);
    self->uid = NOTHING;
    return (PyObject *)self;
}

int PySocket_init(PySocket *self, PyObject *args, PyObject *kwds) {
  char *kwlist[] = {"uid", NULL};
  int        uid = NOTHING;

  // get the uid
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &uid)) {
    PyErr_Format(PyExc_TypeError, "Sockets may only be created using an uid");
    return -1;
  }

  // make sure an socket with this name exists
  if(!propertyTableGet(sock_table, uid)) {
    PyErr_Format(PyExc_TypeError, "Socket with uid, %d, does not exist", uid);
    return -1;
  }

  self->uid = uid;
  return 0;
}

int PySocket_compare(PySocket *sock1, PySocket *sock2) {
  if(sock1->uid == sock2->uid)
    return 0;
  else if(sock1->uid < sock2->uid)
    return -1;
  else
    return 1;
}



//*****************************************************************************
// getters and setters for the Socket class
//*****************************************************************************
PyObject *PySocket_getuid(PySocket *self, void *closure) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock != NULL) return Py_BuildValue("i", socketGetUID(sock));
  else             return NULL;
}

PyObject *PySocket_getaccount(PySocket *self, void *closure) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return NULL;
  else {
    ACCOUNT_DATA *acc = socketGetAccount(sock);
    if(acc == NULL)
      return Py_BuildValue("O", Py_None);
    return Py_BuildValue("O", accountGetPyFormBorrowed(acc));
  }
}

PyObject *PySocket_getchar(PySocket *self, void *closure) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return NULL;
  else {
    CHAR_DATA *ch = socketGetChar(sock);
    // for the time being, we don't return characters without UIDs... like ones
    // that are being created. We have to redo character generation to allow
    // for characters-in-progress to be referenced
    if(ch == NULL || charGetUID(ch) == NOBODY)
      return Py_BuildValue("O", Py_None);
    return Py_BuildValue("O", charGetPyFormBorrowed(ch));
  }
}

PyObject *PySocket_getcmdread(PySocket *self, void *closure) { 
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return NULL;
  else {
    return Py_BuildValue("i", socketHasCommand(sock));
  }
}

PyObject *PySocket_get_outbound_text(PySocket *self, void *closure) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return NULL;
  else {
    return Py_BuildValue("s", bufferString(socketGetOutbound(sock)));
  } 
}

int PySocket_set_outbound_text(PySocket *self, PyObject *value, void *closure) {
  if(!PyString_Check(value)) {
    PyErr_Format(PyExc_TypeError, "Outbound text must be in string format.");
    return -1;
  }

  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return -1;
  else {
    bufferClear(socketGetOutbound(sock));
    bufferCat(socketGetOutbound(sock), PyString_AsString(value));
    return 0;
  }
}

PyObject *PySocket_get_can_use(PySocket *self, void *closure) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return NULL;
  else
    return Py_BuildValue("i", (socketGetDNSLookupStatus(sock) == TSTATE_DONE ?
			       TRUE : FALSE));
}

PyObject *PySocket_getstate(PySocket *self, void *closure) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return NULL;
  else
    return Py_BuildValue("s", socketGetState(sock));
}

PyObject *PySocket_getidletime(PySocket *self, void *closure) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return NULL;
  else
    return Py_BuildValue("f", socketGetIdleTime(sock));
}

PyObject *PySocket_gethostname(PySocket *self, void *closure) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return NULL;
  else if(socketGetDNSLookupStatus(sock) == TSTATE_DONE)
    return Py_BuildValue("s", socketGetHostname(sock));
  else
    return Py_BuildValue("s", "unresolved");
}

PyObject *PySocket_bust_prompt(PySocket *self, PyObject *closure) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to bust prompt on nonexistent "
		 "socket, %d.", PySocket_AsUid((PyObject *)self));
    return NULL;
  }
  socketBustPrompt(sock);
  return Py_BuildValue("i", 1);
}

PyObject *PySocket_push_ih(PySocket *self, PyObject *args) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  PyObject *handler = NULL;
  PyObject  *prompt = NULL;
  char       *state = NULL;

  if(sock == NULL)
    return NULL;

  if (!PyArg_ParseTuple(args, "OO|s", &handler, &prompt, &state)) {
    PyErr_Format(PyExc_TypeError, "handler and prompt function must "
		 "be supplied.");
    return NULL;
  }

  socketPushPyInputHandler(sock, handler, prompt, (state ? state : ""));
  return Py_BuildValue("i", 1);
}

PyObject *PySocket_pop_ih(PySocket *self) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return NULL;
  socketPopInputHandler(sock);
  return Py_BuildValue("i", 1);
}

PyObject *PySocket_replace_ih(PySocket *self, PyObject *args) {
  PyObject *retval = PySocket_pop_ih(self);
  if(retval == NULL)
    return NULL;
  Py_DECREF(retval);
  retval = PySocket_push_ih(self, args);
  if(retval == NULL)
    return NULL;
  Py_DECREF(retval);
  return Py_BuildValue("i", 1);
}

PyObject *PySocket_send_raw(PySocket *self, PyObject *value) {
  char *mssg = NULL;
  if (!PyArg_ParseTuple(value, "s", &mssg)) {
    PyErr_Format(PyExc_TypeError, "Sockets may only be sent strings");
    return NULL;
  }

  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock != NULL) {
    send_to_socket(sock, "%s", mssg);
    return Py_BuildValue("i", 1);
  }
  else {
    PyErr_Format(PyExc_TypeError, 
		 "Tried to send message to nonexistant socket, %d.", 
		 self->uid);
    return NULL;
  }
}

//
// Send a message with Python statements potentially embedded in it. For 
//evaluating 
PyObject *PySocket_send(PyObject *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[ ] = { "mssg", "dict", "newline", NULL };
  SOCKET_DATA *me = NULL;
  char      *text = NULL;
  PyObject  *dict = NULL;
  bool    newline = TRUE;

  if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|Ob", kwlist, 
				  &text, &dict, &newline)) {
    PyErr_Format(PyExc_TypeError, "Invalid arguments supplied to Mudsock.send");
    return NULL;
  }

  // is dict None? set it to NULL for expand_to_char
  if(dict == Py_None)
    dict = NULL;

  // make sure the dictionary is a dictionary
  if(!(dict == NULL || PyDict_Check(dict))) {
    PyErr_Format(PyExc_TypeError, "Mudsock.send expects second argument to be a dict object.");
    return NULL;
  }
  
  // make sure we exist
  if( (me = PySocket_AsSocket(self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "Tried to send nonexistent socket.");
    return NULL;
  }

  if(dict != NULL)
    PyDict_SetItemString(dict, "me", self);

  // build our script environment
  BUFFER   *buf = newBuffer(1);
  bufferCat(buf, text);

  // expand out our dynamic descriptions if we have a dictionary supplied
  if(dict != NULL) {
    PyObject *env = restricted_script_dict();
    PyDict_Update(env, dict);

    // do the expansion
    expand_dynamic_descs_dict(buf, env, get_script_locale());
    Py_XDECREF(env);
  }

  if(newline == TRUE)
    bufferCat(buf, "\r\n");
  text_to_buffer(me, bufferString(buf));

  // garbage collection
  deleteBuffer(buf);
  return Py_BuildValue("");
}

PyObject *PySocket_close(PySocket *self) {
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL)
    return NULL;
  close_socket(sock, FALSE);
  return Py_BuildValue("i", 1);
}

PyObject *PySocket_edit_text(PyObject *self, PyObject *args) {
  PyObject *on_complete = NULL;
  char            *text = NULL;
  char            *type = NULL;
  SOCKET_DATA     *sock = NULL;
  EDITOR        *editor = text_editor;

  if(!PyArg_ParseTuple(args, "sO|s", &text, &on_complete, &type)) {
    PyErr_Format(PyExc_TypeError, "invalid arguments to sock.edit_text");
    return NULL;
  }

  if(!PySocket_Check(self)) {
    PyErr_Format(PyExc_TypeError, "Owner of edit_text not a socket");
    return NULL;
  }
  else if( (sock = PySocket_AsSocket(self)) == NULL) {
    PyErr_Format(PyExc_TypeError, "sock.edit_text called on non-existent socket.");
    return NULL;
  }

  if(type != NULL && compares(type, "script"))
    editor = script_editor;
  
  // begin the editor
  socketStartPyEditorFunc(sock, editor, text, on_complete);
  return Py_BuildValue("O", Py_None);
}



//*****************************************************************************
// methods for the Socket class
//*****************************************************************************

//
// returns the specified piece of auxiliary data from the socket
// if it is a piece of python auxiliary data.
PyObject *PySocket_get_auxiliary(PySocket *self, PyObject *args) {
  char *keyword = NULL;
  if(!PyArg_ParseTuple(args, "s", &keyword)) {
    PyErr_Format(PyExc_TypeError,
		 "getAuxiliary() must be supplied with the name that the "
		 "auxiliary data was installed under!");
    return NULL;
  }

  // make sure we exist
  SOCKET_DATA *sock = PySocket_AsSocket((PyObject *)self);
  if(sock == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to get auxiliary data for a nonexistant socket.");
    return NULL;
  }

  // make sure the auxiliary data exists
  if(!pyAuxiliaryDataExists(keyword)) {
    PyErr_Format(PyExc_StandardError,
		 "No auxiliary data named '%s' exists!", keyword);
    return NULL;
  }

  PyObject *data = socketGetAuxiliaryData(sock, keyword);
  if(data == NULL)
    data = Py_None;
  PyObject *retval = Py_BuildValue("O", data);
  //  Py_DECREF(data);
  return retval;
}



//*****************************************************************************
// structures to define our methods and classes
//*****************************************************************************
PyTypeObject PySocket_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "mudsock.Mudsock",         /*tp_name*/
    sizeof(PySocket),          /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PySocket_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    (cmpfunc)PySocket_compare, /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Python Socket object",    /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */ 
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PySocket_init,   /* tp_init */
    0,                         /* tp_alloc */
    PySocket_new,              /* tp_new */
};



//*****************************************************************************
// methods in the mudsock module
//*****************************************************************************
PyObject *PySocket_all_sockets(PyObject *self) {
  PyObject        *list = PyList_New(0);
  LIST_ITERATOR *sock_i = newListIterator(socket_list);
  SOCKET_DATA     *sock = NULL;
  ITERATE_LIST(sock, sock_i) {
    PyList_Append(list, socketGetPyFormBorrowed(sock));
  } deleteListIterator(sock_i);
  PyObject *retval = Py_BuildValue("O", list);
  Py_DECREF(list);
  return retval;
}

PyMethodDef socket_module_methods[] = {
  { "socket_list", (PyCFunction)PySocket_all_sockets, METH_NOARGS,
    "socket_list()\n\n"
    "Returns a list of all sockets currently connected." },
  {NULL, NULL, 0, NULL}  /* Sentinel */
};



//*****************************************************************************
// implementation of pyexit.h
//*****************************************************************************
void PySocket_addGetSetter(const char *name, void *g, void *s,const char *doc){
  // make sure our list of get/setters is created
  if(pysocket_getsetters == NULL) pysocket_getsetters = newList();

  // make the GetSetter def
  PyGetSetDef *def = calloc(1, sizeof(PyGetSetDef));
  def->name        = strdup(name);
  def->get         = (getter)g;
  def->set         = (setter)s;
  def->doc         = (doc ? strdup(doc) : NULL);
  def->closure     = NULL;
  listPut(pysocket_getsetters, def);
}

void PySocket_addMethod(const char *name, void *f, int flags, const char *doc){
  // make sure our list of methods is created
  if(pysocket_methods == NULL) pysocket_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pysocket_methods, def);
}

// initialize sockets for use. This must be called AFTER 
PyMODINIT_FUNC
init_PySocket(void) {
    PyObject* module = NULL;

    // add all of the basic getsetters
    PySocket_addGetSetter("uid", PySocket_getuid, NULL,
      "The socket's uid. Immutable.");
    PySocket_addGetSetter("account", PySocket_getaccount, NULL,
      "The account currently attached to the socket, or None. Immutable.\n"
      "see mudsys.attach_account_socket for connecting sockets and accounts.");
    PySocket_addGetSetter("character", PySocket_getchar, NULL,
      "The character currently attached to the socket, on None. Immutable.\n"
      "see mudsys.attach_char_socket for connecting characters to account.");
    PySocket_addGetSetter("char", PySocket_getchar, NULL,
       "Alias for mudsock.Mudsock.character");
    PySocket_addGetSetter("ch",   PySocket_getchar, NULL,
       "Alias for mudsock.Mudsock.character.");
    PySocket_addGetSetter("has_input", PySocket_getcmdread, NULL,
       "True or False if the socket has any input pending. Immutable.");
    PySocket_addGetSetter("outbound_text",
       PySocket_get_outbound_text, PySocket_set_outbound_text,
       "The socket's outbound text.");
    PySocket_addGetSetter("can_use", PySocket_get_can_use, NULL,
      "True or False if the socket is ready for use. Socket becomes available\n"
      "after its dns addresss resolves. Immutable.");
    PySocket_addGetSetter("state", PySocket_getstate, NULL,
      "The state that the socket is in. Immutable. For more on states see\n"
      "mudsock.Mudsock.push_ih");
    PySocket_addGetSetter("idle_time", PySocket_getidletime, NULL,
      "How long (in seconds) the socket's input handler has been idle for. Immutable.");
    PySocket_addGetSetter("hostname", PySocket_gethostname, NULL,
      "The dns address that the socket is connected from. Immutable.");

    // add all of the basic methods
    PySocket_addMethod("getAuxiliary", PySocket_get_auxiliary, METH_VARARGS,
      "getAuxiliary(name)\n\n"
      "Returns socket's auxiliary data of the specified name.");
    PySocket_addMethod("aux", PySocket_get_auxiliary, METH_VARARGS,
      "Alias for mudsock.Mudsock.getAuxiliary");
    PySocket_addMethod("send", PySocket_send, METH_KEYWORDS,
      "send(mssg, dict = None, newline = True)\n"
      "\n"
      "Sends message to the socket. Messages can have scripts embedded in\n" 
      "them, using [ and ]. If so, a variable dictionary must be provided. By\n"
      "default, 'me' references the socket being sent the message.");
    PySocket_addMethod("send_raw", PySocket_send_raw, METH_VARARGS,
      "send_raw(mssg)\n\n"
      "Sends text to the socket. No appended newline.");
    PySocket_addMethod("pop_ih", PySocket_pop_ih, METH_NOARGS,
      "pop_ih()\n\n"
      "Pops the socket's current input handler from its input handler stack.");
    PySocket_addMethod("push_ih", PySocket_push_ih, METH_VARARGS,
      "push_ih(handler_func, prompt_func, state=None)\n\n"
      "Pushes a new input handler and prompt pair onto the socket's input\n"
      "handler stack. Optionally, a (String) state value can be supplied.\n"
      "Input handlers take two arguments: the socket and a string command.\n"
      "Prompts take one argument: the socket. They should send the relevant\n"
      "text for the prompt to the socket.");
    PySocket_addMethod("replace_ih", PySocket_replace_ih, METH_VARARGS,
      "repalce_ih(handler_func, prompt_func, state=None)\n\n"
      "Calls pop_ih, followed by push_ih.");
    PySocket_addMethod("close", PySocket_close, METH_VARARGS,
      "close()\n\n"
      "Closes the socket's connection.");
    PySocket_addMethod("bust_prompt", PySocket_bust_prompt, METH_NOARGS,
      "bust_prompt()\n\n"
      "Busts the socket's prompt so it will be displayed next pulse.");
    PySocket_addMethod("edit_text", PySocket_edit_text, METH_VARARGS, 
      "edit_text(dflt_value, on_complete, mode='text')\n\n"
      "Enter the text editor, and set its default value. When the text editor\n"
      "is edited, call on_complete. This function should take two arguments:\n"
      "the socket doing the editing, and the output of the editor. Mode can\n"
      "be 'text' or 'script'.");

    // add in all the getsetters and methods
    makePyType(&PySocket_Type, pysocket_getsetters, pysocket_methods);
    deleteListWith(pysocket_getsetters, free); pysocket_getsetters = NULL;
    deleteListWith(pysocket_methods,    free); pysocket_methods    = NULL;

    // make sure the socket class is ready to be made
    if (PyType_Ready(&PySocket_Type) < 0)
        return;

    // initialize the module
    module = Py_InitModule3("mudsock", socket_module_methods,
      "Contains the Python wrapper for sockets, and utilities for listing\n"
      "currently connected sockets.");

    // make sure the module parsed OK
    if (module == NULL)
      return;

    // add the Socket class to the socket module
    PyTypeObject *type = &PySocket_Type;
    PyModule_AddObject(module, "Mudsock", (PyObject *)type);
    Py_INCREF(&PySocket_Type);
}

int PySocket_Check(PyObject *value) {
  return PyObject_TypeCheck(value, &PySocket_Type);
}

int PySocket_AsUid(PyObject *sock) {
  return ((PySocket *)sock)->uid;
}

SOCKET_DATA *PySocket_AsSocket(PyObject *ch) {
  return propertyTableGet(sock_table, PySocket_AsUid(ch));
}

PyObject *
newPySocket(SOCKET_DATA *sock) {
  PySocket *pysock = (PySocket *)PySocket_new(&PySocket_Type, NULL, NULL);
  pysock->uid = socketGetUID(sock);
  return (PyObject *)pysock;
}
