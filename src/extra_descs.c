//*****************************************************************************
//
// extra_desc.c
//
// Extra descriptions are little embellishments to rooms that give extra
// information about the setting if people examine them. Each edesc has a list
// of keywords that activates it. The extra_desc name is a bit misleading,
// as it also can be (and is) used to hold speech keywords/replies for NPCs.
//
//*****************************************************************************

#include "mud.h"
#include "utils.h"
#include "storage.h"
#include "extra_descs.h"

struct edesc_set_data {
  LIST *edescs;
};

struct edesc_data {
  EDESC_SET *set;
  char      *keywords;
  BUFFER    *desc;
};



//*****************************************************************************
//
// edesc set
//
//*****************************************************************************
EDESC_SET  *newEdescSet() {
  EDESC_SET *set = malloc(sizeof(EDESC_SET));
  set->edescs    = NULL;
  return set;
}

EDESC_DATA *edescRead(STORAGE_SET *set) {
  return newEdesc(read_string(set, "keywords"), read_string(set, "desc"));
}

STORAGE_SET *edescStore(EDESC_DATA *data) {
  STORAGE_SET *set = new_storage_set();
  store_string(set, "keywords", data->keywords);
  store_string(set, "desc",     bufferString(data->desc));
  return set;
}

EDESC_SET *edescSetRead(STORAGE_SET *set) {
  EDESC_SET      *edescs = newEdescSet();
  if(storage_contains(set, "list")) {
    STORAGE_SET_LIST *list = read_list(set, "list");
    // deleteList(edescs->edescs);
    edescs->edescs = gen_read_list(list, edescRead);
    LIST_ITERATOR *list_i = newListIterator(edescs->edescs);
    EDESC_DATA     *edesc = NULL;
    ITERATE_LIST(edesc, list_i) {
      edesc->set = edescs;
    } deleteListIterator(list_i);
  }
  return edescs;
}


STORAGE_SET *edescSetStore(EDESC_SET *edescs) {
  STORAGE_SET *set = new_storage_set();
  if(edescs->edescs != NULL)
    store_list(set, "list", gen_store_list(edescs->edescs, edescStore));
  return set;
}

void edescSetCopyTo(EDESC_SET *from, EDESC_SET *to) {
  // delete all of the current entries
  if(to->edescs != NULL) {
    deleteListWith(to->edescs, deleteEdesc);
    to->edescs = NULL;
  }
  if(from->edescs != NULL) {
    to->edescs = listCopyWith(from->edescs, edescCopy);
    LIST_ITERATOR *list_i = newListIterator(to->edescs);
    EDESC_DATA     *edesc = NULL;
    ITERATE_LIST(edesc, list_i) {
      edesc->set = to;
    } deleteListIterator(list_i);
  }
}

EDESC_SET *edescSetCopy(EDESC_SET *set) {
  EDESC_SET *newset = newEdescSet();
  edescSetCopyTo(set, newset);
  return newset;
}

EDESC_DATA *edescSetGet(EDESC_SET *set, const char *keyword) {
  if(set->edescs == NULL)
    return NULL;

  LIST_ITERATOR *edesc_i = newListIterator(set->edescs);
  EDESC_DATA       *desc = NULL;

  ITERATE_LIST(desc, edesc_i) {
    if(edescIsKeyword(desc, keyword))
      break;
  } deleteListIterator(edesc_i);
  return desc;
}

void edescSetPut(EDESC_SET *set, EDESC_DATA *edesc) {
  if(set->edescs == NULL)
    set->edescs = newList();
  edesc->set = set;
  listQueue(set->edescs, edesc);
}

EDESC_DATA *edescSetGetNum(EDESC_SET *set, int num) {
  if(set->edescs == NULL)
    return NULL;
  return listGet(set->edescs, num);
}

void removeEdesc(EDESC_SET *set, EDESC_DATA *edesc) {
  if(set->edescs != NULL && listRemove(set->edescs, edesc))
    edesc->set = NULL;
}

EDESC_DATA *edescSetRemove(EDESC_SET *set, const char *keyword) {
  if(set->edescs == NULL)
    return NULL;
  EDESC_DATA *entry = edescSetGet(set, keyword);
  if(entry && listRemove(set->edescs, entry))
    entry->set = NULL;
  return entry;
}

EDESC_DATA *edescSetRemoveNum (EDESC_SET *set, int num) {
  if(set->edescs == NULL)
    return NULL;

  EDESC_DATA *entry = listRemoveNum(set->edescs, num);
  if(entry)
    entry->set = NULL;
  return entry;
}

void deleteEdescSet(EDESC_SET *set) {
  if(set->edescs)
    deleteListWith(set->edescs, deleteEdesc);
  free(set);
}

int edescGetSetSize(EDESC_SET *set) {
  if(set->edescs)
    return listSize(set->edescs);
  return 0;
}

LIST *edescSetGetList(EDESC_SET *set) {
  return set->edescs;
}

void edescTagDesc(BUFFER *buf, EDESC_SET *set, 
		  const char *start_tag, const char *end_tag) {
  if(set->edescs == NULL)
    return;

  LIST_ITERATOR *list_i = newListIterator(set->edescs);
  EDESC_DATA    *edesc  = NULL;

  // go through, and apply colors for each extra desc we have
  ITERATE_LIST(edesc, list_i) {
    buf_tag_keywords(buf, edesc->keywords, start_tag, end_tag);
  } deleteListIterator(list_i);
}



//*****************************************************************************
// a single edesc
//*****************************************************************************
EDESC_DATA *newEdesc(const char *keywords, const char *desc) {
  EDESC_DATA *edesc = malloc(sizeof(struct edesc_data));

  edesc->set      = NULL;
  edesc->keywords = strdupsafe(keywords);
  edesc->desc     = newBuffer(1);
  bufferCat(edesc->desc, (desc ? desc : ""));

  return edesc;
}

void deleteEdesc(EDESC_DATA *edesc) {
  if(edesc->keywords) free(edesc->keywords);
  if(edesc->desc)     deleteBuffer(edesc->desc);
  free(edesc);
}

EDESC_DATA *edescCopy(EDESC_DATA *edesc) {
  return newEdesc(edesc->keywords, bufferString(edesc->desc));
}

void edescCopyTo(EDESC_DATA *from, EDESC_DATA *to) {
  // copy over the new desc
  bufferCopyTo(from->desc, to->desc);
  if(to->keywords) free(to->keywords);
  to->keywords = strdupsafe(from->keywords);
}

const char *edescGetKeywords(EDESC_DATA *edesc) {
  return edesc->keywords;
}

const char *edescSetGetDesc(EDESC_DATA *edesc) {
  return bufferString(edesc->desc);
}

BUFFER *edescGetDescBuffer(EDESC_DATA *edesc) {
  return edesc->desc;
}

void edescSetKeywords(EDESC_DATA *edesc, const char *keywords) {
  if(edesc->keywords) free(edesc->keywords);
  edesc->keywords   = strdupsafe(keywords);
}

void edescSetDesc(EDESC_DATA *edesc, const char *description) {
  bufferClear(edesc->desc);
  bufferCat(edesc->desc, (description ? description : ""));
}

EDESC_SET *edescGetSet(EDESC_DATA *edesc) {
  return edesc->set;
}

bool edescIsKeyword(EDESC_DATA *edesc, const char *keyword) {
  return is_keyword(edesc->keywords, keyword, TRUE);
}
