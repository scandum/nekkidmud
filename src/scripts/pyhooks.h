#ifndef PYHOOKS_H
#define PYHOOKS_H
//*****************************************************************************
//
// pyhooks.h
//
// The wrapper around hooks for Python to interact with them.
//
//*****************************************************************************

/* initialize mud module for use */
PyMODINIT_FUNC
init_PyHooks(void);

//
// Adds a new method function (i.e. void *f) to the hooks module. Name is the 
// name of the function, f is the PyCFunction implementing the new method, 
// flags is the type of method beings used (almost always METH_VARARGS), and 
// doc is an (optional) description of what the method does. For examples on 
// how to add new methods, see pyhooks.c
void PyHooks_addMethod(const char *name, void *f, int flags, const char *doc);

#endif // PYHOOKS_H
