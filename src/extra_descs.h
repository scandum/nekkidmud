#ifndef __EXTRA_DESC_H
#define __EXTRA_DESC_H
//*****************************************************************************
//
// extra_desc.h
//
// Extra descriptions are little embellishments to rooms that give extra
// information about the setting if people examine them. Each edesc has a list
// of keywords that activates it. The extra_desc name is a bit misleading,
// as it also can be (and is) used to hold speech keywords/replies for NPCs.
//
//*****************************************************************************





//*****************************************************************************
// a set of edescs
//*****************************************************************************
EDESC_SET  *newEdescSet         (void);
void        deleteEdescSet      (EDESC_SET *set);
void        edescSetCopyTo      (EDESC_SET *from, EDESC_SET *to);
EDESC_SET  *edescSetCopy        (EDESC_SET *set);
void        edescSetPut         (EDESC_SET *set, EDESC_DATA *edesc);
EDESC_DATA *edescSetGet         (EDESC_SET *set, const char *keyword);
EDESC_DATA *edescSetGetNum      (EDESC_SET *set, int num);
EDESC_DATA *edescSetRemove      (EDESC_SET *set, const char *keyword);
EDESC_DATA *edescSetRemoveNum   (EDESC_SET *set, int num);
void        removeEdesc         (EDESC_SET *set, EDESC_DATA *edesc);
int         edescGetSetSize     (EDESC_SET *set);
LIST       *edescSetGetList     (EDESC_SET *set);
void        edescTagDesc        (BUFFER *buf, EDESC_SET *set,  
				 const char *start_tag, const char *end_tag);



//*****************************************************************************
// a single edesc
//*****************************************************************************

//
// Create a new EDESC.
// keywords must be comma-separated
//
EDESC_DATA *newEdesc(const char *keywords, const char *desc);

//
// Delete an EDESC
//
void deleteEdesc(EDESC_DATA *edesc);

//
// make a storage set out of the extra desc set, or parse an extra
// desc set from the storage set.
//
EDESC_SET *edescSetRead(STORAGE_SET *set);
STORAGE_SET *edescSetStore(EDESC_SET *edescs);

//
// Make a copy of the EDESC
//
EDESC_DATA *edescCopy(EDESC_DATA *edesc);

//
// copy the contents of one EDESC to another
//
void edescCopyTo(EDESC_DATA *from, EDESC_DATA *to);

//
// Get a list of the keywords
//
const char *edescGetKeywords(EDESC_DATA *edesc);

//
// return a pointer to the description
//
const char *edescSetGetDesc(EDESC_DATA *edesc);

//
// get a pointer to the edesc description (for text editing in OLC)
//
BUFFER *edescGetDescBuffer(EDESC_DATA *edesc);

//
// set the keywords to this new list of keywords. Keywords
// must be comma-separated
//
void edescSetKeywords(EDESC_DATA *edesc, const char *keywords);

//
// set the description to this new description
//
void edescSetDesc(EDESC_DATA *edesc, const char *description);

//
// returns TRUE if the keyword is a valid one
//
bool edescIsKeyword(EDESC_DATA *edesc, const char *keyword);

//
// get the set the edesc belongs to
//
EDESC_SET *edescGetSet(EDESC_DATA *edesc);

//
// For each keyword in our list, tag copies of it in string
// and return a new copy of string (with tags)
//
char *tagKeywords(EDESC_DATA *edesc, const char *string,
		  const char *start_tag, const char *end_tag);

#endif // __EXTRA_DESC_H
