//*****************************************************************************
//
// pymudsys.c
//
// A set of system level commands and variables that may be needed by python,
// but which are not neccessarily needed by scripts.
//
//*****************************************************************************
#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../utils.h"
#include "../character.h"
#include "../socket.h"
#include "../save.h"
#include "../handler.h"
#include "../account.h"
#include "../storage.h"
#include "../world.h"
#include "../zone.h"

#include "pymudsys.h"
#include "scripts.h"
#include "pyplugs.h"
#include "pychar.h"
#include "pyaccount.h"
#include "pysocket.h"
#include "pyroom.h"
#include "pyexit.h"
#include "pyobj.h"
#include "pystorage.h"



//******************************************************************************
// optional modules
//******************************************************************************
#ifdef MODULE_HELP2
#include "../help2/help.h"
#endif



//*****************************************************************************
// local variables and functions
//*****************************************************************************

// a dictionary of world types and their appropriate Python class
PyObject *worldtypes = NULL;

// a list of methods to add to the mudsys module
LIST *pymudsys_methods = NULL;

// a placeholder of the movement command, as set by one of our modules
PyObject *py_cmd_move = NULL;

// a list of checks that need to be performed before movement
LIST *py_move_checks = NULL;

// a list of default movement commands
LIST *dflt_move_cmds = NULL;



//*****************************************************************************
// mudsys methods
//*****************************************************************************
PyObject *mudsys_set_sys_val(PyObject *self, PyObject *args) {
  char *key, *val;
  if(!PyArg_ParseTuple(args, "ss", &key, &val)) {
    PyErr_Format(PyExc_TypeError, "Provide a string key and value");
    return NULL;
  }

  mudsettingSetString(key, val);
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_get_sys_val(PyObject *self, PyObject *args) {
  char *key;
  if(!PyArg_ParseTuple(args, "s", &key)) {
    PyErr_Format(PyExc_TypeError, "Provide a string key");
    return NULL;
  }

  return Py_BuildValue("s", mudsettingGetString(key));
}

PyObject *mudsys_shutdown(PyObject *self, PyObject *args) {
  shut_down = TRUE;
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_copyover(PyObject *self, PyObject *args) {
  do_copyover();
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_create_account(PyObject *self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }

  if(account_exists(name))
    return Py_BuildValue("O", Py_None);
  if(account_creating(name))
    return Py_BuildValue("O", Py_None);
  ACCOUNT_DATA *acct = newAccount();
  accountSetName(acct, name);
  return Py_BuildValue("O", accountGetPyFormBorrowed(acct));
}

PyObject *mudsys_create_player(PyObject *Self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }

  if(player_exists(name))
    return Py_BuildValue("O", Py_None);
  if(player_creating(name))
    return Py_BuildValue("O", Py_None);
  CHAR_DATA *ch = newChar();
  charSetName(ch, name);

  // give the character a unique id 
  int next_char_uid = mudsettingGetInt("puid") + 1;
  mudsettingSetInt("puid", next_char_uid);
  charSetUID(ch, next_char_uid);
      
  // if it's the first player, give him all priviledges
  if(charGetUID(ch) == 1)
    bitSet(charGetUserGroups(ch),
	   "admin, wizard, builder, scripter, player, playtester");

  // make sure we'll load back into the same room we were saved in
  charSetLoadroom(ch, START_ROOM);

  char_exist(ch);
  return Py_BuildValue("O", charGetPyFormBorrowed(ch));
}

PyObject *mudsys_player_exists(PyObject *self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }
  return Py_BuildValue("i", player_exists(name));
}

PyObject *mudsys_account_exists(PyObject *self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }
  return Py_BuildValue("i", account_exists(name));
}

PyObject *mudsys_player_creating(PyObject *self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }
  return Py_BuildValue("i", player_creating(name));
}

PyObject *mudsys_account_creating(PyObject *self, PyObject *args) {
  char *name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A string name must be supplied.");
    return NULL;
  }
  return Py_BuildValue("i", account_creating(name));
}

//
// registers a player into the system
PyObject *mudsys_do_register(PyObject *Self, PyObject *args) {
  PyObject      *val = NULL;
  CHAR_DATA      *ch = NULL;
  ACCOUNT_DATA *acct = NULL;

  if(!PyArg_ParseTuple(args, "O", &val)) {
    PyErr_Format(PyExc_TypeError, 
		 "A char or account to be registered must be supplied.");
    return NULL;
  }

  if(PyChar_Check(val)) {
    if((ch = PyChar_AsChar(val)) == NULL) {
      PyErr_Format(PyExc_StandardError,
		   "Tried to register nonexistant player, %d.",
		   PyChar_AsUid(val));
      return NULL;
    }
    if(!charIsNPC(ch))
      register_player(ch);
  }
  else if(PyAccount_Check(val)) {
    if((acct = PyAccount_AsAccount(val)) == NULL) {
      PyErr_Format(PyExc_StandardError,
		   "Tried to register nonexistant account.");
      return NULL;
    }
    register_account(acct);
  }
  else { 
    PyErr_Format(PyExc_TypeError, 
		 "Only players and accounts may be registered.");
    return NULL;
  }

  return Py_BuildValue("i", 1);
}

//
// saves a player to disk
PyObject *mudsys_do_save(PyObject *self, PyObject *args) {
  PyObject      *val = NULL;
  CHAR_DATA      *ch = NULL;
  ACCOUNT_DATA *acct = NULL;

  if(!PyArg_ParseTuple(args, "O", &val)) {
    PyErr_Format(PyExc_TypeError, "A character or account must be supplied.");
    return NULL;
  }

  if(PyChar_Check(val)) {
    if( (ch = PyChar_AsChar(val)) == NULL) {
      PyErr_Format(PyExc_StandardError,
		   "Tried to save nonexistant character, %d.",
		   PyChar_AsUid(val));
      return NULL;
    }
    // only save registered characters
    if(!player_exists(charGetName(ch)))
      return Py_BuildValue("i", 0);
    if(!charIsNPC(ch))
      save_player(ch);
  }
  else if(PyAccount_Check(val)) {
    if( (acct = PyAccount_AsAccount(val)) == NULL) {
      PyErr_Format(PyExc_StandardError, "Tried to save nonexistant account.");
      return NULL;
    }
    // only save registered players
    if(!account_exists(accountGetName(acct)))
      return Py_BuildValue("i", 0);
    save_account(acct);
  }
  else {
    PyErr_Format(PyExc_TypeError, "Only characters and accounts may be saved.");
    return NULL;
  }

  return Py_BuildValue("i", 1);
}

//
// attaches an account to a socket
PyObject *mudsys_attach_account_socket(PyObject *self, PyObject *args) {
  PyObject   *pyacct = NULL;
  PyObject   *pysock = NULL;
  ACCOUNT_DATA *acct = NULL;
  SOCKET_DATA  *sock = NULL;

  if(!PyArg_ParseTuple(args, "OO", &pyacct, &pysock)) {
    PyErr_Format(PyExc_TypeError,"An account and socket to be attached must be supplied.");
    return NULL;
  }

  if(!PyAccount_Check(pyacct)) {
    PyErr_Format(PyExc_TypeError, "First argument must be an account.");
    return NULL;
  }
  if(!PySocket_Check(pysock)) {
    PyErr_Format(PyExc_TypeError, "Second argument must be a socket.");
    return NULL;
  }

  if( (acct = PyAccount_AsAccount(pyacct)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach nonexistant account.");
    return NULL;
  }
  if( (sock = PySocket_AsSocket(pysock)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach nonexistant socket, %d.",PySocket_AsUid(pysock));
    return NULL;
  }

  // first, do any nessessary detaching for our old character and old socket
  if(socketGetAccount(sock) != NULL && 
     account_exists(accountGetName(socketGetAccount(sock))))
    unreference_account(socketGetAccount(sock));
  socketSetAccount(sock, acct);
  if(account_exists(accountGetName(acct)))
    reference_account(acct);
  return Py_BuildValue("i", 1);
}

//
// attaches a character to a socket
PyObject *mudsys_attach_char_socket(PyObject *self, PyObject *args) {
  PyObject    *pych = NULL;
  PyObject  *pysock = NULL;
  CHAR_DATA     *ch = NULL;
  SOCKET_DATA *sock = NULL;

  if(!PyArg_ParseTuple(args, "OO", &pych, &pysock)) {
    PyErr_Format(PyExc_TypeError,"A character and socket to be attached must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "First argument must be a character.");
    return NULL;
  }
  if(!PySocket_Check(pysock)) {
    PyErr_Format(PyExc_TypeError, "Second argument must be a socket.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }
  if( (sock = PySocket_AsSocket(pysock)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to attach nonexistant socket, %d.",PySocket_AsUid(pysock));
    return NULL;
  }

  // first, do any nessessary detaching for our old character and old socket
  if(socketGetChar(sock) != NULL)
    charSetSocket(socketGetChar(sock), NULL);
  if(charGetSocket(ch) != NULL)
    socketSetChar(charGetSocket(ch), NULL);
  charSetSocket(ch, sock);
  socketSetChar(sock, ch);
  return Py_BuildValue("i", 1);
}

//
// detaches a character from its socket
PyObject *mudsys_detach_char_socket(PyObject *self, PyObject *args) {
  PyObject    *pych = NULL;
  CHAR_DATA     *ch = NULL;
  SOCKET_DATA *sock = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError,"A character to be detached must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may be detached.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to detach nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }

  sock = charGetSocket(ch);
  if(sock != NULL)
    socketSetChar(sock, NULL);
  charSetSocket(ch, NULL);
  return Py_BuildValue("i", 1);
}

//
// quits a character from the game
PyObject *mudsys_do_quit(PyObject *self, PyObject *args) {
  PyObject *pych = NULL;
  CHAR_DATA  *ch = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError,"A character to be quitted must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may be quitted.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to quit nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }

  if(!charIsNPC(ch) && charGetSocket(ch)) {
    SOCKET_DATA *sock = charGetSocket(ch);
    charSetSocket(ch, NULL);
    socketSetChar(sock, NULL);
    socketPopInputHandler(sock);
  }

  extract_mobile(ch);
  return Py_BuildValue("i", 1);
}

//
// loads an account from disk
PyObject *mudsys_load_account(PyObject *self, PyObject *args) {
  char *name = NULL;
  
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "An account name must be supplied.");
    return NULL;
  }

  // newPyAccount increases the reference count on the account. We can
  // unreference it after we've created the Python wrapper for it
  ACCOUNT_DATA *acct = get_account(name);
  if(acct == NULL)
    return Py_BuildValue("O", Py_None);
  PyObject *retval = Py_BuildValue("O", accountGetPyFormBorrowed(acct));
  unreference_account(acct);
  return retval;
}

//
// loads a character from disk
PyObject *mudsys_load_char(PyObject *self, PyObject *args) {
  char *name = NULL;
  
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "A character name must be supplied.");
    return NULL;
  }

  CHAR_DATA *ch = get_player(name);
  if(ch == NULL)
    return Py_BuildValue("O", Py_None);

  char_exist(ch);
  return Py_BuildValue("O", charGetPyFormBorrowed(ch));
}

//
// tries to put the player into the game
PyObject *mudsys_try_enter_game(PyObject *self, PyObject *args) {
  PyObject *pych = NULL;
  CHAR_DATA  *ch = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError,"A character to enter game must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may enter game.");
    return NULL;
  }

  if((ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried enter game with nonexistant character, %d.",
		 PyChar_AsUid(pych));
    return NULL;
  }

  // only enter game if we're not already in the game
  if(listIn(mobile_list, ch))
    return Py_BuildValue("i", 0);
  else
    return Py_BuildValue("i", try_enter_game(ch));
}

//
// disconnects a character from its socket
PyObject *mudsys_do_disconnect(PyObject *self, PyObject *args) {
  PyObject *pych = NULL;
  CHAR_DATA  *ch = NULL;

  if(!PyArg_ParseTuple(args, "O", &pych)) {
    PyErr_Format(PyExc_TypeError,"A character to be dc'd must be supplied.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError, "Only characters may be dc'd.");
    return NULL;
  }

  if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to dc nonexistant character, %d.",PyChar_AsUid(pych));
    return NULL;
  }

  if(charGetSocket(ch)) {
    SOCKET_DATA *sock = charGetSocket(ch);
    charSetSocket(ch, NULL);
    socketSetChar(sock, NULL);
    close_socket(sock, FALSE);
  }

  return Py_BuildValue("i", 1);
}

PyObject *mudsys_password_matches(PyObject *self, PyObject *args) {
  PyObject   *pyacct = NULL;
  char          *pwd = NULL;
  ACCOUNT_DATA *acct = NULL;

  if(!PyArg_ParseTuple(args, "Os", &pyacct, &pwd)) {
    PyErr_Format(PyExc_TypeError, "an account and password must be supplied.");
    return NULL;
  }

  if(!PyAccount_Check(pyacct)) {
    PyErr_Format(PyExc_TypeError, "only accounts may have passwords checked.");
    return NULL;
  }

  if( (acct = PyAccount_AsAccount(pyacct)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to check password for nonexistant account.");
    return NULL;
  }

  return Py_BuildValue("i", compares(crypt(pwd, accountGetName(acct)), 
				     accountGetPassword(acct)));
}

PyObject *mudsys_set_password(PyObject *self, PyObject *args) {
  PyObject   *pyacct = NULL;
  char          *pwd = NULL;
  ACCOUNT_DATA *acct = NULL;

  if(!PyArg_ParseTuple(args, "Os", &pyacct, &pwd)) {
    PyErr_Format(PyExc_TypeError, "an account and password must be supplied.");
    return NULL;
  }

  if(!PyAccount_Check(pyacct)) {
    PyErr_Format(PyExc_TypeError, "only accounts may have passwords set.");
    return NULL;
  }

  if( (acct = PyAccount_AsAccount(pyacct)) == NULL) {
    PyErr_Format(PyExc_StandardError,
		 "Tried to set password for nonexistant account.");
    return NULL;
  }

  accountSetPassword(acct, crypt(pwd, accountGetName(acct)));
  return Py_BuildValue("i", 1);
}


//
// add a new command to the mud, via a python script or module. Takes in a
// command name, a sort_by command, the function, a minimum and maximum 
// position in the form of strings, a level, and boolean values for whether the
// command can be performed by mobiles, and whether it interrupts actions.
PyObject *mudsys_add_cmd(PyObject *self, PyObject *args) {
  PyObject *func = NULL;
  char *name  = NULL, *sort_by = NULL, *group = NULL;
  bool interrupts = FALSE;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "szOsb", &name, &sort_by, &func,
  			&group, &interrupts)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not add new command. Improper arguments supplied");
    return NULL;
  }

  // make sure it's a function object, and check its documentation to see if
  // we can add it as a helpfile
  if(PyFunction_Check(func)) {
    add_py_cmd(name, sort_by, func, group, interrupts);
#ifdef MODULE_HELP2
    if(get_help(name,FALSE)==NULL && 
       ((PyFunctionObject*)func)->func_doc != NULL &&
       PyString_Check(((PyFunctionObject*)func)->func_doc)){
      BUFFER *buf = newBuffer(1);
      bufferCat(buf, PyString_AsString(((PyFunctionObject*)func)->func_doc));
      bufferFormat(buf, SCREEN_WIDTH, 0);
      if(bufferLength(buf) > 0)
	add_help(name, bufferString(buf), group, NULL, FALSE);
      deleteBuffer(buf);
    }
#endif
  }

  return Py_BuildValue("");
}

//
// adds a check prior to the command's execution. If any check returns false,
// the command fails. Checks are assumed to tell people why their command
// failed.
PyObject *mudsys_add_cmd_check(PyObject *self, PyObject *args) {
  PyObject *func = NULL;
  char    *name  = NULL;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "sO", &name, &func)) {
    PyErr_Format(PyExc_TypeError, 
	       "Could not add new command check. Improper arguments supplied.");
    return NULL;
  }

  // add the command to the game
  if(PyFunction_Check(func))
    add_py_cmd_check(name, func);
  return Py_BuildValue("");
}

//
// removes a command from the game
PyObject *mudsys_remove_cmd(PyObject *self, PyObject *args) {
  char *name = NULL;
  // parse all of the values
  if (!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Format(PyExc_TypeError, "function requires string argument.");
    return NULL;
  }
  CMD_DATA *cmd = remove_cmd(name);
  if(cmd != NULL)
    deleteCmd(cmd);
  return Py_BuildValue("O", Py_None);
}


PyObject *mudsys_handle_cmd_input(PyObject *self, PyObject *args) {
  PyObject  *pysock = NULL;
  SOCKET_DATA *sock = NULL;
  char         *cmd = NULL;

  if(!PyArg_ParseTuple(args, "Os", &pysock, &cmd)) {
    PyErr_Format(PyExc_TypeError, "A socket and command must be supplied.");
    return NULL;
  }

  if(!PySocket_Check(pysock)) {
    PyErr_Format(PyExc_TypeError, "A socket must be supplied.");
    return NULL;
  }

  if( (sock = PySocket_AsSocket(pysock)) == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to run command for nonexistent socket.");
    return NULL;
  }

  handle_cmd_input(sock, cmd);
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_show_prompt(PyObject *self, PyObject *args) {
  PyObject  *pysock = NULL;
  SOCKET_DATA *sock = NULL;

  if(!PyArg_ParseTuple(args, "O", &pysock)) {
    PyErr_Format(PyExc_TypeError, "A socket must be supplied.");
    return NULL;
  }

  if(!PySocket_Check(pysock)) {
    PyErr_Format(PyExc_TypeError, "A socket must be supplied.");
    return NULL;
  }

  if( (sock = PySocket_AsSocket(pysock)) == NULL) {
    PyErr_Format(PyExc_StandardError, "Tried to show prompt to nonexistent socket.");
    return NULL;
  }

  show_prompt(sock);
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_set_cmd_move(PyObject *self, PyObject *args) {
  PyObject *cmd = NULL;
  if(!PyArg_ParseTuple(args, "O", &cmd)) {
    PyErr_Format(PyExc_TypeError, "a command must be suppled");
    return NULL;
  }

  // make sure it's a function
  if(!PyFunction_Check(cmd)) {
    PyErr_Format(PyExc_TypeError, "a command must be suppled");
    return NULL;
  }

  Py_XDECREF(py_cmd_move);
  py_cmd_move = cmd;
  Py_XINCREF(cmd);
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_register_dflt_move_cmd(PyObject *self, PyObject *args) {
  char *cmd = NULL;
  if(!PyArg_ParseTuple(args, "s", &cmd)) {
    PyErr_Format(PyExc_TypeError, "a command name must be supplied.");
    return NULL;
  }

  listPut(dflt_move_cmds, strdupsafe(cmd));

  // add all of our default movement checks
  LIST_ITERATOR *chk_i = newListIterator(py_move_checks);
  PyObject        *chk = NULL;
  ITERATE_LIST(chk, chk_i) {
    add_py_cmd_check(cmd, chk);
  } deleteListIterator(chk_i);

  return Py_BuildValue("i", 1);
}

PyObject *mudsys_register_move_check(PyObject *self, PyObject *args) {
  PyObject *check = NULL;
  if(!PyArg_ParseTuple(args, "O", &check)) {
    PyErr_Format(PyExc_TypeError, "a check must be suppled");
    return NULL;
  }

  // make sure it's a function
  if(!PyFunction_Check(check)) {
    PyErr_Format(PyExc_TypeError, "a function must be suppled");
    return NULL;
  }

  // add it to all of our default movement commands
  LIST_ITERATOR *dir_i = newListIterator(dflt_move_cmds);
  char            *dir = NULL;
  ITERATE_LIST(dir, dir_i) {
    add_py_cmd_check(dir, check);
  } deleteListIterator(dir_i);

  // now keep track of it for all of our special movement commands
  Py_XINCREF(check);
  listQueue(py_move_checks, check);
  return Py_BuildValue("i", 1);
}



//*****************************************************************************
// functions for adding methods to classes within Python
//*****************************************************************************
PyObject *mudsys_gen_add_method(PyTypeObject *type, PyObject *args) {
  PyObject *pyname = NULL;
  PyObject *method = NULL;
  char       *name = NULL;

  if(!PyArg_ParseTuple(args, "OO", &pyname, &method)) {
    PyErr_Format(PyExc_TypeError, "Method takes string and function or property argument.");
    return NULL;
  }

  if(!PyString_Check(pyname)) {
    PyErr_Format(PyExc_TypeError, "First argument not a string.");
    return NULL;
  }

  if(!PyFunction_Check(method) && !PyObject_TypeCheck(method,&PyProperty_Type)){
    PyErr_Format(PyExc_TypeError,"Second argument not a function or property.");
    return NULL;
  }

  name = PyString_AsString(pyname);
  if(PyDict_Contains(type->tp_dict, pyname)) {
    PyErr_Format(PyExc_TypeError, "%s already has attribute, %s.", 
		 type->tp_name, name);
    return NULL;
  }

  // add our new method
  PyDict_SetItemString(type->tp_dict, name, method);
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_add_sock_method(PyObject *self, PyObject *args) {
  return mudsys_gen_add_method(&PySocket_Type, args);
}
PyObject *mudsys_add_room_method(PyObject *self, PyObject *args) {
  return mudsys_gen_add_method(&PyRoom_Type, args);
}
PyObject *mudsys_add_exit_method(PyObject *self, PyObject *args) {
  return mudsys_gen_add_method(&PyExit_Type, args);
}
PyObject *mudsys_add_obj_method(PyObject *self, PyObject *args) {
  return mudsys_gen_add_method(&PyObj_Type, args);
}
PyObject *mudsys_add_char_method(PyObject *self, PyObject *args) {
  return mudsys_gen_add_method(&PyChar_Type, args);
}
PyObject *mudsys_add_acct_method(PyObject *self, PyObject *args) {
  return mudsys_gen_add_method(&PyAccount_Type, args);
}

LIST *pychar_see_checks = NULL;
LIST *pyobj_see_checks  = NULL;
LIST *pyexit_see_checks = NULL;

bool pycan_see_char(CHAR_DATA *ch, CHAR_DATA *target) {
  bool ret = TRUE;
  if(pychar_see_checks != NULL) {
    PyObject       *pych = charGetPyFormBorrowed(ch);
    PyObject   *pytarget = charGetPyFormBorrowed(target);
    LIST_ITERATOR *chk_i = newListIterator(pychar_see_checks);
    PyObject        *chk = NULL;
    ITERATE_LIST(chk, chk_i) {
      PyObject *pyret = PyObject_CallFunction(chk, "OO", pych, pytarget);
      if(pyret == NULL)
	log_pyerr("Error evaluating char cansee check");
      else if(!PyObject_IsTrue(pyret))
	ret = FALSE;

      Py_XDECREF(pyret);
      if(ret == FALSE)
	break;
    } deleteListIterator(chk_i);
  }
  return ret;
}

bool pycan_see_obj(CHAR_DATA *ch, OBJ_DATA *target) {
  bool ret = TRUE;
  if(pyobj_see_checks != NULL) {
    PyObject       *pych = charGetPyFormBorrowed(ch);
    PyObject   *pytarget = objGetPyFormBorrowed(target);
    LIST_ITERATOR *chk_i = newListIterator(pyobj_see_checks);
    PyObject        *chk = NULL;
    ITERATE_LIST(chk, chk_i) {
      PyObject *pyret = PyObject_CallFunction(chk, "OO", pych, pytarget);
      if(pyret == NULL)
	log_pyerr("Error evaluating obj cansee check");
      else if(!PyObject_IsTrue(pyret))
	ret = FALSE;

      Py_XDECREF(pyret);
      if(ret == FALSE)
	break;
    } deleteListIterator(chk_i);
  }
  return ret;
}

bool pycan_see_exit(CHAR_DATA *ch, EXIT_DATA *target) {
  bool ret = TRUE;
  if(pyexit_see_checks != NULL) {
    PyObject       *pych = charGetPyFormBorrowed(ch);
    PyObject   *pytarget = newPyExit(target);
    LIST_ITERATOR *chk_i = newListIterator(pyexit_see_checks);
    PyObject        *chk = NULL;
    ITERATE_LIST(chk, chk_i) {
      PyObject *pyret = PyObject_CallFunction(chk, "OO", pych, pytarget);
      if(pyret == NULL)
	log_pyerr("Error evaluating exit cansee check");
      else if(!PyObject_IsTrue(pyret))
	ret = FALSE;

      Py_XDECREF(pyret);
      if(ret == FALSE)
	break;
    } deleteListIterator(chk_i);
    Py_DECREF(pytarget);
  }
  return ret;
}

PyObject *mudsys_register_char_cansee(PyObject *self, PyObject *args) {
  PyObject *check = NULL;
  if(!PyArg_ParseTuple(args, "O", &check)) {
    PyErr_Format(PyExc_TypeError, "Error parsing new character see check.");
    return NULL;
  }

  if(pychar_see_checks == NULL)
    pychar_see_checks = newList();
  Py_INCREF(check);
  listPut(pychar_see_checks, check);

  return Py_BuildValue("O", Py_None);
}

PyObject *mudsys_register_obj_cansee(PyObject *self, PyObject *args) {
  PyObject *check = NULL;
  if(!PyArg_ParseTuple(args, "O", &check)) {
    PyErr_Format(PyExc_TypeError, "Error parsing new object see check.");
    return NULL;
  }

  if(pyobj_see_checks == NULL)
    pyobj_see_checks = newList();
  Py_INCREF(check);
  listPut(pyobj_see_checks, check);

  return Py_BuildValue("O", Py_None);
}

PyObject *mudsys_register_exit_cansee(PyObject *self, PyObject *args) {
  PyObject *check = NULL;
  if(!PyArg_ParseTuple(args, "O", &check)) {
    PyErr_Format(PyExc_TypeError, "Error parsing new exit see check.");
    return NULL;
  }

  if(pyexit_see_checks == NULL)
    pyexit_see_checks = newList();
  Py_INCREF(check);
  listPut(pyexit_see_checks, check);

  return Py_BuildValue("O", Py_None);
}



//*****************************************************************************
// functions for adding and getting types to the gameworld
//*****************************************************************************
void pytype_set_key(const char *type, PyObject *thing, const char *key) {
  PyObject *ret = PyObject_CallMethod(thing, "setKey", "s", key);
  if(ret == NULL)
    log_pyerr("error calling setKey for world type, %s", type);
  Py_XDECREF(ret);
}

PyObject *pytype_read(const char *type, STORAGE_SET *set) {
  PyObject *thing = PyDict_GetItemString(worldtypes, type);

  if(thing == NULL) {
    log_pyerr("world type, %s, does not exist", type);
    return NULL;
  }
  else {
    PyObject *pystore = newPyStorageSet(set);
    PyObject    *copy = PyObject_CallFunction(thing, "O", pystore);

    // did it work?
    if(copy == NULL)
      log_pyerr("Error reading world type, %s", type);
    Py_XDECREF(pystore);
    return copy;
  }
}

STORAGE_SET *pytype_store(const char *type, PyObject *thing) {
  PyObject *pystore = PyObject_CallMethod(thing, "store", NULL);
  STORAGE_SET  *set = NULL;
  // make sure it worked
  if(pystore != NULL)
    set = PyStorageSet_AsSet(pystore);
  else {
    log_pyerr("error calling store for world type, %s", type);
    set = new_storage_set();
  }
  Py_XDECREF(pystore);
  return set;
}

void pytype_delete(const char *type, PyObject *thing) {
  Py_XDECREF(thing);
}

PyObject *mudsys_world_add_type(PyObject *self, PyObject *args) {
  char      *type = NULL;
  PyObject *class = NULL;
  if(!PyArg_ParseTuple(args, "sO", &type, &class)) {
    PyErr_Format(PyExc_TypeError, "Error parsing new world type");
    return NULL;
  }

  // add all of our relevant methods to the game world
  worldAddForgetfulType(gameworld, type, pytype_read, pytype_store, 
			pytype_delete, pytype_set_key);

  // now, store us so we can look up the Python Methods
  PyDict_SetItemString(worldtypes, type, class);

  return Py_BuildValue("i", 1);
}

PyObject *mudsys_world_get_type(PyObject *self, PyObject *args) {
  char      *type = NULL;
  char       *key = NULL;

  if(!PyArg_ParseTuple(args, "ss", &type, &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "Error parsing type and key for world_get_type");
    return NULL;
  }

  PyObject *entry = worldGetType(gameworld, type, key);
  return Py_BuildValue("O", (type == NULL ? Py_None : entry));
}

PyObject *mudsys_world_put_type(PyObject *self, PyObject *args) {
  char      *type = NULL;
  char       *key = NULL;
  PyObject *entry = NULL;

  if(!PyArg_ParseTuple(args, "ssO", &type, &key, &entry)) {
    PyErr_Format(PyExc_TypeError, 
		 "Error parsing arguments for world_put_type");
    return NULL;
  }

  Py_XINCREF(entry);
  worldPutType(gameworld, type, key, entry);
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_world_save_type(PyObject *self, PyObject *args) {
  char      *type = NULL;
  char       *key = NULL;

  if(!PyArg_ParseTuple(args, "ss", &type, &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "Error parsing type and key for world_save_type");
    return NULL;
  }

  worldSaveType(gameworld, type, key);
  return Py_BuildValue("i", 1);
}

PyObject *mudsys_world_remove_type(PyObject *self, PyObject *args) {
  char      *type = NULL;
  char       *key = NULL;

  if(!PyArg_ParseTuple(args, "ss", &type, &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "Error parsing type and key for world_remove_type");
    return NULL;
  }

  PyObject *entry = worldRemoveType(gameworld, type, key);
  PyObject   *ret = Py_BuildValue("O", (entry == NULL ? Py_None : entry));
  Py_XDECREF(entry);
  return ret;
}

PyObject *mudsys_create_bitvector(PyObject *self, PyObject *args) {
  return Py_BuildValue("");
}

PyObject *mudsys_create_bit(PyObject *self, PyObject *args) {
  char *vector = NULL;
  char    *bit = NULL;

  if(!PyArg_ParseTuple(args, "ss", &vector, &bit)) {
    PyErr_Format(PyExc_TypeError, 
		 "Error parsing vector and bit for create_bit.");
    return NULL;
  }
  
  bitvectorAddBit(vector, bit);
  return Py_BuildValue("O", Py_True);
}

PyObject *mudsys_next_uid(PyObject *self, void *closure) {
  return Py_BuildValue("i", next_uid());
}


PyObject *mudsys_list_zone_contents(PyObject *self, PyObject *args) {
  char *zonekey = NULL;
  char    *type = NULL;

  if(!PyArg_ParseTuple(args, "ss", &zonekey, &type)) {
    PyErr_Format(PyExc_TypeError, 
		 "a zone name and content type must be supplied.");
    return NULL;
  }

  // make sure the zone exists
  ZONE_DATA *zone = worldGetZone(gameworld, zonekey);
  if(zone == NULL)
    return PyList_New(0);
  else {
    LIST           *keys = zoneGetTypeKeys(zone, type);
    LIST_ITERATOR *key_i = newListIterator(keys);
    PyObject        *ret = PyList_New(0);
    char            *key = NULL;
    ITERATE_LIST(key, key_i) {
      PyObject *str = Py_BuildValue("s", key);
      PyList_Append(ret, str);
      Py_XDECREF(str);
    } deleteListIterator(key_i);
    deleteListWith(keys, free);
    return ret;
  }
}

PyObject *mudsys_can_edit_zone(PyObject *self, PyObject *args) {
  PyObject  *pych = NULL;
  CHAR_DATA   *ch = NULL;
  ZONE_DATA *zone = NULL;
  char     *zname = NULL;

  if(!PyArg_ParseTuple(args, "Os", &pych, &zname)) {
    PyErr_Format(PyExc_TypeError,"can_edit_zone takes ch and zone name.");
    return NULL;
  }

  if(!PyChar_Check(pych)) {
    PyErr_Format(PyExc_TypeError,"Non-character supplied to can_edit_zone.");
    return NULL;
  }
  else if( (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_Format(PyExc_TypeError,"can_edit_zone supplied non-existent zone.");
    return NULL;
  }

  // find the zone and check
  zone = worldGetZone(gameworld, zname);
  if(zone == NULL || !canEditZone(zone, ch))
    return Py_BuildValue("i", 0);
  return Py_BuildValue("i", 1);
}



//*****************************************************************************
// MudSys module
//*****************************************************************************
void PyMudSys_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pymudsys_methods == NULL) pymudsys_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pymudsys_methods, def);
}


PyMODINIT_FUNC
init_PyMudSys(void) {
  // local variables
  py_move_checks = newList();
  dflt_move_cmds = newList();

  // add in hooks to the main MUD
  register_char_see(pycan_see_char);
  register_obj_see(pycan_see_obj);
  register_exit_see(pycan_see_exit);

  // add all of our methods
  PyMudSys_addMethod("do_shutdown", mudsys_shutdown, METH_VARARGS,
		     "do_shutdown()\n\n"
		     "shuts the mud down.");
  PyMudSys_addMethod("do_copyover", mudsys_copyover, METH_VARARGS,
		     "do_copyover()\n\n"
		     "performs a copyover on the mud.");
  PyMudSys_addMethod("sys_setval", mudsys_set_sys_val, METH_VARARGS,
		     "set_sysval(name, val)\n"
		     "\n"
		     "sets a value registered in the system settings.");
  PyMudSys_addMethod("sys_getval", mudsys_get_sys_val, METH_VARARGS,
		     "sys_getval(name)\n"
		     "\n"
		     "returns a value registered in the system settings.");
  PyMudSys_addMethod("sys_getvar", mudsys_get_sys_val, METH_VARARGS,
		     "Alias to mudsys.sys_getval");
  PyMudSys_addMethod("sys_setvar", mudsys_set_sys_val, METH_VARARGS,
		     "Alias to mudsys.sys_setval");
  PyMudSys_addMethod("player_exists", mudsys_player_exists, METH_VARARGS,
		     "player_exists(name)\n"
		     "\n"
		     "Returns whether a player with the name exists.");
  PyMudSys_addMethod("account_exists", mudsys_account_exists, METH_VARARGS,
		     "account_exists(name)\n"
		     "\n"
		     "Returns whether an account with the name exists.");
  PyMudSys_addMethod("player_creating", mudsys_player_creating, METH_VARARGS,
		     "player_creating(name)\n"
		     "\n"
		     "returns whether a player with the name is creating.");
  PyMudSys_addMethod("account_creating", mudsys_account_creating, METH_VARARGS,
		     "account_creating(name)\n"
		     "\n"
		     "returns whether an account with the name is creating.");
  PyMudSys_addMethod("do_register", mudsys_do_register, METH_VARARGS,
		     "do_register(char_or_account)\n"
		     "\n"
		     "Register a PC or account for the first time. Should be called after creation.");
  PyMudSys_addMethod("get_player", mudsys_load_char, METH_VARARGS,
		     "get_player(name)\n"
		     "\n"
		     "Return a saved character of specified name, or None.");
  PyMudSys_addMethod("load_char", mudsys_load_char, METH_VARARGS,
		     "Alias for mudsys.get_player(name).");
  PyMudSys_addMethod("load_account", mudsys_load_account, METH_VARARGS,
		     "load_account(name)\n"
		     "\n"
		     "Return a saved account of specified name, or None.");
  PyMudSys_addMethod("try_enter_game", mudsys_try_enter_game, METH_VARARGS,
		     "try_enter_game(ch)\n"
		     "\n"
		     "Tries to add a character the game world.");
  PyMudSys_addMethod("do_save", mudsys_do_save, METH_VARARGS,
		     "do_save(char_or_account)\n"
		     "\n"
		     "Save a character or account's information.");
  PyMudSys_addMethod("do_quit", mudsys_do_quit, METH_VARARGS,
		     "do_quit(ch)\n"
		     "\n"
		     "Extract a character from game.");
  PyMudSys_addMethod("attach_account_socket",mudsys_attach_account_socket, 
		     METH_VARARGS, 
		     "attach_account_socket(acct, sock)\n"
		     "\n"
		     "Link a loaded account to a connected socket.");
  PyMudSys_addMethod("attach_char_socket", mudsys_attach_char_socket, 
		     METH_VARARGS,
		     "attach_char_socket(ch, sock)\n"
		     "\n"
		     "Link a loaded character to a connected socket.");
  PyMudSys_addMethod("detach_char_socket", mudsys_detach_char_socket,
		     METH_VARARGS,
		     "detach_char_socket(ch)\n"
		     "\n"
		     "Unlink a character from its attached socket.");
  PyMudSys_addMethod("do_disconnect", mudsys_do_disconnect, METH_VARARGS,
		     "do_disconnect(ch)\n"
		     "\n"
		     "call detach_char_socket, then close the socket.");
  PyMudSys_addMethod("password_matches", mudsys_password_matches, METH_VARARGS,
		     "password_matches(acct, psswd)\n"
		     "\n"
		     "Returns True or False if the given password matches the account's password.");
  PyMudSys_addMethod("set_password", mudsys_set_password, METH_VARARGS,
		     "set_password(acct, passwd)\n"
		     "\n"
		     "Set an account's password.");
  PyMudSys_addMethod("add_cmd", mudsys_add_cmd, METH_VARARGS,
    "add_cmd(name, shorthand, cmd_func, user_group, interrupts_action)\n"
    "\n"
    "Add a new command to the master command table. If a preferred shorthand\n"
    "exists, e.g., 'n' for 'north', it can be specified. Otherwise, shorthand\n"
    "should be None. Command functions take three arguments: a character\n"
    "issuing the command, the command name, and a string argument supplied\n"
    "to the command. Commands must be tied to a specific user group, and they\n"
    "can optionally interupt character actions.");
  PyMudSys_addMethod("add_cmd_check", mudsys_add_cmd_check, METH_VARARGS,
    "add_cmd_check(name, check_func)\n"
    "\n"
    "Add a new command check to a registered command. Check functions take\n"
    "two arguments: the character issuing the command, and the command name.\n"
    "If a check fails, it should return False and send the character a\n"
    "message why.");
  PyMudSys_addMethod("remove_cmd", mudsys_remove_cmd, METH_VARARGS,
    "remove_cmd(name)\n"
    "\n"
    "Removes a command from the master command table.");
  PyMudSys_addMethod("handle_cmd_input", mudsys_handle_cmd_input, METH_VARARGS,
    "handle_cmd_input(sock, cmd)\n"
    "\n"
    "Equivalent to char.Char.act(cmd)");
  PyMudSys_addMethod("show_prompt", mudsys_show_prompt, METH_VARARGS,
    "show_prompt(sock)\n"
    "\n"
    "Display the default game prompt to the socket. Can be replaced in Python\n"
    "by assigning a new function to show_prompt.");
  PyMudSys_addMethod("create_account", mudsys_create_account, METH_VARARGS,
    "create_account(acctname)\n"
    "\n"
    "Returns a new account by the specified name, or None if an account by.\n"
    "the specified name is already registered or creating. After the account" 
    "generation process is complete, mudsys.do_register(acct) must be called.");
  PyMudSys_addMethod("create_player", mudsys_create_player, METH_VARARGS,
    "Same as mudsys.create_account for player characters.");
  PyMudSys_addMethod("add_sock_method", mudsys_add_sock_method, METH_VARARGS,
    "Same as add_acct_method for sockets.");
  PyMudSys_addMethod("add_char_method", mudsys_add_char_method, METH_VARARGS,
    "Same as add_acct_method for characters.");
  PyMudSys_addMethod("add_room_method", mudsys_add_room_method, METH_VARARGS,
    "Same as add_acct_method for rooms.");
  PyMudSys_addMethod("add_exit_method",  mudsys_add_exit_method, METH_VARARGS,
    "Same as add_acct_method for exits.");
  PyMudSys_addMethod("add_obj_method",  mudsys_add_obj_method, METH_VARARGS,
    "Same as add_acct_method for objects.");
  PyMudSys_addMethod("add_acct_method",  mudsys_add_acct_method, METH_VARARGS,
    "add_acct_method(name, method)\n"
    "\n"
    "Adds a function or property to the Account class.");
  PyMudSys_addMethod("world_add_type", mudsys_world_add_type, METH_VARARGS,
    "world_add_type(typename, class_data)\n"
    "\n"
    "Registers a new type to the world database. Like, e.g., mob, obj, and\n"
    "room prototypes. Assumes class has a store and setKey method. Init\n"
    "method should take one optional argument: a storage set to parse the\n"
    "type data from, when loaded.");
  PyMudSys_addMethod("world_get_type", mudsys_world_get_type, METH_VARARGS,
    "world_get_type(typename, key)\n"
    "\n"
    "Returns registered entry of the specified type from the world database.\n"
    "Assumes it is a python type, and not a C type. If no type exists\n"
    "return None.");
  PyMudSys_addMethod("world_put_type", mudsys_world_put_type, METH_VARARGS,
    "world_put_type(typename, key, data)\n"
    "\n"
    "Put and save an entry of the specified type to the world database.");
  PyMudSys_addMethod("world_save_type", mudsys_world_save_type, METH_VARARGS,
    "world_save_type(typename, key)\n"
    "\n"
    "Saves an entry in the world database if it exists.");
  PyMudSys_addMethod("world_remove_type", mudsys_world_remove_type,METH_VARARGS,
    "world_remove_type(typename, key)\n"
    "\n"
    "Remove and return an entry from the world database, or None.");
  PyMudSys_addMethod("register_char_cansee", mudsys_register_char_cansee,
    METH_VARARGS,
   "register_char_cansee(check_function(observer, observee))\n"
   "\n"
   "Register a new check of whether one character can see another.");
  PyMudSys_addMethod("register_obj_cansee", mudsys_register_obj_cansee,
    METH_VARARGS, "Same as register_char_cansee for objects.");
  PyMudSys_addMethod("register_exit_cansee", mudsys_register_exit_cansee,
    METH_VARARGS, "Same as register_char_cansee for exits.");
  PyMudSys_addMethod("set_cmd_move", mudsys_set_cmd_move, METH_VARARGS,
    "set_cmd_move(cmd_func)\n"
    "\n"
    "Register a player command for handling all default movement commands.\n"
    "See mudsys.add_cmd for information about commands.");
  PyMudSys_addMethod("register_dflt_move_cmd", mudsys_register_dflt_move_cmd, 
    METH_VARARGS,
    "register_dflt_move_cmd(cmdname)\n"
    "\n" 
    "registers a new default movement command, e.g., north");
  PyMudSys_addMethod("register_move_check",mudsys_register_move_check,
    METH_VARARGS,
    "register_move_check(check_func)\n"
    "\n"
    "Register a check to perform movement commands. See mudsys.add_cmd_check\n"
    "for information about command checks.");
  PyMudSys_addMethod("create_bitvector", mudsys_create_bitvector,
    METH_VARARGS, "Not yet implemented."); 
  PyMudSys_addMethod("create_bit", mudsys_create_bit,
    METH_VARARGS,
    "create_bit(bitvector, bit)\n"
    "\n"
    "Creates a new bit on the specified bitvector."); 
  PyMudSys_addMethod("next_uid", mudsys_next_uid, METH_NOARGS,
    "next_uid()\n\n"
    "Returns the next available universal identification number.");
  PyMudSys_addMethod("list_zone_contents", mudsys_list_zone_contents, 
    METH_VARARGS,
    "list_zone_contents(zone, type)\n\n" 
    "Returns a list of the content keys of the given type, for the specified\n"
    "zone.");
  PyMudSys_addMethod("can_edit_zone", mudsys_can_edit_zone, METH_VARARGS,
    "can_edit_zone(ch, zone)\n\n"
    "True or False if a character has permission to edit a zone.");

  Py_InitModule3("mudsys", makePyMethods(pymudsys_methods),
		 "The mudsys module, for all MUD system utils.");

  worldtypes = PyDict_New();
  Py_INCREF(worldtypes);
}

void *get_cmd_move(void) {
  return py_cmd_move;
}

LIST *get_move_checks(void) {
  return py_move_checks;
}
