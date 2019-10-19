//*****************************************************************************
//
// command.c
//
// A structure that holds all of the data tied to player commands. 
// Specifically, the function that is executed and all of the restrictions on
// its execution.
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "mud.h"
#include "utils.h"
#include "action.h"
#include "character.h"



//*****************************************************************************
// mandatory modules
//*****************************************************************************
#include "scripts/scripts.h"
#include "scripts/pyplugs.h"
#include "scripts/pychar.h"



//*****************************************************************************
// local structs, defines, and functions
//*****************************************************************************
struct cmd_data {
  char       *name;
  CMD_PTR(func);
  PyObject *pyfunc;
  char *user_group;
  bool  interrupts;
  LIST     *checks;
};

typedef struct cmd_check_data {
  CMD_CHK_PTR(func);
  PyObject *pyfunc;
} CMD_CHK_DATA;



//*****************************************************************************
// functions for manipulating CMD_CHK_DATA
//*****************************************************************************
CMD_CHK_DATA *newCmdCheck(CMD_CHK(func)) {
  CMD_CHK_DATA *data = malloc(sizeof(CMD_CHK_DATA));
  data->func   = func;
  data->pyfunc = NULL;
  return data;
}

CMD_CHK_DATA *newPyCmdCheck(PyObject *pyfunc) {
  CMD_CHK_DATA *data = malloc(sizeof(CMD_CHK_DATA));
  data->func   = NULL;
  data->pyfunc = pyfunc;
  Py_XINCREF(data->pyfunc);
  return data;
}

void deleteCmdCheck(CMD_CHK_DATA *data) {
  if(data->pyfunc) { Py_XDECREF(data->pyfunc); }
  free(data);
}

CMD_CHK_DATA *cmdCheckCopy(CMD_CHK_DATA *data) {
  if(data->func)
    return newCmdCheck(data->func);
  else
    return newPyCmdCheck(data->pyfunc);
}

void cmdCheckCopyTo(CMD_CHK_DATA *from, CMD_CHK_DATA *to) {
  Py_XDECREF(to->pyfunc);
  to->func   = from->func;
  to->pyfunc = from->pyfunc;
  Py_XINCREF(to->pyfunc);
}



//*****************************************************************************
// implementation of command.h
//*****************************************************************************
CMD_DATA *newCmd(const char *name, COMMAND(func), const char *user_group, 
		 bool interrupts) {
  CMD_DATA *cmd = calloc(1, sizeof(CMD_DATA));
  cmd->name     = strdupsafe(name);
  cmd->checks   = newList();
  cmdUpdate(cmd, func, user_group, interrupts);
  return cmd;
}

CMD_DATA *newPyCmd(const char *name, void *pyfunc, const char *user_group, 
		   bool interrupts) {
  CMD_DATA *cmd   = calloc(1, sizeof(CMD_DATA));
  cmd->name       = strdupsafe(name);
  cmd->checks     = newList();
  cmdPyUpdate(cmd, pyfunc, user_group, interrupts);
  return cmd;
}

void cmdUpdate(CMD_DATA *cmd, COMMAND(func), const char *user_group, 
	       bool interrupts) {
  if(cmd->pyfunc)     { Py_DECREF(cmd->pyfunc); cmd->pyfunc = NULL; }
  if(cmd->user_group) free(cmd->user_group);
  cmd->func       = func;
  cmd->user_group = strdupsafe(user_group);
  cmd->interrupts = interrupts;
}

void cmdPyUpdate(CMD_DATA *cmd, void *pyfunc, const char *user_group,
		 bool interrupts) {
  if(cmd->pyfunc)     { Py_DECREF(cmd->pyfunc); cmd->pyfunc = NULL; }
  if(cmd->user_group) free(cmd->user_group);
  cmd->func       = NULL;
  cmd->user_group = strdupsafe(user_group);
  cmd->interrupts = interrupts;
  cmd->pyfunc     = pyfunc;
  Py_XINCREF(cmd->pyfunc);
}

void deleteCmd(CMD_DATA *cmd) {
  if(cmd->name)       free(cmd->name);
  if(cmd->user_group) free(cmd->user_group);
  if(cmd->pyfunc)     { Py_DECREF(cmd->pyfunc); }
  if(cmd->checks)     deleteListWith(cmd->checks, deleteCmdCheck);
  free(cmd);
}

CMD_DATA *cmdCopy(CMD_DATA *cmd) {
  CMD_DATA *newcmd = NULL;
  if(cmd->pyfunc)
    newcmd = newPyCmd(cmd->name, cmd->pyfunc, cmd->user_group, cmd->interrupts);
  else
    newcmd = newCmd(cmd->name, cmd->func, cmd->user_group, cmd->interrupts);
  
  // copy over the checks
  deleteList(newcmd->checks);
  newcmd->checks = listCopyWith(cmd->checks, cmdCheckCopy);
  return newcmd;
}

void cmdCopyTo(CMD_DATA *from, CMD_DATA *to) {
  if(to->name)       free(to->name);
  if(to->user_group) free(to->user_group);
  if(to->pyfunc)     { Py_DECREF(to->pyfunc); }
  to->name         = strdup(from->name);
  to->user_group   = strdup(from->user_group);
  to->pyfunc       = from->pyfunc;
  if(to->pyfunc)     { Py_INCREF(to->pyfunc); }
  to->func         = from->func;
  to->interrupts   = from->interrupts;
  if(to->checks)     { deleteListWith(to->checks, deleteCmdCheck); }
  to->checks       = listCopyWith(from->checks, cmdCheckCopy);
}

const char *cmdGetName(CMD_DATA *cmd) {
  return cmd->name;
}

const char *cmdGetUserGroup(CMD_DATA *cmd) {
  return cmd->user_group;
}

bool cmdGetInterrupts(CMD_DATA *cmd) {
  return cmd->interrupts;
}

LIST *cmdGetChecks(CMD_DATA *cmd) {
  return cmd->checks;
}

void cmdAddCheck(CMD_DATA *cmd, CMD_CHK(func)) {
  listPut(cmdGetChecks(cmd), newCmdCheck(func));
}

void cmdAddPyCheck(CMD_DATA *cmd, void *pyfunc) {
  listPut(cmdGetChecks(cmd), newPyCmdCheck(pyfunc));
}

//
// make sure the character is in a position where this can be performed
bool min_pos_ok(CHAR_DATA *ch, int minpos) {
  if(poscmp(charGetPos(ch), minpos) >= 0)
    return TRUE;
  else {
    switch(charGetPos(ch)) {
    case POS_UNCONSCIOUS:
      send_to_char(ch, "You cannot do that while unconscious!\r\n");
      break;
    case POS_SLEEPING:
      send_to_char(ch, "Not while sleeping, you won't!\r\n");
      break;
    case POS_SITTING:
      send_to_char(ch, "You cannot do that while sitting!\r\n");
      break;
    case POS_STANDING:
      // flying is the highest position... we can deduce this message
      send_to_char(ch, "You must be flying to try that.\r\n");
      break;
    case POS_FLYING:
      send_to_char(ch, "That is not possible in any position you can think of.\r\n");
      break;
    default:
      send_to_char(ch, "Your position is all wrong!\r\n");
      log_string("Character, %s, has invalid position, %d.",
		 charGetName(ch), charGetPos(ch));
      break;
    }
    return FALSE;
  }
}


//
// make sure the character is in a position where this can be performed
bool max_pos_ok(CHAR_DATA *ch, int minpos) {
  if(poscmp(charGetPos(ch), minpos) <= 0)
    return TRUE;
  else {
    switch(charGetPos(ch)) {
    case POS_UNCONSCIOUS:
      send_to_char(ch, "You're still too alive to try that!\r\n");
      break;
    case POS_SLEEPING:
      send_to_char(ch, "Not while sleeping, you won't!\r\n");
      break;
    case POS_SITTING:
      send_to_char(ch, "You cannot do that while sitting!\r\n");
      break;
    case POS_STANDING:
      send_to_char(ch, "You cannot do that while standing.\r\n");
      break;
    case POS_FLYING:
      send_to_char(ch, "You must land first.\r\n");
      break;
    default:
      send_to_char(ch, "Your position is all wrong!\r\n");
      log_string("Character, %s, has invalid position, %d.",
		 charGetName(ch), charGetPos(ch));
      break;
    }
    return FALSE;
  }
}

bool cmdTryChecks(CHAR_DATA *ch, CMD_DATA *cmd) {
  bool cmd_ok = TRUE;
  if(listSize(cmd->checks) > 0) {
    LIST_ITERATOR *chk_i = newListIterator(cmd->checks);
    CMD_CHK_DATA    *chk = NULL;
    ITERATE_LIST(chk, chk_i) {
      if(chk->func)
	cmd_ok = (chk->func)(ch, cmd->name);
      else {
	PyObject *arglist = Py_BuildValue("Os", charGetPyFormBorrowed(ch), 
					  cmd->name);
	PyObject *retval  = PyEval_CallObject(chk->pyfunc, arglist);
	// check for an error:
	if(retval == NULL)
	  log_pyerr("Error running Python command check, %s:", cmd->name);
	else if(retval == Py_False)
	  cmd_ok = FALSE;

	// garbage collection
	Py_XDECREF(retval);
	Py_XDECREF(arglist);
      }

      if(cmd_ok == FALSE)
	break;
    } deleteListIterator(chk_i);
  }
  return cmd_ok;
}

bool cmdHasFunc(CMD_DATA *cmd) {
  return (cmd->func != NULL || cmd->pyfunc != NULL);
}

int charTryCmd(CHAR_DATA *ch, CMD_DATA *cmd, char *arg) {
  // first, go through all of our checks
  if(!cmdTryChecks(ch, cmd))
    return FALSE;
  /*
  if(!min_pos_ok(ch, cmd->min_pos) || !max_pos_ok(ch,cmd->max_pos) ||
     (charIsNPC(ch) && !cmd->mob_ok))
    return FALSE;
  */
  else {
    if(cmd->interrupts) {
#ifdef MODULE_FACULTY
      interrupt_action(ch, FACULTY_ALL);
#else
      interrupt_action(ch, 1);
#endif
    }
    if(cmd->func) {
      (cmd->func)(ch, cmd->name, arg);
      return TRUE;
    }
    else if(cmd->pyfunc) {
      PyObject *arglist = Py_BuildValue("Oss", charGetPyFormBorrowed(ch), 
					cmd->name, arg);
      PyObject *retval  = PyEval_CallObject(cmd->pyfunc, arglist);
      // check for an error:
      if(retval == NULL)
	log_pyerr("Error running Python command, %s:", cmd->name);

      // garbage collection
      Py_XDECREF(retval);
      Py_XDECREF(arglist);
      return TRUE;
    }
    // command is null (but there might have been checks)
    else
      return -1;
  }
}

CMD_CHK(chk_can_move) {
  return min_pos_ok(ch, POS_STANDING);
}

CMD_CHK(chk_conscious) {
  return min_pos_ok(ch, POS_SITTING);
}

CMD_CHK(chk_grounded) {
  return max_pos_ok(ch, POS_STANDING);
}

CMD_CHK(chk_not_mob) {
  if(charIsNPC(ch))
    send_to_char(ch, "NPCs may not use this command.\r\n");
  return !charIsNPC(ch);
}
