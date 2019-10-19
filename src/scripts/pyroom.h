#ifndef __PYROOM_H
#define __PYROOM_H
//*****************************************************************************
//
// pyroom.h
//
// A python extention to allow python scripts to treat MUD rooms as an
// object within the script. If you wish to give python access to more features
// of a room, it should NOT be done by editing pyroom.c! Use PyRoom_addGetSetter
// and PyRoom_addMethod in a new module implementing the feature you want to
// give Python access to.
//
//*****************************************************************************

// used by pymudsys. Do not touch!
PyTypeObject PyRoom_Type;

// initialize rooms for use. This must be called AFTER all other modules
// have added in new get/setters and methods to pyroom
PyMODINIT_FUNC init_PyRoom(void);
PyObject      *newPyRoom(ROOM_DATA *room);

ROOM_DATA *PyRoom_AsRoom(PyObject *room);
int        PyRoom_AsUid(PyObject *room);

//
// checks to see if the PyObject is a PyRoom
int        PyRoom_Check (PyObject *value);

//
// getters allow Python to access pieces of the Room module. Setters allow
// Python to change pieces of the room module. Getters are called when Python
// tries to get the value of some variable on the object, and setters are called
// when Python tries to set the value of some variable on the object. Get and
// Set do not both need to be supplied. Examples of how to add new getters and
// setters is presented in pyroom.c
void PyRoom_addGetSetter(const char *name, void *g, void *s, const char *doc);

//
// Adds a new method function (i.e. void *f) to the Room class. Name is the name
// of the function, f is the PyCFunction implementing the new method, flags is
// the type of method beings used (almost always METH_VARARGS), and dog is an
// (optional) description of what the method does. For examples on how to add
// new methods, see pyroom.c
void PyRoom_addMethod(const char *name, void *f, int flags, const char *doc);

#endif //__PYROOM_H
