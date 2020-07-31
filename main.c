#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

/* local objects */
#include "loadconfig.h"
#include "config.h"
#include "acquire.h"
#include "error.h"
#include "aldl-io.h"
#include "useful.h"
#include "serio.h"
#include "modules.h"

/************ SCOPE *********************************
  Initialize everything, and spawn all threads.
****************************************************/

/* ----- typedefs ------------*/

typedef struct _aldl_threads_t {
  pthread_t acq;
  pthread_t consoleif;
  pthread_t datalogger;
  pthread_t remote;
  pthread_t mode4;
} aldl_threads_t;

/* ------ local functions ------------- */

/* run some post-config loading sanity checks */
void aldl_sanity_check(aldl_conf_t *aldl);

/* run cleanup rountines for aldl and serial crap */
int aldl_finish();

/* spawn all modules */
void modules_start(aldl_threads_t *threads, aldl_conf_t *aldl);

/* check over modules for sanity */
void modules_verify(aldl_conf_t *aldl);

/* do things with cmdline options */
void parse_cmdline(int argc, char **argv, aldl_conf_t *aldl);

/* start acq thread */
void acq_start(aldl_threads_t *thread, aldl_conf_t *aldl);

/*---------- functions --------------------*/

int main(int argc, char **argv) {
  /* ------- initialize some shit ------------ */
  init_locks(); /* initialize locking mechanisms */
  aldl_conf_t *aldl = aldl_setup(); /* alloc everything and parse conf */
  aldl_sanity_check(aldl); /* sanity check the data from above */
  alloc_commbuf(); /* allocate communications static buffer */
  parse_cmdline(argc,argv,aldl); /* parse cmd line opts */
  modules_verify(aldl); /* check for bad module combos */
  aldl_data_init(aldl); /* init aldl data structs */
  set_connstate(ALDL_LOADING,aldl); /* init connection state */
  serial_init(aldl->serialstr); /* init i/o driver */

  /* ------- start threads ----------- */
  aldl_threads_t *thread = smalloc(sizeof(aldl_threads_t)); /* thread spc */
  acq_start(thread,aldl); /* start acquisition thread */
  modules_start(thread,aldl); /* start all other modules */
  pthread_join(thread->acq,NULL); /* pause main thread until acq dies */

  /* ----- cleanup ------------- */
  aldl_finish();
  return 0;
}

void parse_cmdline(int argc, char **argv, aldl_conf_t *aldl) {
  int n_arg = 0;
  for(n_arg=1;n_arg<argc;n_arg++) {
    if(rf_strcmp(argv[n_arg],"configtest") == 1) {
      printf("Loaded config OK.  Exiting...\n");
      exit(0);
    } else if(rf_strcmp(argv[n_arg],"devices") == 1) {
      serial_help_devs();
      exit(0);
    } else if(rf_strcmp(argv[n_arg],"mode4") == 1) {
      aldl->mode4_enable = 1;
    } else if(rf_strcmp(argv[n_arg],"consoleif") == 1) {
      aldl->consoleif_enable = 1;
    } else if(rf_strcmp(argv[n_arg],"datalogger") == 1) {
      aldl->datalogger_enable = 1;
    } else if(rf_strcmp(argv[n_arg],"remote") == 1) {
      aldl->remote_enable = 1;
    } else {
      error(1,ERROR_NULL,"Option %s not recognized",argv[n_arg]);
    }
  }
}

void modules_verify(aldl_conf_t *aldl) {
  /* compatibility checking */
  /* dont specify remote here, as remote by itself isn't enough ... */
  if(aldl->consoleif_enable == 0 &&
     aldl->datalogger_enable == 0) {
    error(1,ERROR_PLUGIN,"no plugins are enabled");
  }
}

void modules_start(aldl_threads_t *thread, aldl_conf_t *aldl) {
  if(aldl->mode4_enable == 1) {
    pthread_create(&thread->mode4,NULL,
              mode4_init,(void *) aldl);
    if(aldl->datalogger_enable == 1) { /* allow datalogger ... */
             pthread_create(&thread->datalogger,NULL,
                     datalogger_init,(void *) aldl);
    }
  } else {
    if(aldl->consoleif_enable == 1) {
      pthread_create(&thread->consoleif,NULL,
                     consoleif_init,(void *) aldl);
    }

    if(aldl->datalogger_enable == 1) {
      pthread_create(&thread->datalogger,NULL,
                     datalogger_init,(void *) aldl);
    }

    if(aldl->remote_enable == 1) {
      pthread_create(&thread->remote,NULL,
                      remote_init,(void *) aldl);
    }
  }
}

void acq_start(aldl_threads_t *thread, aldl_conf_t *aldl) {
  #ifdef ACQ_PRIORITY
  struct sched_param acq_param;
  pthread_attr_t acq_attr;
  pthread_attr_init(&acq_attr);
  pthread_attr_getschedparam(&acq_attr,&acq_param);
  acq_param.sched_priority = ACQ_PRIORITY;
  pthread_attr_setschedparam(&acq_attr,&acq_param);
  pthread_create(&thread->acq,&acq_attr,aldl_acq,(void *)aldl);
  #else
  pthread_create(&thread->acq,NULL,aldl_acq,(void *)aldl);
  #endif
}

void main_exit() {
  consoleif_exit();
  serial_close();
  aldl_finish();
}

int aldl_finish() {
  exit(1);
  return 0;
}

void aldl_sanity_check(aldl_conf_t *aldl) {
  /* if we require more records to start than available ... */
  if(aldl->bufstart > aldl->bufsize) {
    error(1,ERROR_RANGE,"Buffer size is smaller than buffer start");
  }

  /* check for packet out of range */
  int x,y;
  aldl_packetdef_t *p;
  for(x=0; x<aldl->n_defs; x++) {
    y = aldl->def[x].packet;
    if(y > aldl->comm->n_packets || y < 0) {
      error(1,ERROR_CONFIG,"Definition for %s belongs to bad packet %i",
            aldl->def[x].name,aldl->def[x].packet); 
    }
    p = &aldl->comm->packet[y];
    if(aldl->def[x].offset > p->length - p->offset) {
      error(1,ERROR_CONFIG,"Definition for %s has offset out of range",
            aldl->def[x].name);
    }
  }
}
