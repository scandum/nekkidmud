#ifndef FILEBUF_H
#define FILEBUF_H
//*****************************************************************************
//
// filebuf.h
//
// buffered file io
//
//*****************************************************************************

typedef struct buffered_file FILEBUF;

//
// create a new buffered file reader for reading, writing, 
// or appending, specified by the mode
FILEBUF *fbopen(const char *fname, const char *mode);

//
// close, flush, and delete the buffered file reader
void fbclose(FILEBUF *buf);

//
// flush the buffered file reader
void fbflush(FILEBUF *buf);

//
// return the next char in the buffered file
char fbgetc(FILEBUF *buf);

//
// print the formatting to the file
int fbprintf(FILEBUF *buf, const char *fmt, ...) 
__attribute__ ((format (printf, 2, 3)));

//
// append the text to the buffer
void fbwrite(FILEBUF *fb, const char *str);

//
// go to the position, from the specified offset. Uses SEEK_CUR, SEEK_SET,
// and SEEK_END from stdio.h
void fbseek(FILEBUF *buf, int offset, int origin);


#endif // FILEBUF
