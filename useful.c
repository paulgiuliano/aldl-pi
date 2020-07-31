#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "aldl-types.h"
#include "useful.h"
#include "error.h"

/************ SCOPE *********************************
  Useful but generic functions.
****************************************************/

timespec_t get_time() {
  timespec_t currenttime;
  #ifdef USEFUL_BETTERCLOCK
  clock_gettime(CLOCK_MONOTONIC,&currenttime);
  #else
  gettimeofday(&currenttime,NULL);
  #endif
  return currenttime;
}

unsigned long get_elapsed_ms(timespec_t timestamp) {
  timespec_t currenttime = get_time();
  unsigned long seconds = currenttime.tv_sec - timestamp.tv_sec;
  #ifdef USEFUL_BETTERCLOCK
  unsigned long milliseconds =(currenttime.tv_nsec-timestamp.tv_nsec) / 1000000;
  #else
  unsigned long milliseconds =(currenttime.tv_usec-timestamp.tv_usec) / 1000;
  #endif
  return ( seconds * 1000 ) + milliseconds;
}

byte checksum_generate(byte *buf, int len) {
  #ifdef RETARDED
  retardptr(buf,"checksum buf");
  #endif
  int x = 0;
  unsigned int sum = 0;
  for(x=0;x<len;x++) sum += buf[x];
  return ( 256 - ( sum % 256 ) );
}

int checksum_test(byte *buf, int len) {
  int x = 0;
  unsigned int sum = 0;
  for(x=0;x<len;x++) sum += buf[x];
  if(( sum & 0xFF ) == 0) return 1;
  return 0;
}

int cmp_bytestring(byte *h, int hsize, byte *n, int nsize) {
  if(nsize > hsize) return 0; /* needle is larger than haystack */
  if(hsize < 1 || nsize < 1) return 0;
  int cursor = 0; /* haystack compare cursor */
  int matched = 0; /* needle compare cursor */
  while(cursor <= hsize) {
    if(nsize == matched) return 1;
    if(h[cursor] != n[matched]) { /* reset match */
      matched = 0;
    } else {
      matched++;
    }
    cursor++;
  }
  return 0;
}

void printhexstring(byte *str, int length) {
  int x;
  for(x=0;x<length;x++) printf("%X ",(unsigned int)str[x]);
  printf("\n");
}

#ifdef MALLOC_ERRCHECK
void *smalloc(size_t size) {
  void *m = malloc(size); 
  if(m == NULL) {
   fprintf(stderr, "Out of memory trying to alloc %u bytes",(unsigned int)size);
   exit(1);
  }
  return m;
}
#endif

/* General purpose c function library. */

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
