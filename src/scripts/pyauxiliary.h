#ifndef PYAUXILIARY_H
#define PYAUXILIARY_H
//*****************************************************************************
//
// pyauxiliary.h
//
// This is a wrapper around the auxiliary data system so python can make use
// of it. Allows python modules to install new auxiliary data on characters,
// rooms, objects, etc...
//
//*****************************************************************************

// initialize python auxiliary data for use
PyMODINIT_FUNC init_PyAuxiliary(void);

// returns TRUE if auxiliary data of the given name has been installed by python
bool pyAuxiliaryDataExists(const char *name);

#endif // PYAUXILIARY_H
