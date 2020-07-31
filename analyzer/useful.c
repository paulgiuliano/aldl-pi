#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "useful.h"
#include "error.h"

/************ SCOPE *********************************
  Useful but generic functions.
****************************************************/

int rf_strcmp(char *a, char *b) {
  int x = 0;
  while(a[x] == b[x]) {
    x++;
    if(a[x] == 0 || b[x] == 0) {
      if(a[x] == 0 && b[x] == 0) {
        return 1;
      } else {
        return 0;
      }
    }
  }
  return 0;
}

char rf_listcmp(char *str, char *filter) {
  int x = 0;
  int y = 0;
  while(filter[x] != 0) {
    y = 0;
    while(str[y] != 0) {
      if(str[y] == filter[x]) return filter[x];
      y++;
    }
    x++;
  }
  return 0;
}

int rf_clamp_int(int min, int max, int in) {
  if(in > max) return max;
  if(in < min) return min;
  return in;
}

float rf_clamp_float(float min, float max, float in) {
  if(in > max) return max;
  if(in < min) return min;
  return in;
}

int rf_chfilter(char *str, char *filter, char repl) {
  int x = 0;
  int y = 0;
  unsigned int c = 0;
  while(filter[x] != 0) {
    y = 0;
    while(str[y] != 0) {
      if(str[y] == filter[x]) {
        str[y] = repl;
      }
      y++;
    }
    x++;
  }
  return c;
}

char *rf_loadfile(char *filename) {
  FILE *fdesc;
  if(filename == NULL) return NULL;
  fdesc = fopen(filename, "r");
  if(fdesc == NULL) return NULL;
  fseek(fdesc, 0L, SEEK_END);
  int flength = ftell(fdesc);
  if(flength == -1) {
    fclose(fdesc);
    return NULL;
  }
  rewind(fdesc);
  char *buf = malloc(sizeof(char) * ( flength + 1));
  if(fread(buf,1,flength,fdesc) != flength) {
    free(buf);
    fclose(fdesc);
    return NULL;
  }
  fclose(fdesc);
  buf[flength] = 0;
  return buf;
}
