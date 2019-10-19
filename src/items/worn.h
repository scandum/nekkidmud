#ifndef WORN_H
#define WORN_H
//*****************************************************************************
//
// worn.h
//
// handles all of the functioning of wearable items. Perhaps this could
// eventually be extended to include armors? I think, perhaps, one of the 
// weirdest things about (most) DIKUs is that worn items and armors are two
// different item types; really, wearable items are just armors that provide
// no armor class. Fusing the two item types into one might be a much more
// fruitful route to take. Or perhaps another route would be to just make 
// another item type called "armor" that only functions if the item is also of
// type "worn"; it calculates armor class/protection/whatnot based on the type
// of worn item the item is.
//
// that said, I'm not going to do it. Well, not for NakedMud anyways; I don't
// want to burden other developers with my conception of what a good way to do
// armor class is. Therefore, if you agree with me, I leave the exercise up to
// you :)
//
//*****************************************************************************

//
// what type of a worn item is this object? 
const char *wornGetType(OBJ_DATA *obj);
const char *wornGetPositions(OBJ_DATA *obj);
void wornSetType(OBJ_DATA *obj, const char *type);

//
// add a new worn type to our list of possible worn types. A type name and
// a list of open positions for equipping the item are required.
void worn_add_type(const char *type, const char *required_positions);

#endif // WORN_H
