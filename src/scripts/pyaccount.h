#ifndef PYACCOUNT_H
#define PYACCOUNT_H
//*****************************************************************************
//
// pyaccount.h
//
// Contains a python account module, and an Account class that is a python 
// wrapper for NakedMud accounts. If you wish to give python access to more
// features of an account, it should NOT be done by editing pyaccount.c! Use 
// PyAccount_addGetSetter and PyAccount_addMethod in a new module implementing 
// the feature you want to give Python access to.
//
//*****************************************************************************

// used by pymudsys. Do not touch!
PyTypeObject PyAccount_Type;

// initialize accounts for use. This must be called AFTER all other modules
// have added in new get/setters and methods to pyroom
PyMODINIT_FUNC     init_PyAccount(void);
PyObject            *newPyAccount(ACCOUNT_DATA *account);
ACCOUNT_DATA *PyAccount_AsAccount(PyObject *account);

//
// checks to see if the PyObject is a PyAccount
int          PyAccount_Check(PyObject *value);

//
// getters allow Python to access pieces of the Account module. Setters allow
// Python to change pieces of the account module. Getters are called when Python
// tries to get the value of some variable on the object, and setters are called
// when Python tries to set the value of some variable on the object. Get and
// Set do not both need to be supplied. Examples of how to add new getters and
// setters is presented in pyaccount.c
void PyAccount_addGetSetter(const char *name, void *g, void *s, const char *doc);

//
// Adds a new method function (i.e. void *f) to the Account class. Name is the 
// name of the function, f is the PyCFunction implementing the new method, 
// flags is the type of method beings used (almost always METH_VARARGS), and
// dog is an (optional) description of what the method does. For examples on
// how to add new methods, see pyaccount.c
void PyAccount_addMethod(const char *name, void *f, int flags, const char *doc);

#endif // PYACCOUNT_H
