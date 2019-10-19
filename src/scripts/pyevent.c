//*****************************************************************************
//
// pyevent.c
//
// pyevent gives Python access to NakedMud's event handler in a limited form.
// Unlike normal events which can have owners of any sort, python events can
// only have owners that are characters, objects, or rooms. Other than that,
// python events are identical to normal events.
//
//*****************************************************************************

#include <Python.h>

#include "../mud.h"
#include "../event.h"

#include "scripts.h"
#include "pychar.h"
#include "pyroom.h"
#include "pyobj.h"
#include "pyplugs.h"



//*****************************************************************************
// local functions
//*****************************************************************************
void PyEvent_on_complete(void *owner, PyObject *tuple, const char *arg) {
  PyObject *PyOwner = NULL;
  PyObject   *efunc = NULL;
  PyObject   *edata = NULL;
  char       *otype = NULL;

  // make sure we parse everything before we call the function
  if(PyArg_ParseTuple(tuple, "sOO", &otype, &efunc, &edata)) {
    if(!strcasecmp(otype, "char"))
      PyOwner = charGetPyFormBorrowed(owner);
    else if(!strcasecmp(otype, "room"))
      PyOwner = roomGetPyFormBorrowed(owner);
    else if(!strcasecmp(otype, "obj"))
      PyOwner = objGetPyFormBorrowed(owner);
    else
      PyOwner = Py_None;

    PyObject *ret = PyObject_CallFunction(efunc, "OOs", PyOwner, edata, arg);
    if(ret == NULL)
      log_pyerr("Error finishing Python event");
    Py_XDECREF(ret);
  }

  // decrease the reference on our function and data, as well as our owner
  Py_XDECREF(tuple);
}



//*****************************************************************************
// implementation of our methods and method table
//*****************************************************************************

//
// no point in reinventing the wheel; start_event and start_update both
// need to be handled exactly the same, so here's a function to do just that
PyObject *PyEvent_start(PyObject *self, PyObject *args, void *func) {
  void (* start_func)(void *, int, void *, void *, void *, const char *) = func;
  PyObject *PyOwner = NULL;    // the python representation of our owner
  PyObject   *efunc = NULL;    // the event function
  PyObject   *edata = Py_None; // the event data
  char         *arg = NULL;    // the arg we will be supplying to the function
  double      delay = 0;       // how long the event delay is (in seconds)
  void       *owner = NULL;    // actual owner supplied to the event handler
  char    otype[20];           // is the owner a char, room, obj, or None?

  // try to parse all of our values
  if(!PyArg_ParseTuple(args, "OdO|Os", &PyOwner, &delay, &efunc, &edata, &arg)){
    //PyErr_Format(PyExc_TypeError, 
    //"Invalid arguments provided to event handler");
    return NULL;
  }

  // make sure our event is a function
  if(!PyFunction_Check(efunc)) {
    PyErr_Format(PyExc_TypeError,
		 "The event handler supplied must be a python function");
    return NULL;
  }

  // figure out what type of data our owner is
  if(PyOwner == Py_None) {
    owner = NULL;
    sprintf(otype, "none");
  }
  else if(PyChar_Check(PyOwner)) {
    owner = PyChar_AsChar(PyOwner);
    sprintf(otype, "char");
  }
  else if(PyRoom_Check(PyOwner)) {
    owner = PyRoom_AsRoom(PyOwner);
    sprintf(otype, "room");
  }
  else if(PyObj_Check(PyOwner)) {
    owner = PyObj_AsObj(PyOwner);
    sprintf(otype, "obj");
  }
  // invalid type
  else {
    PyErr_Format(PyExc_TypeError,
		 "An event owner must be a room, char, obj, or none");
    return NULL;
  }

  // make sure the owner exists
  if(PyOwner != Py_None && owner == NULL) {
    PyErr_Format(PyExc_StandardError, "Owner supplied does not exist in game");
    return NULL;
  }
  
  // now, queue up the action
  start_func(owner, (int)(delay SECONDS), PyEvent_on_complete, NULL, 
	     Py_BuildValue("sOO", otype, efunc, edata), arg);

  // everything seems ok. exit normally
  return Py_BuildValue("i", 1);
}


//
// start a new event
PyObject *PyEvent_start_event(PyObject *self, PyObject *args) {
  return PyEvent_start(self, args, start_event);
}


//
// start a new update (event that re-queues itself after completion)
PyObject *PyEvent_start_update(PyObject *self, PyObject *args) {
  PyErr_Format(PyExc_StandardError, "start_update is deprecated. Use start_event.");
  return NULL;
  /* return PyEvent_start(self, args, start_update); */
}

//
// interrupt an event involving the given room, object, or mobile
PyObject *PyEvent_interrupt_event(PyObject *self, PyObject *args) {
  PyObject *PyOwner = NULL; // the python representation of our owner

  // parse our owner
  if(!PyArg_ParseTuple(args, "O", &PyOwner)) {
    PyErr_Format(PyExc_TypeError,
		 "A target must be specified for interrupting events.");
    return NULL;
  }

  // check the possibilities
  if(PyOwner == Py_None)
    interrupt_events_involving(NULL);
  else if(PyChar_Check(PyOwner))
    interrupt_events_involving(PyChar_AsChar(PyOwner));
  else if(PyRoom_Check(PyOwner))
    interrupt_events_involving(PyRoom_AsRoom(PyOwner));
  else if(PyObj_Check(PyOwner))
    interrupt_events_involving(PyObj_AsObj(PyOwner));

  // everything ok
  return Py_BuildValue("i", 1);
}


//
// the all methods that are contained within the event module
PyMethodDef event_module_methods[] = {
  {"start_event",  PyEvent_start_event, METH_VARARGS,
   "start_event(owner, delay, event_func, data=None, arg='')\n\n"
   "Queue a new delayed event, to go off after delay seconds. Events are\n"
   "cancelled when their owner is extracted. Owners can be characters, \n"
   "objects, rooms, or None. The event function should take three arguments:\n"
   "the event owner, the event data, and the string argument. Data is an\n"
   "optional argument that can be of any type. Arg is an optional argument\n"
   "that must be a string." },
  {"start_update",  PyEvent_start_update, METH_VARARGS,
   "Deprecated. For repeating events, use events that manually re-queue\n"
   "their selves." },
  {"interrupt_events_involving",  PyEvent_interrupt_event, METH_VARARGS,
   "interrupt_events_involving(thing)\n\n"
   "Interrupt all events involving a given object, room, or character."},
  {NULL, NULL, 0, NULL}  /* Sentinel */
};



//*****************************************************************************
// implementation of pyevent.h
//*****************************************************************************
PyMODINIT_FUNC init_PyEvent(void) {
  Py_InitModule3("event", event_module_methods, 
    "The event module handles delayed function calls.");
}
