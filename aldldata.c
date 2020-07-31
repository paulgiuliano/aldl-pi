#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

#include "serio.h"
#include "config.h"

#include "error.h"
#include "config.h"
#include "aldl-io.h"
#include "useful.h"

/************ SCOPE *********************************
  This object contains all of the functions used for
  structuring and parsing the ALDL data structures,
  including locking and serializing of record retr.
****************************************************/

/* -------- globalstuffs ------------------ */

/* locking */
typedef enum aldl_lock {
  LOCK_CONNSTATE = 0,
  LOCK_RECORDPTR = 1,
  LOCK_STATS = 2,
  LOCK_COMQ = 3,
  N_LOCKS = 4
} aldl_lock_t;
pthread_mutex_t *aldllock;

timespec_t firstrecordtime; /* timestamp used to calc. relative time */

/* primary memory pool for record storage */
aldl_record_t *recordbuffer; /* circular pool for records */
aldl_data_t *databuffer; /* circular pool for data */
unsigned int indexbuffer; /* index for both of above */

/* linked list forming a FIFO queue of commands */
aldl_comq_t *comq;

/* --------- local function decl. ---------------- */

/* update the value in the record from definition n */
aldl_data_t *aldl_parse_def(aldl_conf_t *aldl, aldl_record_t *r, int n);

/* allocate record and timestamp it */
aldl_record_t *aldl_create_record(aldl_conf_t *aldl);

/* link a prepared record to the linked list */
void link_record(aldl_record_t *rec, aldl_conf_t *aldl);

/* fill a prepared record with data */
aldl_record_t *aldl_fill_record(aldl_conf_t *aldl, aldl_record_t *rec);

/* set and unset locks, wrapper with error checking for pthread funcs */
inline void set_lock(aldl_lock_t lock_number);
inline void unset_lock(aldl_lock_t lock_number);

/* allocate memory pool */
void aldl_alloc_pool(aldl_conf_t *aldl);

/* --------------------------------------------------------- */

void init_locks() {
  int x;
  int pthreaderr;
  aldllock = smalloc(sizeof(pthread_mutex_t) * N_LOCKS); 
  for(x=0;x<N_LOCKS;x++) {
    pthreaderr = pthread_mutex_init(&aldllock[x],NULL); 
    if(pthreaderr != 0) error(1,ERROR_LOCK,
         "error initializing lock %i, pthread error %i",x,pthreaderr);
  }
}

inline void set_lock(aldl_lock_t lock_number) {
  int rtval;
  rtval = pthread_mutex_lock(&aldllock[lock_number]);
  if(rtval != 0) error(1,ERROR_LOCK,
          "error setting lock %i, pthread error code %i",lock_number,rtval);
}

inline void unset_lock(aldl_lock_t lock_number) {
  int rtval;
  rtval = pthread_mutex_unlock(&aldllock[lock_number]);
  if(rtval != 0) error(1,ERROR_LOCK,
          "error unsetting lock %i, pthread error code %i",lock_number,rtval);
}

void lock_stats() {
  set_lock(LOCK_STATS);
}

void unlock_stats() {
  unset_lock(LOCK_STATS);
}

aldl_record_t *process_data(aldl_conf_t *aldl) {
  aldl_record_t *rec = aldl_create_record(aldl);
  aldl_fill_record(aldl,rec);
  link_record(rec,aldl);
  return rec;
}

void link_record(aldl_record_t *rec, aldl_conf_t *aldl) {
  rec->next = NULL; /* terminate linked list */
  rec->prev = aldl->r; /* previous link */
  set_lock(LOCK_RECORDPTR);
  aldl->r->next = rec; /* attach to linked list */
  aldl->r = rec; /* fix master link */
  unset_lock(LOCK_RECORDPTR);
}

void aldl_data_init(aldl_conf_t *aldl) {
  aldl_alloc_pool(aldl);
  aldl_record_t *rec = aldl_create_record(aldl);
  set_lock(LOCK_RECORDPTR);
  rec->next = NULL;
  rec->prev = NULL;
  aldl->r = rec;
  unset_lock(LOCK_RECORDPTR);
  firstrecordtime = get_time();
  comq = NULL; /* no records yet */
}

aldl_record_t *aldl_create_record(aldl_conf_t *aldl) {
  /* get memory pool addresses */
  aldl_record_t *rec = &recordbuffer[indexbuffer];
  rec->data = &databuffer[indexbuffer * aldl->n_defs];

  /* advance pool index (for next time around) */
  if(indexbuffer > aldl->bufsize - 2) { /* end of buffer */
    indexbuffer = 0; /* return to beginning */
  } else {
    indexbuffer++;
  }

  /* timestamp record */
  rec->t = get_elapsed_ms(firstrecordtime);

  #ifdef TIMESTAMP_WRAPAROUND
  /* handle wraparound if we're 100 seconds before time limit */
  if(rec->t > ULONG_MAX - 100000) firstrecordtime = get_time();
  #endif

  return rec;
}

aldl_record_t *aldl_fill_record(aldl_conf_t *aldl, aldl_record_t *rec) {
  /* process packet data */
  int def_n;
  for(def_n=0;def_n<aldl->n_defs;def_n++) {
    aldl_parse_def(aldl,rec,def_n);
  }
  return rec;
}

aldl_data_t *aldl_parse_def(aldl_conf_t *aldl, aldl_record_t *r, int n) {
  /* check for out of range */
  if(n < 0 || n > aldl->n_defs - 1) error(1,ERROR_RANGE,
                                    "def number %i is out of range",n); 

  aldl_define_t *def = &aldl->def[n]; /* shortcut to definition */

  int id = def->packet; /* packet id (array index) */

  aldl_packetdef_t *pkt = &aldl->comm->packet[id]; /* ptr to packet */

  /* location of actual data byte */
  byte *data = pkt->data + def->offset + pkt->offset;

  /* location for output of data, matches definition array index ... */
  aldl_data_t *out = &r->data[n];

  /* location for input of data */
  unsigned int x;

  /* convert input bit size into unsigned integer */
  switch(def->size) {
    case 16:
      /* turn two 8 bit bytes into a 16 bit int */
      x = (unsigned int)((*data<<8)|*(data+1));
      break;
    case 8: /* direct conversion */
    default: /* no other types supported, default to 8 bit */
      x = (unsigned int)*data;
  }

  /* apply any math or whatever and output as desired type */
  switch(def->type) {
    case ALDL_INT:
      out->i = ( (int)x * def->multiplier.i ) + def->adder.i;
      if(aldl->minmax == 1) {
        out->i = rf_clamp_int(def->min.i,def->max.i,out->i);
      }
      break;
    case ALDL_FLOAT:
      out->f = ( (float)x * def->multiplier.f ) + def->adder.f;
      if(aldl->minmax == 1) {
        out->f = rf_clamp_float(def->min.f,def->max.f,out->f); 
      }
      break;
    case ALDL_BOOL:
      if(aldl->comm->byteorder == 1) { /* deal with MSB or LSB */
        out->i = getbit(x,(7 - def->binary),def->invert);
      } else {
        out->i = getbit(x,def->binary,def->invert);
      }
      break;
    default:
      error(1,ERROR_RANGE,"invalid type spec: %i",def->type);
  }

  return out;
}

aldl_state_t get_connstate(aldl_conf_t *aldl) {
  set_lock(LOCK_CONNSTATE);
  aldl_state_t st = aldl->state;
  unset_lock(LOCK_CONNSTATE);
  return st;
}

void set_connstate(aldl_state_t s, aldl_conf_t *aldl) {
  set_lock(LOCK_CONNSTATE);
  #ifdef DEBUGSTRUCT
  printf("set connection state to %i (%s)\n",s,get_state_string(s));
  #endif
  aldl->state = s;
  unset_lock(LOCK_CONNSTATE);
}

aldl_record_t *newest_record(aldl_conf_t *aldl) {
  aldl_record_t *rec = NULL;
  set_lock(LOCK_RECORDPTR);
  rec = aldl->r; 
  unset_lock(LOCK_RECORDPTR);
  return rec;
}

aldl_record_t *newest_record_wait(aldl_conf_t *aldl, aldl_record_t *rec) {
  aldl_record_t *next = NULL;
  while(1) {
    next = newest_record(aldl);
    if(next != rec) {
      return next;
    } else if(get_connstate(aldl) > 10) {
      return NULL;
    } else {
      #ifndef AGGRESSIVE
      usleep(500);
      #endif
    }
  } 
}

aldl_record_t *next_record_wait(aldl_conf_t *aldl, aldl_record_t *rec) {
  aldl_record_t *next = NULL;
  while(next == NULL) {
    next = next_record(rec);
    if(get_connstate(aldl) > 10) return NULL;
    #ifndef AGGRESSIVE
    usleep(500); /* throttling ... */
    #endif
  }
  return next;
}

aldl_record_t *next_record_waitf(aldl_conf_t *aldl, aldl_record_t *rec) {
  aldl_record_t *next = NULL;
  while((next = next_record_wait(aldl,rec)) == NULL) usleep(500);
  return next;
}

aldl_record_t *newest_record_waitf(aldl_conf_t *aldl, aldl_record_t *rec) {
  aldl_record_t *next = NULL;
  while((next = newest_record_wait(aldl,rec)) == NULL) usleep(500);
  return next;
}

aldl_record_t *next_record(aldl_record_t *rec) {
  #ifdef DEBUGSTRUCT
  /* check for underrun ... */
  if(rec->prev == NULL) {
     error(1,ERROR_BUFFER,"underrun in record retrieve %p",rec);
  }
  #endif
  aldl_record_t *next;
  set_lock(LOCK_RECORDPTR);
  next = rec->next;
  unset_lock(LOCK_RECORDPTR);
  return next;
}

void pause_until_connected(aldl_conf_t *aldl) {
  while(get_connstate(aldl) > 10) {
    #ifdef AGGRESSIVE
      usleep(100);
    #else
      msleep(100);
    #endif
  }
}

void pause_until_buffered(aldl_conf_t *aldl) {
  while(aldl->ready ==0) {
    #ifdef AGGRESSIVE
      usleep(100);
    #else
      msleep(100); 
    #endif
  }
}

int get_index_by_name(aldl_conf_t *aldl, char *name) {
  int x;
  for(x=0;x<aldl->n_defs;x++) {
    if(rf_strcmp(name,aldl->def[x].name) == 1) return x;
  }
  return -1; /* not found */
}

char *get_state_string(aldl_state_t s) {
  switch(s) {
    case ALDL_CONNECTED:
      return "Connected";
    case ALDL_CONNECTING:
      return "Connecting";
    case ALDL_LOADING:
      return "Loading";
    case ALDL_DESYNC:
      return "Lost Sync";
    case ALDL_ERROR:
      return "Error";
    case ALDL_LAGGY:
      return "Laggy";
    case ALDL_QUIT:
      return "Quit";
    case ALDL_PAUSE:
      return "Paused";
    case ALDL_SERIALERROR:
      return "Serial ERR";
    default:
      return "Undefined";
  }
}

void aldl_alloc_pool(aldl_conf_t *aldl) {
  /* get sizes */
  size_t databuffer_size = sizeof(aldl_data_t) * aldl->n_defs * aldl->bufsize;
  size_t recordbuffer_size = sizeof(aldl_record_t) * aldl->bufsize;

  /* alloc */
  databuffer = smalloc(databuffer_size);
  recordbuffer = smalloc(recordbuffer_size);
  indexbuffer = 0; /* start at ptr 0 */

  /* optional print sizes */
  #ifdef DEBUGMEM
  printf("aldldata.c Circular Buffer: BUF=%u Recs, DATA=%uKb REC=%uKb\n",
          aldl->bufsize, (unsigned int)databuffer_size/1024,
         (unsigned int)recordbuffer_size/1024);
  #endif
}

void aldl_add_command(byte *command, byte length, int delay) {
  if(command == NULL) return;

  /* build new command */
  aldl_comq_t *n; /* new command */
  n = smalloc(sizeof(aldl_comq_t));
  n->length = length;
  n->delay = delay;
  n->command = smalloc(sizeof(char) * length);
  int x; for(x=0;x<length;x++) { /* copy command */
    n->command[x] = command[x];
  }
  n->next = NULL;

  /* link in new command */
  set_lock(LOCK_COMQ);
  if(comq == NULL) { /* no other commands exist */
    comq = n;
  } else {
    aldl_comq_t *e = comq; /* end of linked list */
    while(e->next != NULL) e = e->next; /* seek end */
    e->next = n; 
  } 
  unset_lock(LOCK_COMQ);
}

aldl_comq_t *aldl_get_command() {
  set_lock(LOCK_COMQ);
  if(comq == NULL) {
    unset_lock(LOCK_COMQ);
    return NULL; /* no command available */
  }
  aldl_comq_t *c = comq;
  comq = c->next; /* advance to next command */
  unset_lock(LOCK_COMQ);
  return c;
  /* WARNING you need to free this after you're done with it ... */
}
