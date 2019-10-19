#ifndef __PYMUD_H
#define __PYMUD_H
//*****************************************************************************
//
// pymud.h
//
// a python module that provides some useful utility functions for interacting
// with the MUD. Includes stuff like global variables, messaging functions,
// and a whole bunch of other stuff. Browse pymud.c to see.
//
//*****************************************************************************

/* initialize mud module for use */
PyMODINIT_FUNC
init_PyMud(void);

//
// Adds a new method function (i.e. void *f) to the Mud module. Name is the name
// of the function, f is the PyCFunction implementing the new method, flags is
// the type of method beings used (almost always METH_VARARGS), and doc is an
// (optional) description of what the method does. For examples on how to add
// new methods, see pymud.c
void PyMud_addMethod(const char *name, void *f, int flags, const char *doc);

#endif //__PYMUD_H
