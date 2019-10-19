#ifndef PYPLUGS_H
#define PYPLUGS_H
//*****************************************************************************
//
// pyplugs.h
//
// Pyplugs are just various things that occur at the intersection between 
// Python and C, in NakedMud. They include the loading of python modules,
// a few commands for interacting with python modules, and miscellaneory (sp?)
// python stuff that doesn't neccessarily fall under "scripting".
//
//*****************************************************************************

// 
// Returns a string representation of the last error that Python threw. Must
// be freed afterwards.
char *getPythonTraceback(void);

//
// If we've enountered an error in python, log the traceback along
// with the specified message
void log_pyerr(const char *mssg, ...)
__attribute__ ((format (printf, 1, 2)));

//
// initialize all of our plugs with python
void init_pyplugs();

//
// Takes in a PyType, and adds lists of get/setters and methods to it. The
// lists can each be NULL if there are no getsetters or methods, respectively
void makePyType(PyTypeObject *type, LIST *getsetters, LIST *methods);

//
// makes an array of get/setters
PyGetSetDef *makePyGetSetters(LIST *getsetters);

//
// makes an array of python methods
PyMethodDef *makePyMethods(LIST *methods);

#endif // PYPLUGS_H
