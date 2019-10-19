#ifndef PYEXIT_H
#define PYEXIT_H
//*****************************************************************************
//
// pyexit.h
//
// Contains a python exit module, and an Exit class that is a python wrapper
// for NakedMud exits. If you wish to give python access to more features
// of an exit, it should NOT be done by editing pyexit.c! Use 
// PyExit_addGetSetter and PyExit_addMethod in a new module implementing the 
// feature you want to give Python access to.
//
//*****************************************************************************

// used by pymudsys. Do not touch!
PyTypeObject PyExit_Type;

// initialize exits for use. This must be called AFTER all other modules
// have added in new get/setters and methods to pyroom
PyMODINIT_FUNC init_PyExit(void);
PyObject        *newPyExit(EXIT_DATA *exit);

EXIT_DATA   *PyExit_AsExit(PyObject *exit);
int           PyExit_AsUid(PyObject *exit);

//
// checks to see if the PyObject is a PyExit
int          PyExit_Check(PyObject *value);

//
// getters allow Python to access pieces of the Exit module. Setters allow
// Python to change pieces of the exit module. Getters are called when Python
// tries to get the value of some variable on the object, and setters are called
// when Python tries to set the value of some variable on the object. Get and
// Set do not both need to be supplied. Examples of how to add new getters and
// setters is presented in pyexit.c
void PyExit_addGetSetter(const char *name, void *g, void *s, const char *doc);

//
// Adds a new method function (i.e. void *f) to the Exit class. Name is the name
// of the function, f is the PyCFunction implementing the new method, flags is
// the type of method beings used (almost always METH_VARARGS), and dog is an
// (optional) description of what the method does. For examples on how to add
// new methods, see pyexit.c
void PyExit_addMethod(const char *name, void *f, int flags, const char *doc);

#endif // PYEXIT_H
