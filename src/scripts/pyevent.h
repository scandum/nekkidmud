#ifndef PYEVENT_H
#define PYEVENT_H
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

// initialize the pyevent system for use
PyMODINIT_FUNC init_PyEvent(void);

#endif // PYEVENT_H
