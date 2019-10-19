//*****************************************************************************
//
// parse.h
//
// Player commands usually each take their own strict format. For instance,
// the give command takes the format give <player> <object>. However, having to
// parse out these arguments by hand in each command is a bit of a hassle and
// something we can probably cut out most of the legwork for. That's what the
// functions in this header are designed to do.
//
//*****************************************************************************
#include "mud.h"
#include "utils.h"
#include "handler.h"
#include "inform.h"
#include "parse.h"

// mandatory modules
#include "scripts/scripts.h"
#include "scripts/pyexit.h"



//*****************************************************************************
// local datastructures, variables, and defines
//*****************************************************************************
#define PARSE_TOKEN_CHAR       0
#define PARSE_TOKEN_OBJ        1
#define PARSE_TOKEN_ROOM       2
#define PARSE_TOKEN_EXIT       3
#define PARSE_TOKEN_MULTI      4
#define PARSE_TOKEN_FLAVOR     5
#define PARSE_TOKEN_WORD       6
#define PARSE_TOKEN_DOUBLE     7
#define PARSE_TOKEN_INT        8
#define PARSE_TOKEN_BOOL       9
#define PARSE_TOKEN_OPTIONAL  10
#define PARSE_TOKEN_STRING    11

#define PARSE_VAR_BOOL         0
#define PARSE_VAR_INT          1
#define PARSE_VAR_DOUBLE       2
#define PARSE_VAR_CHAR         4
#define PARSE_VAR_OBJ          5
#define PARSE_VAR_ROOM         6
#define PARSE_VAR_EXIT         7
#define PARSE_VAR_STRING       8



//
// data for one format argument
typedef struct {
  int             type; // ch, obj, room, string, int, double, etc...?
  bitvector_t    scope; // what scope arguments are supplied to find?
  bool          all_ok; // is all. syntax allowed?
  bool         self_ok; // if we're looking for a char, can we find ourself?
  LIST     *token_list; // if we're a multi-type, a list of our possible format
  char         *flavor; // if we're a flavor type, the string we accept
  bool flavor_optional; // pretty self-explanatory :)
} PARSE_TOKEN;


//
// create a new parse token of the specified type
PARSE_TOKEN *newParseToken(int type) {
  PARSE_TOKEN *token = calloc(1, sizeof(PARSE_TOKEN));
  token->type = type;
  if(type == PARSE_TOKEN_MULTI)
    token->token_list = newList();
  else if(type == PARSE_TOKEN_OBJ) {
    SET_BIT(token->scope, FIND_SCOPE_VISIBLE);
  }
  else if(type == PARSE_TOKEN_EXIT) {
    SET_BIT(token->scope, FIND_SCOPE_VISIBLE);
    SET_BIT(token->scope, FIND_SCOPE_ROOM);
  }
  else if(type == PARSE_TOKEN_CHAR) {
    SET_BIT(token->scope, FIND_SCOPE_VISIBLE);
    token->self_ok = TRUE;
  }
  return token;
}

PARSE_TOKEN *newParseTokenDescriptive(int type, const char *format) {
  PARSE_TOKEN *token = newParseToken(type);
  // do we have a describer between ( and )?
  if(endswith(format, ")") && strchr(format, '(')) {
    format = format + next_letter_in(format, '(') + 1;
    token->flavor = strdup(format);
    token->flavor[strlen(token->flavor)-1] = '\0';
  }
  return token;
}

//
// free a token from memory
void deleteParseToken(PARSE_TOKEN *arg) {
  if(arg->token_list)
    deleteListWith(arg->token_list, deleteParseToken);
  if(arg->flavor)
    free(arg->flavor);
  free(arg);
}



//
// data for one parsed variable
typedef struct {
  int               type; // the type we parsed out
  bool          bool_val; // bool value
  int            int_val; // integer value
  double         dbl_val; // double value
  void          *ptr_val; // pointer value (room, ch, obj, exit)
  int disambiguated_type; // our parse type (in parse.h) if we were parsed 
                          // from a set of multiple possible types
  bool multiple_possible; // was it possible to parse multiple variables
  bool          multiple; // were multiple things parsed
} PARSE_VAR;


//
// create a new parse var of the specified type
PARSE_VAR *newParseVar(int type) {
  PARSE_VAR          *var = calloc(1, sizeof(PARSE_VAR));
  var->disambiguated_type = PARSE_NONE;
  var->type               = type;
  return var;
}  


//
// delete a parse var
void deleteParseVar(PARSE_VAR *var) {
  free(var);
}



//*****************************************************************************
// local functions
//*****************************************************************************

//
// makes a char token, and parses options. Returns NULL if there's bad options
PARSE_TOKEN *parse_token_char(const char *options) {
  // make it
  PARSE_TOKEN *token = newParseTokenDescriptive(PARSE_TOKEN_CHAR, options);

  // search for options
  while(*options != '\0') {
    if(!strncasecmp(options, ".room", 5)) {
      options = options + 5;
      SET_BIT(token->scope, FIND_SCOPE_ROOM);
    }
    else if(!strncasecmp(options, ".world", 6)) {
      options = options + 6;
      SET_BIT(token->scope, FIND_SCOPE_WORLD);
    }
    else if(!strncasecmp(options, ".multiple", 9)) {
      options = options + 9;
      token->all_ok = TRUE;
    }
    else if(!strncasecmp(options, ".noself", 7)) {
      options = options + 7;
      token->self_ok = FALSE;
    }
    else if(!strncasecmp(options, ".invis_ok", 9)) {
      options = options + 9;
      REMOVE_BIT(token->scope, FIND_SCOPE_VISIBLE);
    }
    else if(*options == '(' && endswith(options, ")"))
      break;
    // didn't recognize the option
    else {
      deleteParseToken(token);
      token = NULL;
      break;
    }
  }

  // if we successfully parsed the token, make sure there's a scope to look in
  if(token != NULL && !IS_SET(token->scope, FIND_SCOPE_ROOM|FIND_SCOPE_WORLD)) {
    deleteParseToken(token);
    token = NULL;
  }

  return token;
}


//
// makes an obj token, and parses options. Returns NULL if there's bad options
PARSE_TOKEN *parse_token_obj(const char *options) {
  // make it
  PARSE_TOKEN *token = newParseTokenDescriptive(PARSE_TOKEN_OBJ, options);

  // search for options
  while(*options != '\0') {
    if(!strncasecmp(options, ".room", 5)) {
      options = options + 5;
      SET_BIT(token->scope, FIND_SCOPE_ROOM);
    }
    else if(!strncasecmp(options, ".world", 6)) {
      options = options + 6;
      SET_BIT(token->scope, FIND_SCOPE_WORLD);
    }
    else if(!strncasecmp(options, ".inv", 4)) {
      options = options + 4;
      SET_BIT(token->scope, FIND_SCOPE_INV);
    }
    else if(!strncasecmp(options, ".eq", 3)) {
      options = options + 3;
      SET_BIT(token->scope, FIND_SCOPE_WORN);
    }
    else if(!strncasecmp(options, ".multiple", 9)) {
      options = options + 9;
      token->all_ok = TRUE;
    }
    else if(!strncasecmp(options, ".invis_ok", 9)) {
      options = options + 9;
      REMOVE_BIT(token->scope, FIND_SCOPE_VISIBLE);
    }
    else if(*options == '(' && endswith(options, ")"))
      break;
    // didn't recognize the option
    else {
      deleteParseToken(token);
      token = NULL;
      break;
    }
  }

  // if we successfully parsed the token, make sure there's a scope to look in
  if(token != NULL && !IS_SET(token->scope, FIND_SCOPE_ROOM | FIND_SCOPE_WORLD |
			                    FIND_SCOPE_INV  | FIND_SCOPE_WORN)){
    deleteParseToken(token);
    token = NULL;
  }

  return token;
}


//
// makes an exit token, and parses options. Returns NULL if there's bad options
PARSE_TOKEN *parse_token_exit(const char *options) {
  // make it
  PARSE_TOKEN *token = newParseTokenDescriptive(PARSE_TOKEN_EXIT, options);

  // search for options
  while(*options != '\0') {
    if(!strncasecmp(options, ".multiple", 9)) {
      options = options + 9;
      token->all_ok = TRUE;
    }
    else if(!strncasecmp(options, ".invis_ok", 9)) {
      options = options + 9;
      REMOVE_BIT(token->scope, FIND_SCOPE_VISIBLE);
    }
    else if(*options == '(' && endswith(options, ")"))
      break;
    // didn't recognize the option
    else {
      deleteParseToken(token);
      token = NULL;
      break;
    }
  }

  return token;
}


//
// parses one actual thing (int, double, bool, string, ch, obj, room) marker.
// Does not understand { }, ( ), [ ], |, or *. Assumes no leading whitespaces.
PARSE_TOKEN *parse_one_datatype(const char **format) {
  char buf[SMALL_BUFFER];
  const char    *fmt = *format;
  PARSE_TOKEN *token = NULL;
  int i = 0;

  // copy what we're trying to parse
  for(i = 0; isalpha(*fmt) || (strchr("_.()", *fmt) && *fmt); i++, fmt++)
    // *fmt == '_' || *fmt == '.'; i++, fmt++)
    buf[i] = *fmt;
  buf[i] = '\0';

  // figure out our type
  if(startswith(buf, "int"))
    token = newParseTokenDescriptive(PARSE_TOKEN_INT, buf);
  else if(startswith(buf, "double"))
    token = newParseTokenDescriptive(PARSE_TOKEN_DOUBLE, buf);
  else if(startswith(buf, "bool"))
    token = newParseTokenDescriptive(PARSE_TOKEN_BOOL, buf);
  else if(startswith(buf, "word"))
    token = newParseTokenDescriptive(PARSE_TOKEN_WORD, buf);
  else if(startswith(buf, "string"))
    token = newParseTokenDescriptive(PARSE_TOKEN_STRING, buf);
  else if(startswith(buf, "room"))
    token = newParseTokenDescriptive(PARSE_TOKEN_ROOM, buf);

  // exits, chars, and objs can have arguments tagged to the end of them. For
  // exits, it is optional. For both chars and objs, it is neccessary to specify
  // at least the scope of search (e.g. ch.room, obj.inv, obj.room.inv)
  else if(!strcasecmp(buf, "exit") || !strncasecmp(buf, "exit.", 5))
    token = parse_token_exit(buf+4);
  else if(!strncasecmp(buf, "ch.", 3))
    token = parse_token_char(buf+2);
  else if(!strncasecmp(buf, "obj.", 4))
    token = parse_token_obj(buf+3);

  // up the location of format if we successfully parsed something
  if(token != NULL)
    *format = fmt;

  // return whatever we found
  return token;
}


//
// parses a multi token assumes no leading whitespace
PARSE_TOKEN *parse_multi_token(const char **format) {
  const char *fmt = *format;
  bool close_found = FALSE;

  // skip our opening {
  fmt++;

  // skip any leading whitespace
  while(isspace(*fmt))
    fmt++;

  // make our multi-token
  PARSE_TOKEN *multi_token = newParseToken(PARSE_TOKEN_MULTI);

  // parse all of our tokens and add them to the multi-list
  while(*fmt != '\0' && !close_found) {
    PARSE_TOKEN *one_token = parse_one_datatype(&fmt);

    // did we parse the token alright?
    if(one_token != NULL) {
      listQueue(multi_token->token_list, one_token);
      // up the positioning of format
      *format = fmt;
    }
    // syntactic error. break out!
    else
      break;

    // skip any whitespace
    while(isspace(*fmt))
      fmt++;

    // have we encountered the closing bracket?
    if(*fmt == '}')
      close_found = TRUE;
  }

  // if we never found a close, or we didn't parse any arguments,
  // delete the token because it was not finished
  if(close_found == FALSE || listSize(multi_token->token_list) == 0) {
    deleteParseToken(multi_token);
    multi_token = NULL;
  }
  // otherwise, skip the closing bracket and up the position of format
  else {
    fmt++;
    *format = fmt;
  }

  return multi_token;
}


//
// parses a flavor token
PARSE_TOKEN *parse_flavor_token(const char **format, bool optional) {
  // make it
  char buf[SMALL_BUFFER];
  PARSE_TOKEN     *token = newParseToken(PARSE_TOKEN_FLAVOR);
  token->flavor_optional = optional;
  char       open_marker = (optional ? '[' : '<');
  char      close_marker = (optional ? ']' : '>');
  const char        *fmt = *format;
  int      i, open_count = 1;

  // skip the opening marker
  fmt++;

  // copy the flavor over to our buffer
  for(i = 0; *fmt != '\0'; i++, fmt++) {
    // did we encounter an open marker? if so, up our open count
    if(*fmt == open_marker)
      open_count++;
    // did we encounter a close marker? if so, lower our open count
    else if(*fmt == close_marker)
      open_count--;
    // did we just close everything off? if so, break out and skip last token
    if(open_count == 0) {
      fmt++;
      break;
    }
    buf[i] = *fmt;
  }
  buf[i] = '\0';

  // make sure we closed everything off
  if(open_count > 0) {
    deleteParseToken(token);
    token = NULL;
  }
  // move our format up and copy over the flavor string
  else {
    token->flavor = strdup(buf);
    *format = fmt;
  }

  return token;
}


//
// parse an optional marker
PARSE_TOKEN *parse_optional_token(const char **format) {
  // skip over the optional marker
  (*format)++;
  return newParseToken(PARSE_TOKEN_OPTIONAL);
}


//
// parses out one argument. Returns NULL if an error was encountered. If there
// was no error, ups the location of fmt to the end of the last argument parsed
PARSE_TOKEN *parse_one_token(const char **format) {
  const char    *fmt = *format;
  PARSE_TOKEN *token = NULL;

  // skip whitespaces
  while(isspace(*fmt))
    fmt++;

  // are we trying to parse multiple arguments?
  if(*fmt == '{')
    token = parse_multi_token(&fmt);

  // optional flavor format
  else if(*fmt == '[')
    token = parse_flavor_token(&fmt, TRUE);

  // mandatory flavor format
  else if(*fmt == '<')
    token = parse_flavor_token(&fmt, FALSE);

  // optional marker
  else if(*fmt == '|')
    token = parse_optional_token(&fmt);

  // a datatype token
  else
    token = parse_one_datatype(&fmt);

  // up the location of our format
  *format = fmt;
  
  return token;
}


//
// breaks up a format string into its parts and returns a list of them
LIST *decompose_parse_format(const char *format) {
  const char  *fmt = format; 
  bool       error = FALSE;
  LIST *token_list = newList();

  // try to parse all of our format down into tokens
  while(*fmt != '\0' && !error) {
    PARSE_TOKEN *token = parse_one_token(&fmt);

    // did the token parse OK?
    if(token != NULL)
      listQueue(token_list, token);
    else
      error = TRUE;
  }

  // did we encounter an error?
  if(error == TRUE) {
    deleteListWith(token_list, deleteParseToken);
    token_list = NULL;
  }

  return token_list;
}

//
// turns a string into a boolean
bool string_to_bool(const char *string) {
  if(!strcasecmp(string, "yes") || !strcasecmp(string, "true"))
    return TRUE;
  return FALSE;
}


//
// returns whether or not there is a boolean value available at the 
// head of the string.
bool string_is_bool(const char *string) {
  return (!strcasecmp(string, "yes") || !strcasecmp(string, "true")  || 
	  !strcasecmp(string, "no")  || !strcasecmp(string, "false"));
}


//
// returns whether or not the string is a double value
bool string_is_double(const char *string) {
  bool deci_found = FALSE;
  int     len = 0;
  // check if it's negative
  if(*string == '-')
    string++;
  // it's a double if we only encounter digits and a single dicimal
  for(; *string != '\0'; string++, len++) {
    if(isdigit(*string))
      continue;
    // only allowed decimals, and only one of them...
    else if(*string != '.' || deci_found == TRUE)
      return FALSE;
    deci_found = TRUE;
  }
  // found no problems, and actually found _something_
  return (len > 0);
}


//
// returns whether or not the string is an integer
bool string_is_int(const char *string) {
  int len = 0;
  // check if it's a negative
  if(*string == '-')
    string++;
  // it's an int if all we encounter is digits
  for(; *string != '\0'; string++, len++)
    if(!isdigit(*string))
      return FALSE;
  // found no problems, and actually found _something_
  return (len > 0);
}


//
// tries to make an int parse var
PARSE_VAR *use_one_parse_token_int(const char *buf) {
  PARSE_VAR *var = NULL;
  if(string_is_int(buf)) {
    var = newParseVar(PARSE_VAR_INT);
    var->int_val = atoi(buf);
  }
  return var;
}


//
// tries to make a double parse var
PARSE_VAR *use_one_parse_token_double(const char *buf) {
  PARSE_VAR *var = NULL;
  if(string_is_double(buf)) {
    var = newParseVar(PARSE_VAR_DOUBLE);
    var->dbl_val = atof(buf);
  }
  return var;
}


//
// tries to make a boolean parse var
PARSE_VAR *use_one_parse_token_bool(const char *buf) {
  PARSE_VAR *var = NULL;
  if(string_is_bool(buf)) {
    var = newParseVar(PARSE_VAR_BOOL);
    var->bool_val = string_to_bool(buf);
  }
  return var;
}


//
// tries to make a char parse var
PARSE_VAR *use_one_parse_token_char(CHAR_DATA *looker, PARSE_TOKEN *tok,
				    const char *name) {
  int    type = FOUND_NONE;
  void *found = find_specific(looker, name, "", "", FIND_TYPE_CHAR, tok->scope,
			      tok->all_ok, &type);

  // make sure we found something...
  if(found == NULL)
    return NULL;
  else {
    PARSE_VAR *var = newParseVar(PARSE_VAR_CHAR);

    // if multiple vals were possible, flag it
    var->multiple_possible = tok->all_ok;

    // make sure it's not us if that's not allowed
    if(type == FOUND_CHAR) {
      if(tok->self_ok || looker != found)
	var->ptr_val = found;
      else {
	deleteParseVar(var);
	var = NULL;
      }
    }
    // if we got a list, make sure we remove ourself as neccessary
    else if(type == FOUND_LIST) {
      if(!tok->self_ok)
	listRemove(found, looker);
      // make sure we're not empty...
      if(listSize(found) > 1) {
	var->ptr_val  = found;
	var->multiple = TRUE;
      }
      else if(listSize(found) == 1) {
	var->ptr_val = listPop(found);
	deleteList(found);
      }
      else {
	deleteList(found);
	deleteParseVar(var);
	var = NULL;
      }
    }
    // this shouldn't happen...
    else {
      deleteParseVar(var);
      var = NULL;
    }

    // return what we found, if anything
    return var;
  }
}


//
// tries to make an obj parse var
PARSE_VAR *use_one_parse_token_obj(CHAR_DATA *looker, PARSE_TOKEN *tok,
				   const char *name) {
  int    type = FOUND_NONE;
  void *found = find_specific(looker, name, "", "", FIND_TYPE_OBJ, tok->scope,
			      tok->all_ok, &type);

  // make sure we found something
  if(found == NULL)
    return NULL;
  else {
    PARSE_VAR *var = newParseVar(PARSE_VAR_OBJ);

    // if multiple vals were possible, flag it
    var->multiple_possible = tok->all_ok;

    // Is it a single item?
    if(type == FOUND_OBJ)
      var->ptr_val = found;

    // or is it multiple items?
    else if(type == FOUND_LIST) {
      if(listSize(found) > 1) {
	var->ptr_val = found;
	var->multiple = TRUE;
      }
      else if(listSize(found) == 1) {
	var->ptr_val = listPop(found);
	deleteList(found);
      }
      else {
	deleteList(found);
	deleteParseVar(var);
	var = NULL;
      }
    }

    // We should never reach this case
    else {
      deleteParseVar(var);
      var = NULL;
    }

    // return whatever we found
    return var;
  }
}


//
// tries to make an exit parse var
PARSE_VAR *use_one_parse_token_exit(CHAR_DATA *looker, PARSE_TOKEN *tok, 
				    const char *name) {
  int    type = FOUND_NONE;
  void *found = find_specific(looker, name, "", "", FIND_TYPE_EXIT, tok->scope,
			      tok->all_ok, &type);

  // make sure we found something
  if(found == NULL)
    return NULL;
  else {
    PARSE_VAR *var = newParseVar(PARSE_VAR_EXIT);

    // if multiple vals were possible, flag it
    var->multiple_possible = tok->all_ok;

    // Is it a single exit?
    if(type == FOUND_EXIT)
      var->ptr_val = found;

    // or is it multiple exits?
    else if(type == FOUND_LIST) {
      if(listSize(found) > 1) {
	var->ptr_val = found;
	var->multiple = TRUE;
      }
      else if(listSize(found) == 1) {
	var->ptr_val = listPop(found);
	deleteList(found);
      }
      else {
	deleteList(found);
	deleteParseVar(var);
	var = NULL;
      }
    }

    // We should never reach this case
    else {
      deleteParseVar(var);
      var = NULL;
    }

    // return whatever we found
    return var;
  }
}


//
// tries to make a room parse var
PARSE_VAR *use_one_parse_token_room(CHAR_DATA *looker, PARSE_TOKEN *tok,
				    const char *name) {
  ROOM_DATA *room = find_specific(looker, name, "", "", FIND_TYPE_ROOM, 
				  FIND_SCOPE_WORLD, FALSE, NULL);

  // did we find something?
  if(room == NULL)
    return NULL;
  else {
    PARSE_VAR *var = newParseVar(PARSE_VAR_ROOM);
    var->ptr_val   = room;
    return var;
  }
}


//
// Tries to apply a token to args, to parse something out. Returns a PARSE_VAR
// if the token's usage resulted in the creation of a variable. If an error was
// encountered, make this apparent by setting the value of the variable at error
PARSE_VAR *use_one_parse_token(CHAR_DATA *looker, PARSE_TOKEN *tok, 
			       char **args, bool *error, char *err_buf) {
  char buf[SMALL_BUFFER];
  PARSE_VAR  *var = NULL;
  char       *arg = *args;

  // skip over any leading spaces we might have
  while(isspace(*arg))
    arg++;

  switch(tok->type) {
  case PARSE_TOKEN_MULTI: {
    // make a proxy error value and error buf so we don't accidentally fill
    // these up when it turns out we find something on a var past the first
    bool multi_err = FALSE;
    char multi_err_buf[SMALL_BUFFER] = "";

    // go through all of our possible types until we find something
    LIST_ITERATOR *multi_i = newListIterator(tok->token_list);
    PARSE_TOKEN      *mtok = NULL;
    bool multiple_possible = FALSE;
    ITERATE_LIST(mtok, multi_i) {
      if(mtok->all_ok)
	multiple_possible = TRUE;
      var = use_one_parse_token(looker, mtok, &arg, &multi_err, multi_err_buf);
      // reset our error value for the next pass at it...
      if(var == NULL)
	multi_err = FALSE;
      // found something! Disambiguate the type
      else {
	switch(mtok->type) {
	case PARSE_TOKEN_CHAR:    var->disambiguated_type = PARSE_CHAR;   break;
	case PARSE_TOKEN_ROOM:    var->disambiguated_type = PARSE_ROOM;   break;
	case PARSE_TOKEN_EXIT:    var->disambiguated_type = PARSE_EXIT;   break;
	case PARSE_TOKEN_OBJ:     var->disambiguated_type = PARSE_OBJ;    break;
	case PARSE_TOKEN_WORD:    var->disambiguated_type = PARSE_STRING; break;
	case PARSE_TOKEN_STRING:  var->disambiguated_type = PARSE_STRING; break;
	case PARSE_TOKEN_INT:     var->disambiguated_type = PARSE_INT;    break;
	case PARSE_TOKEN_DOUBLE:  var->disambiguated_type = PARSE_DOUBLE; break;
	case PARSE_TOKEN_BOOL:    var->disambiguated_type = PARSE_BOOL;   break;
	}
	// break out of the loop... we found something
	break;
      }
    } deleteListIterator(multi_i);

    // did we manage not to find something?
    if(var != NULL)
      var->multiple_possible = multiple_possible;
    else {
      one_arg(arg, buf); // get the first arg, for reporting...
      sprintf(err_buf, "Your argument '%s' was invalid or could not be found.",
	      buf);
      *error = TRUE;
    }
    break;
  }

  case PARSE_TOKEN_FLAVOR: {
    int len = strlen(tok->flavor);
    // have we found the flavor text?
    if(strncasecmp(tok->flavor, arg, len) == 0 &&
	 (arg[len] == '\0' || isspace(arg[len])))
      arg = arg + len;
    // do we need to do something about it?
    else if(!tok->flavor_optional)
      *error = TRUE;
    break;
  }

    // parse out a char value
  case PARSE_TOKEN_CHAR:
    arg = one_arg(arg, buf);
    var = use_one_parse_token_char(looker, tok, buf);
    if(var == NULL) {
      sprintf(err_buf, "The person, %s, could not be found.", buf);
      *error = TRUE;
    }
    break;

    // parse out an obj value
  case PARSE_TOKEN_OBJ:
    arg = one_arg(arg, buf);
    var = use_one_parse_token_obj(looker, tok, buf);
    if(var == NULL) {
      sprintf(err_buf, "The object, %s, could not be found.", buf);
      *error = TRUE;
    }
    break;

    // parse out a room value
  case PARSE_TOKEN_ROOM:
    arg = one_arg(arg, buf);
    var = use_one_parse_token_room(looker, tok, buf);
    if(var == NULL) {
      sprintf(err_buf, "The room, %s, could not be found.", buf);
      *error = TRUE;
    }
    break;

    // parse out an exit value
  case PARSE_TOKEN_EXIT:
    arg = one_arg(arg, buf);
    var = use_one_parse_token_exit(looker, tok, buf);
    if(var == NULL) {
      sprintf(err_buf, "The direction, %s, could not be found.", buf);
      *error = TRUE;
    }
    break;

    // try to parse out a double value
  case PARSE_TOKEN_DOUBLE:
    arg = one_arg(arg, buf);
    var = use_one_parse_token_double(buf);
    if(var == NULL) {
      sprintf(err_buf, "'%s' is not a decimal value.", buf);
      *error = TRUE;
    }
    break;

    // try to parse out an integer value
  case PARSE_TOKEN_INT:
    arg = one_arg(arg, buf);
    var = use_one_parse_token_int(buf);
    if(var == NULL) {
      sprintf(err_buf, "'%s' is not a%s number.", buf,
	      (string_is_double(buf) ? "n acceptable" : ""));
      *error = TRUE;
    }
    break;

    // try to parse out a boolean value
  case PARSE_TOKEN_BOOL:
    arg = one_arg(arg, buf);
    var = use_one_parse_token_bool(buf);
    if(var == NULL) {
      sprintf(err_buf, "'%s' is not a yes/no value.", buf);
      *error = TRUE;
    }
    break;

    // parse out a single word
  case PARSE_TOKEN_WORD: {
    var = newParseVar(PARSE_VAR_STRING);
    var->ptr_val    = arg;
    bool multi_word = FALSE;
    char multi_mark = '"';

    // are we using quotation marks to specify multiple words?
    if(*arg == '"' || *arg == '\'') {
      multi_word = TRUE;
      multi_mark = *arg;
      arg++;
      var->ptr_val = arg;
    }

    // go through arg to the next space, and delimit the word
    for(; *arg != '\0'; arg++) {
      if((multi_word && *arg == multi_mark) || (!multi_word && isspace(*arg))) {
	*arg = '\0';
	arg++;
	break;
      }
    }
    break;
  }

    // copies whatever is left
  case PARSE_TOKEN_STRING:
    var = newParseVar(PARSE_VAR_STRING);
    var->ptr_val = arg;
    // skip up the place of the arg...
    while(*arg != '\0')
      arg++;
    break;

    // since this doesn't really parse a value...
  case PARSE_TOKEN_OPTIONAL:
    break;
  }

  // up the placement of our arg if we didn't encounter an error
  if(!*error)
    *args = arg;

  return var;
}


//
// gets the name of the type for printing in syntax error messages
const char *get_datatype_format_error_mssg(PARSE_TOKEN *tok) {
  if(tok->flavor)
    return tok->flavor;

  switch(tok->type) {
  case PARSE_TOKEN_CHAR:
    return "person";
  case PARSE_TOKEN_ROOM:
    return "room";
  case PARSE_TOKEN_EXIT:
    return "direction";
  case PARSE_TOKEN_OBJ:
    return "object";
  case PARSE_TOKEN_INT:
    return "number";
  case PARSE_TOKEN_DOUBLE:
    return "decimal";
  case PARSE_TOKEN_BOOL:
    return "yes/no";
  case PARSE_TOKEN_WORD:
    return "word";
  case PARSE_TOKEN_STRING:
    return "text";
  default:
    return "UNKNOWN_TYPE";
  }
}


//
// Takes a list of tokens, and builds the proper syntax for the command and
// then sends it to the character.
void show_parse_syntax_error(CHAR_DATA *ch, const char *cmd, LIST *tokens) {
  BUFFER            *buf = newBuffer(1);
  LIST_ITERATOR   *tok_i = newListIterator(tokens);
  PARSE_TOKEN       *tok = NULL;
  bool    optional_found = FALSE;
  int              count = 0;

  // go through all of our tokens, and append their syntax to the buf
  ITERATE_LIST(tok, tok_i) {
    // make sure we add a space before anything else...
    if(count > 0)
      bprintf(buf, " ");
    count++;

    // have we encountered the "optional" marker? if so, switch our open/close
    if(tok->type == PARSE_TOKEN_OPTIONAL) {
      bprintf(buf, "[");
      optional_found = TRUE;
      // we don't want to put a space right after this [
      count = 0;
      continue;
    }

    // append our message
    switch(tok->type) {
    case PARSE_TOKEN_MULTI: {
      bprintf(buf, "<");
      LIST_ITERATOR *multi_i = newListIterator(tok->token_list);
      PARSE_TOKEN      *mtok = NULL;
      int            m_count = 0;
      ITERATE_LIST(mtok, multi_i) {
	if(m_count > 0)
	  bprintf(buf, ", ");
	m_count++;
	bprintf(buf, "%s", get_datatype_format_error_mssg(mtok));
      } deleteListIterator(multi_i);
      bprintf(buf, ">");
      break;
    }

    case PARSE_TOKEN_FLAVOR:
      bprintf(buf, "%s%s%s", (tok->flavor_optional ? "[" : ""), tok->flavor,
	      (tok->flavor_optional ? "]" : ""));
      break;

    default:
      bprintf(buf, "<%s>", get_datatype_format_error_mssg(tok));
      break;
    }
  } deleteListIterator(tok_i);

  // send the message
  send_to_char(ch, "Proper syntax is: %s %s%s\r\n", cmd, bufferString(buf),
	       (optional_found ? "]" : ""));
  deleteBuffer(buf);
}


//
// builds up a list of variables out of the token list, and arguments. If we
// encounter an error and we need to show it to the looker, do so.
LIST *compose_variable_list(CHAR_DATA *looker, LIST *tokens, char *args,
			    char *err_buf) {
  LIST      *variables = newList();
  LIST_ITERATOR *tok_i = newListIterator(tokens);
  PARSE_TOKEN     *tok = NULL;
  bool           error = FALSE;
  bool  optional_found = FALSE;

  // go through our list of tokens, and try dealing with the args
  ITERATE_LIST(tok, tok_i) {
    PARSE_VAR *var = NULL;

    // did we just encounter an optional value?
    if(tok->type == PARSE_TOKEN_OPTIONAL) {
      optional_found = TRUE;
      continue;
    }
    
    // can we still use tokens to process stuff?
    if(*args != '\0')
      var = use_one_parse_token(looker, tok, &args, &error, err_buf);
    // we haven't found an "optional" marker yet - this isn't allowed
    else if(optional_found == FALSE)
      error = TRUE;
    // we have found an "optional" marker. Just break out of the loop
    else 
      break;

    // if use of the token returned a new variable, append it
    if(var != NULL)
      listQueue(variables, var);
    // if we enountered an error, tell the person if neccessary
    else if(error == TRUE) {
      deleteListWith(variables, deleteParseVar);
      variables = NULL;
      break;
    }
  } deleteListIterator(tok_i);

  return variables;
}


//
// Goes through the list of variables and fills up vargs with them as needed
void parse_assign_vars(LIST *variables, va_list vargs) {
  LIST_ITERATOR *var_i = newListIterator(variables);
  PARSE_VAR   *one_var = NULL;

  // go through each variable and assign to vargs as needed
  ITERATE_LIST(one_var, var_i) {
    // first, do our basic type
    switch(one_var->type) {
    case PARSE_VAR_BOOL:
      *va_arg(vargs, bool *) = one_var->bool_val;
      break;
    case PARSE_VAR_INT:
      *va_arg(vargs, int *) = one_var->int_val;
      break;
    case PARSE_VAR_DOUBLE:
      *va_arg(vargs, double *) = one_var->dbl_val;
      break;
    case PARSE_VAR_STRING:
    case PARSE_VAR_OBJ:
    case PARSE_VAR_CHAR:
    case PARSE_VAR_EXIT:
    case PARSE_VAR_ROOM:
      *va_arg(vargs, void **) = one_var->ptr_val;
      break;
    // this should never happen...
    default:
      break;
    }
    
    // now see if we have a multi_type
    if(one_var->disambiguated_type != PARSE_NONE)
      *va_arg(vargs, int *) = one_var->disambiguated_type;

    // and if we parsed multiple occurences
    if(one_var->multiple_possible == TRUE)
      *va_arg(vargs, bool *) = one_var->multiple;
  } deleteListIterator(var_i);
}


//
// Goes through the list of variables and make a python list with them
PyObject *parse_create_py_vars(LIST *variables) {
  LIST_ITERATOR *var_i = newListIterator(variables);
  PARSE_VAR   *one_var = NULL;
  PyObject       *list = PyList_New(0);
  PyObject      *pyval = NULL;

  // go through each variable and assign to vargs as needed
  ITERATE_LIST(one_var, var_i) {
    // first, do our basic type
    switch(one_var->type) {
    case PARSE_VAR_BOOL:
      pyval = Py_BuildValue("b", one_var->bool_val);
      PyList_Append(list, pyval);
      Py_DECREF(pyval);
      break;
    case PARSE_VAR_INT:
      pyval = Py_BuildValue("i", one_var->int_val);
      PyList_Append(list, pyval);
      Py_DECREF(pyval);
      break;
    case PARSE_VAR_DOUBLE:
      pyval = Py_BuildValue("f", one_var->dbl_val);
      PyList_Append(list, pyval);
      Py_DECREF(pyval);
      break;
    case PARSE_VAR_STRING:
      pyval = Py_BuildValue("s", one_var->ptr_val);
      PyList_Append(list, pyval);
      Py_DECREF(pyval);
      break;
    case PARSE_VAR_OBJ:
      if(one_var->multiple == FALSE)
	PyList_Append(list, objGetPyFormBorrowed(one_var->ptr_val));
      else {
	pyval = PyList_fromList(one_var->ptr_val, objGetPyForm);
	PyList_Append(list, pyval);
	Py_DECREF(pyval);
      }
      break;
    case PARSE_VAR_CHAR:
      if(one_var->multiple == FALSE)
	PyList_Append(list, charGetPyFormBorrowed(one_var->ptr_val));
      else {
	pyval = PyList_fromList(one_var->ptr_val, charGetPyForm);
	PyList_Append(list, pyval);
	Py_DECREF(pyval);
      }
      break;
    case PARSE_VAR_EXIT:
      pyval = newPyExit(one_var->ptr_val);
      PyList_Append(list, pyval);
      Py_DECREF(pyval);
      break;
    case PARSE_VAR_ROOM:
      PyList_Append(list, roomGetPyFormBorrowed(one_var->ptr_val));
      break;
    // this should never happen...
    default:
      break;
    }
    
    // now see if we have a multi_type
    if(one_var->disambiguated_type != PARSE_NONE) {
      pyval = 
	Py_BuildValue("s", (one_var->disambiguated_type==PARSE_CHAR?"char":
			    (one_var->disambiguated_type==PARSE_ROOM?"room":
			     (one_var->disambiguated_type==PARSE_OBJ?"obj":
			      (one_var->disambiguated_type==PARSE_EXIT?"exit":
			       NULL)))));
      PyList_Append(list, pyval);
      Py_DECREF(pyval);
    }

    // and if we parsed multiple occurences
    if(one_var->multiple_possible == TRUE) {
      pyval = Py_BuildValue("b", one_var->multiple);
      PyList_Append(list, pyval);
      Py_DECREF(pyval);
    }
  } deleteListIterator(var_i);

  return list;
}



//*****************************************************************************
// implementation of parse.h
//*****************************************************************************
int parse_expected_py_args(const char *syntax) {
  LIST *tokens = NULL;
  if((tokens = decompose_parse_format(syntax)) == NULL)
    return -1;
  int count = 0;
  LIST_ITERATOR *token_i = newListIterator(tokens);
  PARSE_TOKEN     *token = NULL;
  ITERATE_LIST(token, token_i) {
    if(token->type == PARSE_TOKEN_FLAVOR || token->type == PARSE_TOKEN_OPTIONAL)
      continue;

    // one for a returnable type
    count++;

    // one to denote what kind in an ambiguous case
    if(token->type == PARSE_TOKEN_MULTI)
      count++;

    // one to denote if we had multiples
    if(token->all_ok)
      count++;
  } deleteListIterator(token_i);
  deleteListWith(tokens, deleteParseToken);

  return count;
}

void *Py_parse_args(CHAR_DATA *looker, bool show_errors, const char *cmd, 
		    char *args, const char *syntax) {
  char err_buf[SMALL_BUFFER] = "";
  bool       parse_ok = TRUE;
  LIST        *tokens = NULL;
  LIST     *variables = NULL;
  PyObject      *list = NULL;

  // get our list of tokens
  if((tokens = decompose_parse_format(syntax)) == NULL) {
    log_string("Command '%s', format error in argument parsing: %s",cmd,syntax);
    parse_ok = FALSE;
  }
  // try to use our tokens to compose a variable list
  else if((variables = compose_variable_list(looker, tokens, args, err_buf))
	  == NULL)
    parse_ok = FALSE;
  else {
    // go through all of our vars and make python forms for them
    list = parse_create_py_vars(variables);

    // fill up optional spots at the end we didn't parse args for
    int expected = parse_expected_py_args(syntax);
    while(PyList_Size(list) < expected)
      PyList_Append(list, Py_None);
  }

  // did we encounter an error with the arguments and need to mssg someone?
  if(tokens != NULL && !parse_ok && show_errors) {
    // do we have a specific error message?
    if(*err_buf)
      send_to_char(looker, "%s\r\n", err_buf);
    // assume a syntax error
    else
      show_parse_syntax_error(looker, cmd, tokens);
  }

  // clean up our mess
  if(tokens != NULL)
    deleteListWith(tokens, deleteParseToken);
  if(variables != NULL)
    deleteListWith(variables, deleteParseVar);

  // return our parse status
  return list;
}

bool parse_args(CHAR_DATA *looker, bool show_errors, const char *cmd,
		char *args, const char *syntax, ...) {
  char err_buf[SMALL_BUFFER] = "";
  bool       parse_ok = TRUE;
  LIST       *tokens  = NULL;
  LIST     *variables = NULL;

  // get our list of tokens
  if((tokens = decompose_parse_format(syntax)) == NULL) {
    log_string("Command '%s', format error in argument parsing: %s",cmd,syntax);
    parse_ok = FALSE;
  }
  // try to use our tokens to compose a variable list
  else if((variables = compose_variable_list(looker, tokens, args, err_buf))
	  == NULL)
    parse_ok = FALSE;
  else {
    // go through all of our vars and assign them to the proper args
    va_list vargs;
    va_start(vargs, syntax);
    parse_assign_vars(variables, vargs);
    va_end(vargs);
  }

  // did we encounter an error with the arguments and need to mssg someone?
  if(tokens != NULL && !parse_ok && show_errors) {
    // do we have a specific error message?
    if(*err_buf)
      send_to_char(looker, "%s\r\n", err_buf);
    // assume a syntax error
    else
      show_parse_syntax_error(looker, cmd, tokens);
  }

  // clean up our mess
  if(tokens != NULL)
    deleteListWith(tokens, deleteParseToken);
  if(variables != NULL)
    deleteListWith(variables, deleteParseVar);

  // return our parse status
  return parse_ok;
}
