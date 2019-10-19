#ifndef BITVECTOR_H
#define BITVECTOR_H
//*****************************************************************************
//
// bitvector.h
//
// One of the questions that came up when designing NakedMud is, how do we
// efficiently implement bitvectors while maintaining the modularity of the mud?
// For those unfamiliar with the purposes of a bitvector, let me elaborate:
// there are many instances in which we may want to know about the presence or
// absence of a property (e.g. is the room dark? is the character blind?). We
// could easily have a variable for each of these properties in the relevant
// datastructures, but this is often quite wasteful on memory. What would be
// really nice is if we could compress the space that it took to store one
// bit of information (in C, the smallest basic datatype is 8 bits ... i.e. we
// are wasting 7 bits to store a piece of information that be represented as
// on/off). In a nutshell, this is the purpose of a bitvector (there are more
// reasons why bitvectors are attractive, but this is perhaps the most important
// one in the context of muds).
//
// For anyone who has ever worked on a DIKU-derivative, they know that
// bits in a bitvector are typically declared on after another like this:
//    #define BIT_ONE    (1 << 0)
//    #define BIT_TWO    (1 << 1)
//    #define BIT_THREE  (1 << 2)
//    ...
//
// Of course, this is hugely unattractive for us: If we had a module that wanted
// to add a new bit, we surely would not want to add it to some global header
// file; we would want to keep the definition within the module headers. And,
// most certainly we cannot do this because we have to assocciate a position
// with each bit name, and every other module has to know that position so that
// noone else adds a new bit with the same position. This module is an attempt
// to address this problem, allowing modules to use bits, keep their 
// declarations in some place local to the module, and make sure there are no
// conflicting definitions. This also ensures that bits are persistent for all
// rooms, objects, and characters (ie. they save if we crash, or the player
// logs off, or whatnot).
//
//*****************************************************************************

typedef struct bitvector BITVECTOR;

//
// prepare bitvector systems for use
void init_bitvectors();

//
// Create a new type of bitvector. This must be called before any bit types 
// can be added, or before a bitvector of the given name can be created
void bitvectorCreate(const char *name);

//
// Add a new bit to an existing bitvector. The bitvector must have already
// been created. Multiple bits can be added if their names are comma-separated.
// This should only be called after init_bitvectors() is run, and it should 
// never be called when the game is actually running (i.e. the world has loaded,
// mobs have spawned, etc...). Ideally, bitvectorAddBit should only ever be 
// called in a module's initialization function.
void bitvectorAddBit(const char *name, const char *bit);

//
// create a new instance of the bitvector with the given name. 
// Return NULL if a bitvector with the given name does not exist
BITVECTOR   *bitvectorInstanceOf(const char *name);
void         deleteBitvector(BITVECTOR *v);
void         bitvectorCopyTo(BITVECTOR *from, BITVECTOR *to);
BITVECTOR   *bitvectorCopy(BITVECTOR *v);

//
// checks to see if ANY of the bits in the name list are set. Name can be 
// a single bit, or a comma-separated list of bits
bool bitIsSet(BITVECTOR *v, const char *bit);

//
// checks to see if ALL of the bits in the name list are set
bool bitIsAllSet(BITVECTOR *v, const char *bit);

//
// Check to see if only one bit is set, unlike bit_is_set and
// bit_is_all_set, which check for potentially multiple functions
bool bitIsOneSet(BITVECTOR *v, const char *bit);

//
// set the bits in the name list onto the bitvector
void bitSet(BITVECTOR *v, const char *name);

//
// remove the bits in the name list from the bitvector
void bitRemove(BITVECTOR *v, const char *name);

//
// sets all of the bits on the vector to off
void bitClear(BITVECTOR *v);

//
// toggle the specified bits on or off... whichever one they are not, currently
void bitToggle(BITVECTOR *v, const char *name);

//
// return a comma-separated list of the bits the vector has set
const char *bitvectorGetBits(BITVECTOR *v);

//
// returns the number of possible bits that can be set on this bitvector
int bitvectorSize(BITVECTOR *v);

//
// return a list of the possible bits that this vector can have set. The list
// must be deleted after use, as well as all the contents. 
// try: deleteListWith(list, free)
LIST *bitvectorListBits(BITVECTOR *v);

#endif // BITVECTOR_H
