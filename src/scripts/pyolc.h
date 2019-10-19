#ifndef __PYOLC_H
#define __PYOLC_H
//*****************************************************************************
//
// pyolc.h
//
// A set of system level commands and variables that may be needed by python,
// but which are not neccessarily needed by scripts.
//
//*****************************************************************************

// init the Python olc module for use
PyMODINIT_FUNC init_PyOLC(void);

//
// Adds a new method function (i.e. void *f) to the olc module. Name is the name
// of the function, f is the PyCFunction implementing the new method, flags is
// the type of method beings used (almost always METH_VARARGS), and doc is an
// (optional) description of what the method does. For examples on how to add
// new methods, see pymud.c
void PyOLC_addMethod(const char *name, void *f, int flags, const char *doc);

#endif //__PYOLC_H
