//*****************************************************************************
//
// bitvector.c
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

#include "mud.h"
#include "utils.h"

#include "bitvector.h"



//*****************************************************************************
// local functions, datastructures, and defines
//*****************************************************************************
// a table of mappings between bitvector names, and the 
// data assocciated with them (i.e. bit:value mappings)
HASHTABLE *bitvector_table = NULL;

typedef struct bitvector_data {
  HASHTABLE *bitmap; // a mapping from bit name to bit number
  char        *name; // which bitvector is this?
} BITVECTOR_DATA;

struct bitvector {
  BITVECTOR_DATA *data; // the data corresponding to this bitvector
  char           *bits; // the bits we have set/unset
};

BITVECTOR_DATA *newBitvectorData(const char *name) {
  BITVECTOR_DATA *data = malloc(sizeof(BITVECTOR_DATA));
  data->bitmap = newHashtable();
  data->name   = strdup(name);
  return data;
}




//*****************************************************************************
// implementation of bitvector.h
//*****************************************************************************
void init_bitvectors() {
  // create the bitvector table
  bitvector_table = newHashtable();

  // and also create some of the basic bitvectors and 
  // bits that come stock and are required for the core release
  bitvectorCreate("char_prfs");

  bitvectorCreate("obj_bits");
  bitvectorAddBit("obj_bits", "notake");

  bitvectorCreate("room_bits");

  bitvectorCreate("user_groups");
  bitvectorAddBit("user_groups", "admin");
  bitvectorAddBit("user_groups", "scripter");
  bitvectorAddBit("user_groups", "builder");
  bitvectorAddBit("user_groups", "player");
  bitvectorAddBit("user_groups", "playtester");
  bitvectorAddBit("user_groups", "wizard");
  bitvectorAddBit("user_groups", "empty");
}

void bitvectorAddBit(const char *name, const char *bit) {
  BITVECTOR_DATA *data = hashGet(bitvector_table, name);
  if(data != NULL)
    hashPut(data->bitmap, bit, (void *)(hashSize(data->bitmap) + 1));
}

void bitvectorCreate(const char *name) {
  if(!hashGet(bitvector_table, name))
    hashPut(bitvector_table, name, newBitvectorData(name));
}

BITVECTOR *newBitvector() {
  BITVECTOR *v = calloc(1, sizeof(BITVECTOR));
  return v;
}

BITVECTOR   *bitvectorInstanceOf(const char *name) {
  BITVECTOR_DATA *data = hashGet(bitvector_table, name);
  BITVECTOR    *vector = NULL;
  if(data != NULL) {
    int vector_len = hashSize(data->bitmap)/8 + 1;
    vector = newBitvector();
    vector->data = data;
    vector->bits = calloc(vector_len, sizeof(char));
  }
  return vector;
}

void         deleteBitvector(BITVECTOR *v) {
  if(v->bits) free(v->bits);
  free(v);
}

void         bitvectorCopyTo(BITVECTOR *from, BITVECTOR *to) {
  int bit_len = 1 + hashSize(from->data->bitmap)/8;
  to->data = from->data;
  if(to->bits) free(to->bits);
  to->bits = malloc(sizeof(char) * bit_len);
  to->bits = memcpy(to->bits, from->bits, bit_len);
}

BITVECTOR   *bitvectorCopy(BITVECTOR *v) {
  BITVECTOR *newvector = bitvectorInstanceOf(v->data->name);
  bitvectorCopyTo(v, newvector);
  return newvector;
}

bool bitIsSet(BITVECTOR *v, const char *bit) {
  LIST    *bits = parse_keywords(bit);
  char *one_bit = NULL; 
  bool    found = FALSE;

  // check for one
  while( !found && (one_bit = listPop(bits)) != NULL) {
    found = bitIsOneSet(v, one_bit);
    free(one_bit);
  }
  
  // clean up our mess
  deleteListWith(bits, free);
  return found;
}

bool bitIsAllSet(BITVECTOR *v, const char *bit) {
  LIST    *bits = parse_keywords(bit);
  char *one_bit = NULL;
  bool    found = TRUE;

  // check for each one
  while( found && (one_bit = listPop(bits)) != NULL) {
    found = bitIsOneSet(v, one_bit);
    free(one_bit);
  }

  // clean up our mess
  deleteListWith(bits, free);
  return found;
}

bool bitIsOneSet(BITVECTOR *v, const char *bit) {
  int val = (int)hashGet(v->data->bitmap, bit);
  return IS_SET(v->bits[val/8], (1 << (val % 8)));
}

void bitSet(BITVECTOR *v, const char *name) {
  LIST    *bits = parse_keywords(name);
  char *one_bit = NULL;

  // set each one
  while( (one_bit = listPop(bits)) != NULL) {
    int val = (int)hashGet(v->data->bitmap, one_bit);
    free(one_bit);
    // 0 is a filler meaning 'this is not an actual name for a bit'
    if(val == 0) continue;
    SET_BIT(v->bits[val/8], (1 << (val % 8)));
  }

  // garbage collection
  deleteListWith(bits, free);
}

void bitClear(BITVECTOR *v) {
  int i;
  for(i = 1; i <= hashSize(v->data->bitmap); i++)
    REMOVE_BIT(v->bits[i/8], (1 << (i % 8)));
}

void bitRemove(BITVECTOR *v, const char *name) {
  LIST    *bits = parse_keywords(name);
  char *one_bit = NULL;

  // remove each one
  while( (one_bit = listPop(bits)) != NULL) {
    int val = (int)hashGet(v->data->bitmap, one_bit);
    REMOVE_BIT(v->bits[val/8], (1 << (val % 8)));
    free(one_bit);
  }

  // garbage collection
  deleteListWith(bits, free);
}

void bitToggle(BITVECTOR *v, const char *name) {
  LIST    *bits = parse_keywords(name);
  char *one_bit = NULL;

  // toggle each one
  while( (one_bit = listPop(bits)) != NULL) {
    int val = (int)hashGet(v->data->bitmap, one_bit);
    free(one_bit);
    // 0 is a filler meaning 'this is not an actual name for a bit'
    if(val == 0) continue;
    TOGGLE_BIT(v->bits[val/8], (1 << (val % 8)));
  }

  // garbage collection
  deleteListWith(bits, free);
}

const char *bitvectorGetBits(BITVECTOR *v) {
  static char bits[MAX_BUFFER];
  HASH_ITERATOR *hash_i = newHashIterator(v->data->bitmap);
  const char *key = NULL;
  void *val       = NULL;
  int bit_i       = 0;
  *bits = '\0';

  // add each set bit to our list to store
  ITERATE_HASH(key, val, hash_i) {
    if(bitIsOneSet(v, key)) {
      bit_i += snprintf(bits+bit_i, MAX_BUFFER-bit_i, "%s%s", 
			(bit_i == 0 ? "" : ", "), key);
    }
  }
  deleteHashIterator(hash_i);
  return bits;
}

int bitvectorSize(BITVECTOR *v) {
  return hashSize(v->data->bitmap);
}

LIST *bitvectorListBits(BITVECTOR *v) {
  return hashCollect(v->data->bitmap);
}
