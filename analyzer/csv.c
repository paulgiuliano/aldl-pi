#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "csv.h"

#define DELIM ','

char *csv_get_string(char *start) {
  int length = field_end(start);
  if(length == 0) return NULL;
  char tmp = start[length];
  start[length] = 0;
  char *out = malloc(length + 1);
  strcpy(out,start);
  start[length] = tmp;
  return out;
}

int csv_get_int(char *start) {
  int length = field_end(start);
  if(length == 0) return 0;
  char tmp = start[length]; 
  start[length] = 0;
  int out = atoi(start);
  start[length] = tmp;
  return out;
}

float csv_get_float(char *start) {
  int length = field_end(start);
  if(length == 0) return 0;
  char tmp = start[length];
  start[length] = 0;
  float out = atof(start);
  start[length] = tmp;
  return out;
}

int field_end(char *in) {
  if(in == NULL) return 0;
  char *cursor = in;
  while(cursor[0] != DELIM && cursor[0] != '\n' && cursor[0] != 0) cursor++;
  return (int)(cursor - in);
}

char *field_start(char *in, int f) {
  char *cursor = in;
  int x;
  for(x=1;x<=f;x++) { /* step through req. number of fields */
    while(cursor[0] != DELIM) {
      if(cursor[0] == '\n' || cursor[0] == 0) {
        return NULL;
      }
      cursor++;
    }
    cursor++;
  }
  return cursor;
}

char *line_start(char *in, int ln) {
  char *cursor = in;
  int x;
  for(x=1;x<=ln;x++) {
    while(cursor[0] != '\n') {
      if(cursor[0] == 0) {
        return NULL;
      }
      cursor++;
    }
    cursor++;
  }
  return cursor;
}

