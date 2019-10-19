#ifndef PYSOCKET_H
#define PYSOCKET_H
//*****************************************************************************
//
// pysocket.h
//
// Contains a python socket module, and an Socket class that is a python 
// wrapper for NakedMud sockets. If you wish to give python access to more
// features of an socket, it should NOT be done by editing pysocket.c! Use 
// PySocket_addGetSetter and PySocket_addMethod in a new module implementing 
// the feature you want to give Python access to.
//
//*****************************************************************************

// used by pymudsys. Do not touch!
PyTypeObject PySocket_Type;

// initialize sockets for use. This must be called AFTER all other modules
// have added in new get/setters and methods to pyroom
PyMODINIT_FUNC   init_PySocket(void);
PyObject          *newPySocket(SOCKET_DATA *sock);
SOCKET_DATA *PySocket_AsSocket(PyObject *sock);
int             PySocket_AsUid(PyObject *sock);

//
// checks to see if the PyObject is a PySocket
int PySocket_Check(PyObject *value);


//
// getters allow Python to access pieces of the Socket module. Setters allow
// Python to change pieces of the socket module. Getters are called when Python
// tries to get the value of some variable on the object, and setters are called
// when Python tries to set the value of some variable on the object. Get and
// Set do not both need to be supplied. Examples of how to add new getters and
// setters is presented in pysocket.c
void PySocket_addGetSetter(const char *name, void *g, void *s, const char *doc);

//
// Adds a new method function (i.e. void *f) to the Socket class. Name is the 
// name of the function, f is the PyCFunction implementing the new method, 
// flags is the type of method beings used (almost always METH_VARARGS), and
// dog is an (optional) description of what the method does. For examples on
// how to add new methods, see pysocket.c
void PySocket_addMethod(const char *name, void *f, int flags, const char *doc);

#endif // PYSOCKET_H
