//*****************************************************************************
//
// filebuf.h
//
// buffered file io
//
//*****************************************************************************

#include <stdio.h>
#include <time.h>
#include "mud.h"



//*****************************************************************************
// local structures and defines
//*****************************************************************************

#define FBMODE_READ              0
#define FBMODE_WRITE             1

struct buffered_file {
  FILE    *fl;
  BUFFER *buf;
  int     pos;
  int    mode;
};

//
// buffer up input from our file
void fb_buffer(FILEBUF *fb) {
  if(feof(fb->fl))
    return;

  char in[MAX_BUFFER+1];
  int amnt;
  do {
    amnt = fread(in, sizeof(char), MAX_BUFFER, fb->fl);
    in[amnt] = '\0';
    bufferCat(fb->buf, in);
  } while(amnt == MAX_BUFFER);
}

FILEBUF *fbopen(const char *fname, const char *mode) {
  // make sure we can access the file...
  FILE *fl = fopen(fname, mode);
  if(fl == NULL)
    return NULL;

  // clock our performance
  // struct timeval start_time;
  // gettimeofday(&start_time, NULL);

  FILEBUF *fb = malloc(sizeof(FILEBUF));
  fb->fl      = fl;
  fb->buf     = newBuffer(1024);
  fb->pos     = 0;

  if(*mode == 'r') {
    fb->mode = FBMODE_READ;
    fb_buffer(fb);
  }
  else
    fb->mode = FBMODE_WRITE;

  // finish clocking performance
  // struct timeval end_time;
  // gettimeofday(&end_time, NULL);
  // int usecs = (int)(end_time.tv_usec - start_time.tv_usec);
  // int secs  = (int)(end_time.tv_sec  - start_time.tv_sec);
  // log_string("buffer open time %d %s", (int)(secs*1000000 + usecs), fname);

  return fb;
}

//
// close, flush, and delete the buffered file reader
void fbclose(FILEBUF *fb) {
  fbflush(fb);
  fclose(fb->fl);
  deleteBuffer(fb->buf);
  free(fb);
}

//
// flush the buffered file reader
void fbflush(FILEBUF *fb) {
  if(fb->mode == FBMODE_WRITE && bufferLength(fb->buf) > fb->pos) {
    const char *to_flush = bufferString(fb->buf);
    fprintf(fb->fl, "%s", to_flush+fb->pos);
    fb->pos = bufferLength(fb->buf);
  }
}

//
// return the next char in the buffered file
char fbgetc(FILEBUF *fb) {
  if(fb->pos >= bufferLength(fb->buf))
    return EOF;
  else
    return bufferString(fb->buf)[fb->pos++];
}

//
// print the formatting to the file
int fbprintf(FILEBUF *fb, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  int res = vbprintf(fb->buf, fmt, va);
  va_end(va);
  return res;
}

//
// append the text to the buffer
void fbwrite(FILEBUF *fb, const char *str) {
  bufferCat(fb->buf, str);
}

//
// go to the position, from the specified offset. Uses SEEK_CUR, SEEK_SET,
// and SEEK_END from stdio.h
void fbseek(FILEBUF *fb, int offset, int origin) {
  if(origin == SEEK_SET)
    fb->pos = 0;
  else if(origin == SEEK_END)
    fb->pos = bufferLength(fb->buf) - 1;
  else if(origin != SEEK_CUR)
    return;
  fb->pos += offset;
}
