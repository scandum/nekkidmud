/*
 * This file handles input/output to files (including log)
 */
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>  
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>

/* include main header file */
#include "mud.h"
#include "utils.h"

//extern FILE *stderr;
time_t current_time;

/*
 * Nifty little extendable logfunction,
 * if it wasn't for Erwins social editor,
 * I would never have known about the
 * va_ functions.
 */
void log_string(const char *txt, ...)
{
  FILE *fp;
  char logfile[MAX_BUFFER];
  char buf[MAX_BUFFER];
  char *strtime = get_time();
  va_list args;

  va_start(args, txt);
  vsnprintf(buf, MAX_BUFFER, txt, args);
  va_end(args);

  /* point to the correct logfile */
  snprintf(logfile, MAX_BUFFER, "../log/%6.6s.log", strtime);

  /* try to open logfile */
  if ((fp = fopen(logfile, "a")) == NULL)
  {
    communicate(NULL, "log: cannot open logfile", COMM_LOG);
    return;
  }

  fprintf(fp, "%s: %s\n", strtime, buf);
  fclose(fp);

  communicate(NULL, buf, COMM_LOG);
}

/*
 * Nifty little extendable bugfunction,
 * if it wasn't for Erwins social editor,
 * I would never have known about the
 * va_ functions.
 */
void bug(const char *txt, ...)
{
  FILE *fp;
  char buf[MAX_BUFFER];
  va_list args;
  char *strtime = get_time();

  va_start(args, txt);
  vsnprintf(buf, MAX_BUFFER, txt, args);
  va_end(args);

  /* try to open logfile */
  if ((fp = fopen("../log/bugs.txt", "a")) == NULL)
  {
    communicate(NULL, "bug: cannot open bugfile", COMM_LOG);
    return;
  }

  fprintf(fp, "%s: %s\n", strtime, buf);
  fclose(fp);

  communicate(NULL, buf, COMM_LOG);
}


BUFFER *read_file(const char *file) {
  FILE *fl;

  if((fl = fopen(file, "r")) == NULL)
    return NULL;
  else {
    BUFFER *buf = newBuffer(MAX_BUFFER);
    char     ch = '\0';

    while((ch = getc(fl)) != EOF)
      bprintf(buf, "%c", ch);

    fclose(fl);
    return buf;
  }
}
