#ifndef SET_VAL_H
#define SET_VAL_H
//*****************************************************************************
//
// set_val.h
//
// There are often times in-game when we will want to change the values of a
// field in a character or object; most DIKU muds do this via a "set" command
// that simply has a list of all of the fields, and how to handle performing
// the set. However, because of our modular nature, it would be fairly 
// cumbersome to have all of the possible things to set in one big list. 
// Instead, we provide a way that modules can install new fields that can be
// set for rooms, objects, and mobiles.
//
//*****************************************************************************

#define SET_TYPE_INT       0
#define SET_TYPE_DOUBLE    1
#define SET_TYPE_LONG      2
#define SET_TYPE_STRING    3

#define SET_CHAR           0
#define SET_OBJECT         1
#define SET_ROOM           2
#define SET_ACCOUNT        3


//
// prepare the set utility for use
void init_set();


//
// Adds a new field that can be set for the type of thing we are trying to
// modify. type is the datatype we're modifying (int, double, long, string)
// and setter is the function that takes the thing to be set (char, obj, room)
// and the new value. If there are constraints on the applicable values that
// can be set, checker should be provided. It takes in the to-be-set value and
// makes sure it is OK, returning TRUE if it is and FALSE if it is not. The
// checker function should be of the form:
//    bool checker(datatype newval)
void add_set(const char *name, int set_for, int type, void *setter, void *checker);

#endif // SET_VAL_H
