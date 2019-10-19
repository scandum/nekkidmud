//*****************************************************************************
//
// body.c
//
// Different creatures are shaped in fundamentally different ways (e.g.
// bipedal humans and quadrapedal bears). Here's our attempt to create a
// structure that captures this idea.
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "body.h"


struct bodypart_data {
  char          *name;       // the name of the position
  int            type;       // what kind of position type is this?
  int            size;       // how big is it, relative to other positions?
  OBJ_DATA *equipment;       // what is being worn here?
};

typedef struct bodypart_data   BODYPART;

struct body_data {
  LIST   *parts;             // a list of all the parts on the body
  int      size;             // how big is our body?
};


//*****************************************************************************
//
// Local functions. Mosty concerned with the handling of bodypart_datas, as
// opposed to their container, the body.
//
//*****************************************************************************
const char *bodypos_list[NUM_BODYPOS] = {
  "floating about head",
  "head",
  "face",
  "ear",
  "neck",
  "about body",
  "torso",
  "arm",
  "wing",
  "wrist",
  "left hand",
  "right hand",
  "finger",
  "waist",
  "leg",
  "left foot",
  "right foot",
  "hoof",
  "claw",
  "tail",
  "held"
};

const char *bodysize_list[NUM_BODYSIZES] = {
  "diminuitive",
  "tiny",
  "small",
  "medium",
  "large",
  "huge",
  "gargantuan",
  "collosal"
};


/**
 * Create a new bodypart_data
 */
BODYPART *newBodypart(const char *name, int type, int size) {
  BODYPART *P = malloc(sizeof(BODYPART));
  P->equipment = NULL;
  P->type = type;
  P->size = MAX(0, size); // parts of size 0 cannot be hit
  P->name = strdup((name ? name : "nothing"));

  return P;
};


/**
 * Delete a bodypart_data, but do not delete the equipment on it
 */
void deleteBodypart(BODYPART *P) {
  if(P->name) free(P->name);
  free(P);
};

/**
 * Copy a bodypart
 */
BODYPART *bodypartCopy(BODYPART *P) {
  BODYPART *p_new = malloc(sizeof(BODYPART));
  p_new->equipment = NULL;
  p_new->type      = P->type;
  p_new->size      = P->size;
  p_new->name      = strdup(P->name);
  return p_new;
}



//*****************************************************************************
//
// Functions for body interface. Documentation contained in body.h
//
//*****************************************************************************

char *list_postypes(const BODY_DATA *B, const char *posnames) {
  LIST           *names = parse_keywords(posnames);
  LIST_ITERATOR *name_i = newListIterator(names);
  BUFFER           *buf = newBuffer(100);
  char            *name = NULL;
  char          *retval = NULL;
  int             found = 0;

  ITERATE_LIST(name, name_i) {
    int part = bodyGetPart(B, name);
    if(part != BODYPOS_NONE) {
      found++;
      if(found != 1)
	bufferCat(buf, ", ");
      bufferCat(buf, bodyposGetName(part));
    }
  } deleteListIterator(name_i);
  deleteListWith(names, free);

  retval = strdup(bufferString(buf));
  deleteBuffer(buf);
  return retval;
}

const char *bodysizeGetName(int size) {
  return bodysize_list[size];
}

int bodysizeGetNum(const char *size) {
  int i;
  for(i = 0; i < NUM_BODYSIZES; i++)
    if(!strcasecmp(size, bodysize_list[i]))
      return i;
  return BODYSIZE_NONE;
}

const char *bodyposGetName(int bodypos) {
  return bodypos_list[bodypos];
}

int bodyposGetNum(const char *bodypos) {
  int i;
  for(i = 0; i < NUM_BODYPOS; i++)
    if(!strcasecmp(bodypos, bodypos_list[i]))
      return i;
  return BODYPOS_NONE;
}

BODY_DATA *newBody() {
  struct body_data*B = malloc(sizeof(BODY_DATA));
  B->parts = newList();

  return B;
}

void deleteBody(BODY_DATA *B) {
  // delete all of the bodyparts
  deleteListWith(B->parts, deleteBodypart);
  // free us
  free(B);
}

BODY_DATA *bodyCopy(const BODY_DATA *B) {
  BODY_DATA *Bnew = newBody();
  deleteListWith(Bnew->parts, deleteBodypart);
  Bnew->parts = listCopyWith(B->parts, bodypartCopy);
  Bnew->size  = B->size;

  return Bnew;
}

int bodyGetSize(const BODY_DATA *B) {
  return B->size;
}

void bodySetSize(BODY_DATA *B, int size) {
  B->size = size;
}

//
// Find a bodypart on the body with the given name
//
BODYPART *findBodypart(const BODY_DATA *B, const char *pos) {
  LIST_ITERATOR *part_i = newListIterator(B->parts);
  BODYPART *part = NULL;

  ITERATE_LIST(part, part_i)
    if(!strcasecmp(part->name, pos))
      break;
  deleteListIterator(part_i);

  return part;
}

//
// Find a bodypart on the body with of the specified type that
// is not yet equipped with an item
//
BODYPART *findFreeBodypart(BODY_DATA *B, const char *type) {
  int typenum = bodyposGetNum(type);
  if(typenum == BODYPOS_NONE) return NULL;

  LIST_ITERATOR *part_i = newListIterator(B->parts);
  BODYPART *part = NULL;
  ITERATE_LIST(part, part_i)
    if(part->type == typenum && part->equipment == NULL)
      break;
  deleteListIterator(part_i);

  return part;
}

void bodyAddPosition(BODY_DATA *B, const char *pos, int type, int size) {
  BODYPART *part = findBodypart(B, pos);

  // if we've already found the part, just modify it
  if(part) {
    part->type = type;
    part->size = size;
  }
  // otherwise, add a new part
  else
    listPut(B->parts, newBodypart(pos, type, size));
}

bool bodyRemovePosition(BODY_DATA *B, const char *pos) {
  BODYPART *part = findBodypart(B, pos);

  if(!part)
    return FALSE;

  listRemove(B->parts, part);
  deleteBodypart(part);
  return TRUE;
}

int bodyGetPart(const BODY_DATA *B, const char *pos) {
  BODYPART *part = findBodypart(B, pos);
  if(part) return part->type;
  else     return BODYPOS_NONE;
}


double bodyPartRatio(const BODY_DATA *B, const char *pos) {
  LIST_ITERATOR *part_i = newListIterator(B->parts);
  BODYPART        *part = NULL;
  double      part_size = 0.0;
  double      body_size = 0.0;

  // add up all of the weights, and find the weight of our pos
  ITERATE_LIST(part, part_i) {
    body_size += part->size;
    if(is_keyword(pos, part->name, FALSE))
      part_size += part->size;
  }
  deleteListIterator(part_i);

  // to prevent div0, albeit an unlikely event
  return (body_size == 0.0 ? 0 : (part_size / body_size));
}


const char *bodyRandPart(const BODY_DATA *B, const char *pos) {
  LIST_ITERATOR *part_i = newListIterator(B->parts);
  BODYPART *part = NULL;
  char     *name = NULL;
  int   size_sum = 0;
  int   pos_roll = 0;

  // add up all of the weights
  ITERATE_LIST(part, part_i) {
    // if we have a list of positions to draw from, only factor in those
    if(pos && *pos && !is_keyword(pos, part->name, FALSE))
      continue;
    size_sum += part->size;
  } deleteListIterator(part_i);

  // nothing that can be hit was found
  if(size_sum <= 1) {
    deleteListIterator(part_i);
    return NULL;
  }

  pos_roll = rand_number(1, size_sum);
  
  // find the position the roll corresponds to
  part_i = newListIterator(B->parts);
  ITERATE_LIST(part, part_i) {
    // if we have a list of positions to draw from, only factor in those
    if(pos && *pos && !is_keyword(pos, part->name, FALSE))
      continue;
    pos_roll -= part->size;
    if(pos_roll <= 0) {
      name = part->name;
      break;
    }
  } deleteListIterator(part_i);
  return name;
}


const char **bodyGetParts(const BODY_DATA *B, bool sort, int *num_pos) {
  *num_pos = listSize(B->parts);

  const char **parts = malloc(sizeof(char *) * *num_pos);
  LIST_ITERATOR *part_i = newListIterator(B->parts);
  BODYPART *part = NULL;
  int i = 0;

  // if we don't need to sort, this should be fine ...
  if(!sort) {
    ITERATE_LIST(part, part_i)
      parts[i] = part->name;
    deleteListIterator(part_i);
  }
  // take some extra steps to make sure everything is sorted
  else {
    int pos[*num_pos];

    i = 0;
    ITERATE_LIST(part, part_i) {
      parts[i] = part->name;
      pos[i]   = part->type;
      i++;
    } deleteListIterator(part_i);

    // now sort everything in the array
    for(i = 0; i < *num_pos; i++) {
      // max_pos in this context is not the position in the array with the
      // highest value. Rather, it is the position in the array whose value
      // corresponds to the highest position on a body, which is negatively
      // related to the value of the position.
      int j, max_pos = i;
      for(j = i+1; j < *num_pos; j++) {
	if(pos[j] < pos[max_pos])
	  max_pos = j;
      }

      // do a shuffle
      if(max_pos != i) {
	const char *tmp_ptr = parts[i];
	parts[i] = parts[max_pos];
	parts[max_pos] = tmp_ptr;

	int tmp_val = pos[i];
	pos[i] = pos[max_pos];
	pos[max_pos] = tmp_val;
      }
    }
  }
  return parts;
}


bool bodyEquipPostypes(BODY_DATA *B, OBJ_DATA *obj, const char *types) {
  LIST  *pos_list = parse_keywords(types);
  LIST     *parts = NULL;
  BODYPART  *part = NULL;
  bool    success = TRUE;

  // make sure we have more than zero positions
  if(listSize(pos_list) == 0) {
    deleteList(pos_list);
    return FALSE;
  }

  // create our list of parts
  parts = newList();

  // get a list of all open slots in the list provided ...
  // equip them as we go along, incase we more than one of a piece.
  // if we don't do it this way, findFreeBodypart might find the same
  // piece multiple times (e.g. the same ear when it's looking for two ears)
  LIST_ITERATOR *pos_i = newListIterator(pos_list);
  char            *pos = NULL;

  ITERATE_LIST(pos, pos_i) {
    part = findFreeBodypart(B, pos);
    if(part && !part->equipment) {
      part->equipment = obj;
      listPut(parts, part);
    }
  } deleteListIterator(pos_i);

  // make sure we supplied a valid number of empty positions
  if(listSize(pos_list) != listSize(parts)) {
    // remove equipment for every part we put it on
    while((part = listPop(parts)) != NULL)
      part->equipment = NULL;
    success = FALSE;
  }

  // garbage collection
  deleteListWith(pos_list, free);
  deleteList(parts);

  return success;
}


bool bodyEquipPosnames(BODY_DATA *B, OBJ_DATA *obj, const char *positions) {
  LIST *pos_list = parse_keywords(positions);
  LIST    *parts = NULL;
  BODYPART *part = NULL;
  bool   success = TRUE;

  // make sure we have more than zero positions
  if(listSize(pos_list) == 0) {
    deleteList(pos_list);
    return FALSE;
  }

  // create our list of parts
  parts = newList();

  // get a list of all open slots in the list provided
  LIST_ITERATOR *pos_i = newListIterator(pos_list);
  char            *pos = NULL;
  ITERATE_LIST(pos, pos_i) {
    part = findBodypart(B, pos);
    if(part && !part->equipment && !listIn(parts, part))
      listPut(parts, part);
  } deleteListIterator(pos_i);

  // make sure we found the right amount of parts
  if(listSize(parts) != listSize(pos_list) || listSize(parts) == 0)
    success = FALSE;

  // fill in all of the parts that need to be filled
  while( (part = listPop(parts)) != NULL)
    part->equipment = obj;

  // clean up our garbage
  deleteListWith(pos_list, free);
  deleteList(parts);

  return success;
}

const char *bodyEquippedWhere(BODY_DATA *B, OBJ_DATA *obj) {
  static char buf[SMALL_BUFFER];
  *buf = '\0';

  LIST_ITERATOR *part_i = newListIterator(B->parts);
  BODYPART *part = NULL;
  // go through the list of all parts, and print the name of any one
  // with the piece of equipment on it, onto the buf
  ITERATE_LIST(part, part_i) {
    if(part->equipment == obj) {
      // if we've already printed something, add a comma
      if(*buf)
	strcat(buf, ", ");
      strcat(buf, part->name);
    }
  } deleteListIterator(part_i);
  return buf;
}

OBJ_DATA *bodyGetEquipment(BODY_DATA *B, const char *pos) {
  BODYPART *part = findBodypart(B, pos);
  return (part ? part->equipment : NULL);
}

bool bodyUnequip(BODY_DATA *B, const OBJ_DATA *obj) {
  LIST_ITERATOR *part_i = newListIterator(B->parts);
  BODYPART      *part   = NULL;
  bool           found  = FALSE;

  ITERATE_LIST(part, part_i) {
    if(part->equipment == obj) {
      part->equipment = NULL;
      found = TRUE;
    }
  } deleteListIterator(part_i);

  return found;
}

LIST *bodyGetAllEq(BODY_DATA *B) {
  LIST *equipment = newList();
  LIST_ITERATOR *part_i = newListIterator(B->parts);
  BODYPART *part = NULL;

  ITERATE_LIST(part, part_i) {
    if(part->equipment && !listIn(equipment, part->equipment))
      listPut(equipment, part->equipment);
  } deleteListIterator(part_i);
  return equipment;
}

LIST *bodyUnequipAll(BODY_DATA *B) {
  LIST *equipment = newList();
  LIST_ITERATOR *part_i = newListIterator(B->parts);
  BODYPART *part = NULL;

  ITERATE_LIST(part, part_i) {
    if(part->equipment && !listIn(equipment, part->equipment)) {
      listPut(equipment, part->equipment);
      part->equipment = NULL;
    }
  } deleteListIterator(part_i);
  return equipment;
}

int numBodyparts(const BODY_DATA *B) {
  return listSize(B->parts);
}
