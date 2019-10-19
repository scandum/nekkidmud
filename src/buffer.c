//*****************************************************************************
//
// buffer.c
//
// The buffer datastructure is a string-like thing of infinite length. Contains
// some basic utilities for interacting with it.
//
//*****************************************************************************

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mud.h"
#include "utils.h"
#include "buffer.h"


struct buffer_data {
  char *data;   // what we're holding
  int  maxlen;  // what's the maximum storage capacity of data
  int  len;     // what's the current content length of data
};

BUFFER    *newBuffer   (int start_capacity) {
  BUFFER *buf = malloc(sizeof(BUFFER));
  if(start_capacity <= 0) start_capacity = 1;
  buf->data   = malloc(sizeof(char) * start_capacity);
  *buf->data  = '\0';
  buf->maxlen = start_capacity;
  buf->len    = 0;
  return buf;
}

void        deleteBuffer(BUFFER *buf) {
  if(buf->data) free(buf->data);
  free(buf);
}

void        bufferExpand(BUFFER *buf, int newsize) {
  char *newdata = malloc(sizeof(char) * newsize);
  strcpy(newdata, buf->data);
  free(buf->data);
  buf->maxlen = newsize;
  buf->data = newdata;
}

void        bufferCat   (BUFFER *buf, const char *txt) {
  int txtlen = strlen(txt);
  // see if we need to expand the size of the buffer
  if(txtlen + buf->len >= buf->maxlen)
    bufferExpand(buf, ((txtlen + buf->len) * 5) / 4 + 20); 
  // copy the new text over
  strcat(buf->data+buf->len, txt);
  buf->len += txtlen;
}

void        bufferCatCh (BUFFER *buf, const char ch) {
  static char tmp[2];
  tmp[0] = ch; tmp[1] = '\0';
  bufferCat(buf, tmp);
}

void        bufferClear (BUFFER *buf) {
  *buf->data = '\0';
  buf->len = 0;
}

BUFFER    *bufferCopy  (BUFFER *buf) {
  BUFFER *newbuf = newBuffer(bufferLength(buf)+1);
  bufferCopyTo(buf, newbuf);
  return newbuf;
}

void        bufferCopyTo(BUFFER *from, BUFFER *to) {
  bufferClear(to);
  bufferCat(to, bufferString(from));
}

const char *bufferString(BUFFER *buf) {
  return buf->data;
}

int         bufferLength(BUFFER *buf) {
  return buf->len;
}

int vbprintf(BUFFER *buf, const char *fmt, va_list va) {
  static int printsize = 8192;
  char buftmp[printsize];
  int res = vsnprintf(buftmp, printsize, fmt, va);

  if (res >= printsize - 1)
    *buftmp = '\0';
  else
    bufferCat(buf, buftmp);
   
  return res;
}

int bprintf(BUFFER *buf, const char *fmt, ...) {  
  va_list va;
  va_start(va, fmt);
  int res = vbprintf(buf, fmt, va);
  va_end(va);
  return res;
}

int bufferReplace(BUFFER *buf, const char *a, const char *b, int all) {
  // first, check if we'll need to expand the size of the buffer
  int a_len = strlen(a), b_len = strlen(b);
  int len_needed = buf->maxlen;
  int replaced = 0;

  if(b_len - a_len > 0) {
    int to_replace = (all ? 
		      count_occurences(buf->data, a) : 
		      strstr(buf->data, a) != NULL);

    // if we don't have any to replace, do nothing
    if(to_replace == 0)
      return 0;

    // if we won't have enough room, do the expansion
    if(to_replace * (b_len - a_len) + buf->len > buf->maxlen)
      len_needed = ((buf->maxlen + to_replace*(b_len-a_len))*5)/4 + 20;
  }

  int buf_i, tmp_i;
  char buftmp[len_needed];

  // go through and replace all the occurences we need to
  for(buf_i = tmp_i = 0; buf->data[buf_i] != '\0';) {
    // we found a match
    if(!strncmp(buf->data+buf_i, a, a_len)) {
      replaced++;
      strcpy(buftmp+tmp_i, b);
      buf_i += a_len;
      tmp_i += b_len;
      // exit if we only need to replace the first occurence
      if(all == 0) break;
    }
    else {
      buftmp[tmp_i] = buf->data[buf_i];
      tmp_i++; buf_i++;
    }
  }

  // continue filling up the tmpbuf (if we only replaced 1, and not all)
  for(; buf->data[buf_i] != '\0'; buf_i++, tmp_i++)
    buftmp[tmp_i] = buf->data[buf_i];

  // make sure the end of string marker is in place
  buftmp[tmp_i] = '\0';

  // copy over all the changes
  if(buf->maxlen >= len_needed) {
    strcpy(buf->data, buftmp);
    buf->len = tmp_i;
  }
  else {
    free(buf->data);
    buf->data = malloc(len_needed);
    strcpy(buf->data, buftmp);
    buf->maxlen = len_needed;
    buf->len    = tmp_i;
  }

  return replaced;
}


int bufferInsert(BUFFER *buf, const char *newline, int line) {
  // first, check if we'll need to expand the size of the buffer
  int line_len = strlen(newline);
  if(line_len + buf->len + 2 >= buf->maxlen) // +2 for \r\n
    bufferExpand(buf, ((line_len + buf->len + 2) * 5)/4 + 20);

  // insert it in
  char *start = line_start(buf->data, line);
  if(start != NULL) {
    char tmpbuf[buf->maxlen];
    sprintf(tmpbuf, "%s", start);
    sprintf(start, "%s\r\n", newline);
    strcpy(start+line_len+2, tmpbuf);
    buf->len = buf->len + line_len + 2;
    return TRUE;
  }

  return FALSE;
}

int bufferRemove(BUFFER *buf, int line) {
  char *start = line_start(buf->data, line);
  if(start == NULL) 
    return FALSE;

  int i;
  // copy over the line with everything from the end of the line onwards
  for(i = 0; start[i] != '\0'; i++) {
    if(start[i] == '\n') {
      i++;
      break;
    }
  }
  strcpy(start, start+i);
  buf->len = strlen(buf->data);
  return TRUE;
}

int bufferReplaceLine(BUFFER *buf, const char *newline, int line) {
  if(bufferRemove(buf, line))
    return bufferInsert(buf, newline, line);
  else
    return 0;
}

void bufferFormatPy(BUFFER *buf) {
  bufferReplace(buf, "\\", "\\\\", TRUE);
  bufferReplace(buf, "\n", "\\n",  TRUE);
  bufferReplace(buf, "\r", "\\r",  TRUE);
  bufferReplace(buf, "\"", "\\\"", TRUE);
}

void bufferFormatFromPy(BUFFER *buf) {
  bufferReplace(buf, "\\\\", "\\", TRUE);
  bufferReplace(buf, "\\n",  "\n", TRUE);
  bufferReplace(buf, "\\r",  "\r", TRUE);
  bufferReplace(buf, "\\\"", "\"", TRUE);
}

void bufferFormat(BUFFER *buf, int max_width, int indent) {
  char formatted[(buf->len * 3)/2];
  bool needs_capital = TRUE, needs_indent = FALSE;
  int fmt_i = 0, buf_i = 0, col = 0, next_space = 0;

  // put in our indent
  if(indent > 0) {
    char fmt[20];
    sprintf(fmt, "%%%ds", indent);
    sprintf(formatted, fmt, " ");
    fmt_i += indent;
    col   += indent;

    // skip the leading spaces
    while(isspace(buf->data[buf_i]) && buf->data[buf_i] != '\0')
      buf_i++;
  }

  for(; buf->data[buf_i] != '\0'; buf_i++) {
    // we have to put a newline in because the word won't fit on the line
    next_space = next_space_in(buf->data + buf_i);
    if(next_space == -1)
      next_space = buf->len - buf_i;
    if(col + next_space > max_width-1) {
      formatted[fmt_i] = '\r'; fmt_i++;
      formatted[fmt_i] = '\n'; fmt_i++;
      col = 0;
    }

    char ch = buf->data[buf_i];
    int para_start = -1;

    // try to preserve our paragraph structure
    if((para_start = is_paragraph_marker(buf->data, buf_i)) > buf_i) {
      formatted[fmt_i] = '\r'; fmt_i++;
      formatted[fmt_i] = '\n'; fmt_i++;
      formatted[fmt_i] = '\r'; fmt_i++;
      formatted[fmt_i] = '\n'; fmt_i++;
      buf_i = para_start - 1;
      col = 0;
      continue;
    }
    // no spaces on newlines or ends of lines
    else if(isspace(ch) && (col == 0 || col == max_width-1))
      continue;
    // we will do our own sentance formatting
    else if(needs_capital && isspace(ch))
      continue;
    // delete multiple spaces
    else if(isspace(ch) && fmt_i > 0 && isspace(formatted[fmt_i-1]))
      continue;
    // treat newlines as spaces (to separate words on different lines), 
    // since we are creating our own newlines
    else if(ch == '\r' || ch == '\n') {
      // we've already spaced
      if(col == 0)
	continue;
      formatted[fmt_i] = ' ';
      col++;
    }
    // if someone is putting more than 1 sentence delimiter, we
    // need to catch it so we will still capitalize the next word
    else if(strchr("?!.", ch) && isspace(buf->data[buf_i + 1])) {
      needs_capital = TRUE;
      needs_indent  = TRUE;
      formatted[fmt_i] = ch;
      col++;
    }
    // see if we are the first letter after the end of a sentence
    else if(needs_capital) {
      // check if indenting will make it so we don't
      // have enough room to print the word. If that's the
      // case, then skip down to a new line instead
      next_space = next_space_in(buf->data + buf_i);
      if(next_space == -1)
	next_space = buf->len - buf_i;
      if(col + 2 + next_space > max_width-1) {
	formatted[fmt_i] = '\r'; fmt_i++;
	formatted[fmt_i] = '\n'; fmt_i++;
	col = 0;
      }
      // indent two spaces if we're not at the start of a line 
      else if(needs_indent && col != 0) {
	formatted[fmt_i] = ' '; fmt_i++;
	formatted[fmt_i] = ' '; fmt_i++;
	col += 2;
      }
      // capitalize the first letter on the new word
      formatted[fmt_i] = toupper(ch);
      needs_capital = FALSE;
      needs_indent  = FALSE;
      col++;
    }
    else {
      formatted[fmt_i] = ch;
     
      // if we're adding a { or our last character was an {, don't increase
      // our columns, because these are colour codes
      if(ch != '{' && (fmt_i == 0 || formatted[fmt_i-1] != '{'))
	col++;
    }

    fmt_i++;
  }

  // tag a newline onto the end of the string if there isn't one already
  if(fmt_i > 0 && formatted[fmt_i-1] != '\n') {
    formatted[fmt_i++] = '\r';
    formatted[fmt_i++] = '\n';
  }

  formatted[fmt_i] = '\0';

  // if all we have are spaces and newlines, erase it all
  if(fmt_i == 2 + indent) {
    formatted[0] = '\0';
    fmt_i = 0;
  }

  // make sure we have enough room to copy everything over
  if(fmt_i >= buf->maxlen)
    bufferExpand(buf, (fmt_i * 5)/4 + 20);
  
  // copy over our changes
  strcpy(buf->data, formatted);
  buf->len = fmt_i;
}
