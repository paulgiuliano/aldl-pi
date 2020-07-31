#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdarg.h>

/* local objects */
#include "config.h"
#include "error.h"
#include "loadconfig.h"
#include "useful.h"

/* This is a stripped down and portable version of ../loadconfig.c,
   it may be a bit further behind, though. */

/* -- LOCAL FUNCTIONS -- */

/* is a char whitespace ...? */
inline int is_whitespace(char ch);

char *dconfig(char *buf, char *parameter, int n);

char *configopt_fatal(dfile_t *config, char *str) {
  char *val = configopt(config,str,NULL);
  if(val == NULL) error("Missing config option: %s",str);
  return val;
}

char *configopt(dfile_t *config,char *str,char *def) {
  char *val = value_by_parameter(str, config);
  if(val == NULL) return def;
  return val;
}

float configopt_float(dfile_t *config, char *str, float def) {
  char *in = configopt(config,str,NULL);
  #ifdef DEBUGCONFIG
  if(in == NULL) {
    printf("caught default value for %s: %f\n",str,def);
    return def;
  }
  #else
  if(in == NULL) return def;
  #endif
  float x = atof(in);
  return x;
}

float configopt_float_fatal(dfile_t *config,char *str) {
  float x = atof(configopt_fatal(config,str));
  return x;
}

int configopt_int(dfile_t *config,char *str, int min, int max, int def) {
  char *in = configopt(config,str,NULL);
  #ifdef DEBUGCONFIG
  if(in == NULL) {
    printf("caught default value for %s: %i\n",str,def);
    return def;
  }
  #else
  if(in == NULL) return def;
  #endif
  int x = atoi(in);
  if(x < min || x > max) error("Config error: %s must be between %i and %i",
                       str,min,max);
  return x;
}

int configopt_int_fatal(dfile_t *config,char *str, int min, int max) {
  int x = atoi(configopt_fatal(config,str));
  if(x < min || x > max) error("Config error: %s must be between %i and %i",
                       str,min,max);
  return x;
}

char *dconfig(char *buf, char *parameter, int n) {
  sprintf(buf,"D%i.%s",n,parameter);
  return buf;
}

dfile_t *dfile_load(char *filename) {
  char *data = rf_loadfile(filename);
  if(data == NULL) return NULL;
  dfile_t *d = dfile(data);
  dfile_strip_quotes(d);
  dfile_shrink(d);
  free(data);
  return d; 
}

void dfile_strip_quotes(dfile_t *d) {
  int x;
  char *c; /* cursor*/
  for(x=0;x<d->n;x++) {
    if(d->v[x][0] == '"') {
      d->v[x]++;
      c = d->v[x];
      while(*c != '"') c++;
      c[0] = 0;
    }
  }
}

dfile_t *dfile(char *data) {
  /* allocate base structure */
  dfile_t *out = malloc(sizeof(dfile_t));

  /* initial allocation of pointer array */
  out->p = malloc(sizeof(char*) * MAX_PARAMETERS);
  out->v = malloc(sizeof(char*) * MAX_PARAMETERS);
  out->n = 0;

  /* more useful variables */
  char *c; /* operating cursor within data */
  char *cx; /* auxiliary operating cursor */
  char *cz;
  int len = strlen(data);

  for(c=data;c<data+len;c++) { /* iterate through entire data set */
    if(c[0] == '=') {
      if(c == data || c == data + len) continue; /* handle first or last char */
      out->v[out->n] = c + 1;
      c[0] = 0;
      cx = c + 1;
      while(is_whitespace(*cx) != 1) {
        if(*cx == '"') { /* skip quoted string */
          cx++;
          while(cx[0] != '"') {
            if(cx[1] == 0) error("Unterminated quote in config");
            if(cx == data + len) continue;
            cx++;
          }
        }
        cx++;
      }
      cx[0] = 0; /* null end */
      cz = cx; /* rememeber end point */
      /* find starting point */
      cx = c - 1;
      while(is_whitespace(*cx) != 1) {
        if(*cx == '"') { /* skip quoted string */
          cx--;
          if(cx < data) error("Unterminated quote in config");
          while(cx[0] != '"') {
            if(cx == data) error("Unterminated quote in config");
            if(cx == data + len) continue;
            cx--;
          }
        }
        cx--;
        if(cx == data) { /* handle case of beginning of file */
          cx--;
          break;
        }
      }
      out->p[out->n] = cx + 1;
      out->n++;
      if(out->n == MAX_PARAMETERS) return out; /* out of space */
      c = cz;
    }
  }
  return out;
}

/* reduce memory footprint */
char *dfile_shrink(dfile_t *d) {
  /* shrink arrays */
  d->p = realloc(d->p,sizeof(char*) * d->n);
  d->v = realloc(d->v,sizeof(char*) * d->n);
  /* calculate total size of new data storage */
  size_t ttl = 0;
  int x;
  for(x=0;x<d->n;x++) {
    ttl += strlen(d->p[x]);
    ttl += strlen(d->v[x]);
    ttl += 2; /* for null terminators */
  }
  ttl++;
  /* move everything and update pointers */
  char *newdata = malloc(ttl); /* new storage */
  char *c = newdata; /* cursor*/
  for(x=0;x<d->n;x++) {
    /* copy parameter */
    strcpy(c,d->p[x]);
    d->p[x] = c;
    c += (strlen(c) + 1);
    /* copy value */
    strcpy(c,d->v[x]);
    d->v[x] = c;
    c += (strlen(c) + 1);
  }
  return newdata;
}


inline int is_whitespace(char ch) {
  if(ch == 0 || ch == ' ' || ch == '\n') return 1;
  return 0;
}

char *value_by_parameter(char *str, dfile_t *d) {
  int x;
  for(x=0;x<d->n;x++) {
    if(rf_strcmp(str,d->p[x]) == 1) return d->v[x];
  }
  return NULL;
}

void print_config(dfile_t *d) {
  printf("config list has %i parsed options:-------------\n",d->n);
  int x;
  for(x=0;x<d->n;x++) printf("p(arameter):%s v(alue):%s\n",d->p[x],d->v[x]);
  printf("----------end config\n");
}

