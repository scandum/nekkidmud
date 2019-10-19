#ifndef __PYCHAR_H
#define __PYCHAR_H
//*****************************************************************************
//
// pychar.h
//
// A python extention to allow python scripts to treat MUD chars as an
// object within the script. If you wish to give python access to more features
// of a char, it should NOT be done by editing pychar.c! Use PyChar_addGetSetter
// and PyChar_addMethod in a new module implementing the feature you want to
// give Python access to.
//
//*****************************************************************************

// used by pymudsys. Do not touch!
PyTypeObject PyChar_Type;

// initialize rooms for use. This must be called AFTER all other modules
// have added in new get/setters and methods to pyroom
PyMODINIT_FUNC init_PyChar(void);
PyObject        *newPyChar(CHAR_DATA *ch);

CHAR_DATA   *PyChar_AsChar(PyObject *ch);
int           PyChar_AsUid(PyObject *ch);

//
// checks to see if the PyObject is a PyChar
int PyChar_Check(PyObject *value);

//
// getters allow Python to access pieces of the Char module. Setters allow
// Python to change pieces of the char module. Getters are called when Python
// tries to get the value of some variable on the object, and setters are called
// when Python tries to set the value of some variable on the object. Get and
// Set do not both need to be supplied. Examples of how to add new getters and
// setters is presented in pychar.c
void PyChar_addGetSetter(const char *name, void *g, void *s, const char *doc);

//
// Adds a new method function (i.e. void *f) to the Char class. Name is the name
// of the function, f is the PyCFunction implementing the new method, flags is
// the type of method beings used (almost always METH_VARARGS), and dog is an
// (optional) description of what the method does. For examples on how to add
// new methods, see pychar.c
void PyChar_addMethod(const char *name, void *f, int flags, const char *doc);

#endif //__PYCHAR_H
