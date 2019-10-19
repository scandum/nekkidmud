#ifndef __PYOBJ_H
#define __PYOBJ_H
//*****************************************************************************
//
// pyobj.h
//
// A python extention to allow python scripts to treat MUD obj as an
// object within the script. If you wish to give python access to more features
// of an obj, it should NOT be done by editing pyobj.c! Use PyObj_addGetSetter
// and PyObj_addMethod in a new module implementing the feature you want to
// give Python access to.
//
//*****************************************************************************

// used by pymudsys. Do not touch!
PyTypeObject PyObj_Type;

// initialize objects for use. This must be called AFTER all other modules
// have added in new get/setters and methods to pyobj
PyMODINIT_FUNC init_PyObj(void);
PyObject      *newPyObj(OBJ_DATA *obj);

OBJ_DATA   *PyObj_AsObj(PyObject *obj);
int         PyObj_AsUid(PyObject *obj);

//
// checks to see of the object is a PyObj
int         PyObj_Check(PyObject *value);

//
// getters allow Python to access pieces of the Obj module. Setters allow
// Python to change pieces of the obj module. Getters are called when Python
// tries to get the value of some variable on the object, and setters are called
// when Python tries to set the value of some variable on the object. Get and
// Set do not both need to be supplied. Examples of how to add new getters and
// setters is presented in pyobj.c
void PyObj_addGetSetter(const char *name, void *g, void *s, const char *doc);

//
// Adds a new method function (i.e. void *f) to the Obj class. Name is the name
// of the function, f is the PyCFunction implementing the new method, flags is
// the type of method beings used (almost always METH_VARARGS), and dog is an
// (optional) description of what the method does. For examples on how to add
// new methods, see pyobj.c
void PyObj_addMethod(const char *name, void *f, int flags, const char *doc);

#endif //__PYOBJ_H
