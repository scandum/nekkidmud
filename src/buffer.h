#ifndef BUFFER_H
#define BUFFER_H
//*****************************************************************************
//
// buffer.h
//
// The buffer datastructure is a string-like thing of infinite length. Contains
// some basic utilities for interacting with it.
//
//*****************************************************************************

typedef struct buffer_data BUFFER;

// allocate/deallocate a buffer. start capacity must be supplied, but the
// buffer can grow indefinitely.
BUFFER *newBuffer(int start_capacity);
void deleteBuffer(BUFFER *buf);

// concatinate the text to the end of the buffer
void        bufferCat   (BUFFER *buf, const char *txt);
void        bufferCatCh (BUFFER *buf, const char ch);

// clear the buffer's contents
void bufferClear(BUFFER *buf);

// copy the contents of one buffer to another
void bufferCopyTo(BUFFER *from, BUFFER *to);

// return a copy of the buffer
BUFFER *bufferCopy(BUFFER *buf);

// return the string contents of the buffer
const char *bufferString(BUFFER *buf);

// return the length of the buffer's contents
int bufferLength(BUFFER *buf);

// do a formatted print onto the buffer (concats it)
int bprintf(BUFFER *buf, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

// bprintf with a variable list and format supplied
int vbprintf(BUFFER *buf, const char *fmt, va_list va);

// replace 'a' with 'b'. Return back how many occurences were replaced. If
// all is FALSE, then only the first occurence is replaced.
int bufferReplace(BUFFER *buf, const char *a, const char *b, int all);

// insert the line into the string at the specified place. If the line was
// inserted, return TRUE. Otherwise, return FALSE.
int bufferInsert(BUFFER *buf, const char *newline, int line);

// delete the specified line. Return TRUE if successful and FALSE otherwise
int bufferRemove(BUFFER *buf, int line);

// replace the given line number with the new information. Return TRUE if
// successful and FALSE otherwise.
int bufferReplaceLine(BUFFER *buf, const char *newline, int line);

// format a string with the given arguments
void bufferFormat(BUFFER *buf, int max_width, int indent);

// format our buffer to be a viable Python string
void bufferFormatPy(BUFFER *buf);

// format our buffer to be a C string from a Python string
void bufferFormatFromPy(BUFFER *buf);

#endif // BUFFER_H
