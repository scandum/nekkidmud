#ifndef PYSTORAGE_H
#define PYSTORAGE_H
//*****************************************************************************
//
// pystorage.c
//
// Provides a wrapper around NakedMud's storage system for use by scripts
// and python modules.
//
//*****************************************************************************

//
// initialize the pystorage system for use
PyMODINIT_FUNC init_PyStorage(void);

//
// create a new python representation of the storage set
PyObject *newPyStorageSet (STORAGE_SET *set);

//
// return the storage set that is contained within it.
STORAGE_SET *PyStorageSet_AsSet(PyObject *set);

//
// checks to see of the object is a PyStorageSet
int PyStorageSet_Check(PyObject *value);

#endif // PYSTORAGE_H
