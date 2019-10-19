#ifndef __PYMUDSYS_H
#define __PYMUDSYS_H
//*****************************************************************************
//
// pymudsys.h
//
// A set of system level commands and variables that may be needed by python,
// but which are not neccessarily needed by scripts.
//
//*****************************************************************************

/* initialize mud sys module for use */
PyMODINIT_FUNC
init_PyMudSys(void);

//
// Adds a new method function (i.e. void *f) to the sys module. Name is the name
// of the function, f is the PyCFunction implementing the new method, flags is
// the type of method beings used (almost always METH_VARARGS), and doc is an
// (optional) description of what the method does. For examples on how to add
// new methods, see pymud.c
void PyMudSys_addMethod(const char *name, void *f, int flags, const char *doc);

//
// returns the current defined cmd_move
void *get_cmd_move(void);

//
// returns a list of checks that must be performed before moving
LIST *get_move_checks(void);

#endif //__PYMUD_H
