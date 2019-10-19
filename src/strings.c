/*
 * This file handles string copy/search/comparison/etc.
 */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

/* include main header file */
#include "mud.h"
#include "utils.h"

/*
 * Compares two strings, and returns TRUE
 * if they match 100% (not case sensetive).
 */
bool compares(const char *aStr, const char *bStr)
{
  int i = 0;

  /* NULL strings never compares */
  if (aStr == NULL || bStr == NULL) return FALSE;

  while (aStr[i] != '\0' && bStr[i] != '\0' && toupper(aStr[i]) == toupper(bStr[i]))
    i++;

  /* if we terminated for any reason except the end of both strings return FALSE */
  if (aStr[i] != '\0' || bStr[i] != '\0')
    return FALSE;

  /* success */
  return TRUE;
}

/*
 * Checks if aStr is a prefix of bStr.
 */
bool is_prefix(const char *aStr, const char *bStr)
{
  /* NULL strings never compares */
  if (aStr == NULL || bStr == NULL) return FALSE;

  /* empty strings never compares */
  if (aStr[0] == '\0' || bStr[0] == '\0') return FALSE;

  /* check if aStr is a prefix of bStr */
  while (*aStr)
  {
    if (tolower(*aStr++) != tolower(*bStr++))
      return FALSE;
  }

  /* success */
  return TRUE;
}


// same as one_arg, but can take constants
const char *one_arg_safe(const char *fStr, char *bStr) {
  /* skip leading spaces */
  while (isspace(*fStr))
    fStr++; 

  /* copy the beginning of the string */
  while (*fStr != '\0')
  {
    /* have we reached the end of the first word ? */
    if (isspace(*fStr))
    {
      fStr++;
      break;
    }

    /* copy one char */
    *bStr++ = *fStr++;
  }

  /* terminate string */
  *bStr = '\0';

  /* skip past any leftover spaces */
  while (isspace(*fStr))
    fStr++;

  /* return the leftovers */
  return fStr;
}


char *one_arg(char *fStr, char *bStr)
{
  /* skip leading spaces */
  while (isspace(*fStr))
    fStr++; 

  char arg_end = ' ';

  // are we using quotation marks or commas to specify multiple words?
  if(*fStr == '"' || *fStr == '\'') {
    arg_end = *fStr;
    fStr++;
  }

  /* copy the beginning of the string */
  while (*fStr != '\0')
  {
    /* have we reached the end of the first word ? */
    if (*fStr == arg_end)
    {
      fStr++;
      break;
    }

    /* copy one char */
    *bStr++ = *fStr++;
  }

  /* terminate string */
  *bStr = '\0';

  /* skip past any leftover spaces */
  while (isspace(*fStr))
    fStr++;

  /* return the leftovers */
  return fStr;
}

char *two_args(char *from, char *arg1, char *arg2) {
  return one_arg(one_arg(from, arg1), arg2);
}

char *three_args(char *from, char *arg1, char *arg2, char *arg3) {
  return one_arg(one_arg(one_arg(from, arg1), arg2), arg3);
}

//
// pull out the argument of the specified number
//
void arg_num(const char *from, char *to, int num) {
  int count = 1;

  while(count < num && *from != '\0') {
    if(isspace(*from))
      count++;
    from++;
  }

  int i;
  // copy up to the first space
  for(i = 0; !isspace(from[i]) && from[i] != '\0'; i++)
    to[i] = from[i];

  // now cap our string
  to[i] = '\0';
}


char *capitalize(char *txt)
{
  static char buf[MAX_BUFFER];
  int size, i;

  buf[0] = '\0';

  if (txt == NULL || txt[0] == '\0')
    return buf;

  size = strlen(txt);

  for (i = 0; i < size; i++)
    buf[i] = toupper(txt[i]);
  buf[size] = '\0';

  return buf;
}

char *strfind(char *txt, char *sub)
{
  int i, j;

  int len = strlen(txt) - strlen(sub);

  for(i = 0; i <= len; i++) {
    if(txt[i] == sub[0]) {
      bool found = TRUE;

      for(j = 1; sub[j] != '\0'; j++) {
	if(sub[j] != txt[i+j]) {
	  found = FALSE;
	  break;
	}
      }

      if(found)
	return &txt[i];

      i += j;
    }
  }

  return NULL;
}
