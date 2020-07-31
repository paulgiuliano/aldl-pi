#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* local objects */
#include "error.h"
#include "aldl-io.h"
#include "loadconfig.h"
#include "useful.h"

typedef struct _datalogger_conf {
  dfile_t *dconf; /* raw config data */
  int autostart;
  char *log_filename;
  int log_all;
  int sync;
  int rate;
  int skip;
  int marker;
  FILE *fdesc;
} datalogger_conf_t;

int logger_be_quiet(aldl_conf_t *aldl);

void datalogger_make_file(datalogger_conf_t *conf,aldl_conf_t *aldl);

datalogger_conf_t *datalogger_load_config(aldl_conf_t *aldl);

void *datalogger_init(void *aldl_in) {
  unsigned int n_records = 0; /* number of record counter */
  unsigned long last_timestamp = 0;
  int x = 0; /* tmp */
  float pps; /* packet per second rate */
  aldl_conf_t *aldl = (aldl_conf_t *)aldl_in;

  /* grab config data */
  datalogger_conf_t *conf = datalogger_load_config(aldl);

  /* calculate appropriate linebuffer size */
  size_t linebufsize = 0;
  for(x=0;x<aldl->n_defs;x++) {
    if(aldl->def[x].log == 1 || conf->log_all == 1) {
      if(aldl->def[x].type == ALDL_BOOL) {
        linebufsize += 3; /* 3 bytes for a bool */
      } else {
        linebufsize += 64; /* 64 bytes for anything else */
      }
    }
  }

  char *linebuf = smalloc(linebufsize);
  char *cursor = linebuf; /* ptr to working byte in line buffer */

  /* this label is to be used for pausing and starting a new logfile in case
     one is required ... */
  //JUMP_ROLL_LOG:

  /* wait for buffered connection.  we do this before creating the actual
     log file, this makes sense because if a connection never occurs,
     the file never gets made ... */
  pause_until_buffered(aldl);

  /* create logfile */
  datalogger_make_file(conf,aldl);

  cursor += sprintf(cursor,"TIMESTAMP(ms)"); /* string cursor */

  /* write csv header */
  for(x=0;x<aldl->n_defs;x++) {
    if(conf->log_all == 0) {
      if(aldl->def[x].log == 0) continue;
    }
    cursor += sprintf(cursor,",%s",aldl->def[x].name);
    if(aldl->def[x].uom != NULL) {
      cursor += sprintf(cursor,"(%s)",aldl->def[x].uom);
    }
  }
  cursor += sprintf(cursor,"\n");
  fwrite(linebuf,cursor - linebuf,1,conf->fdesc);

  aldl_record_t *rec = aldl->r;
  /* event loop */
  while(1) {
    if(conf->skip == 1) {
      rec = newest_record_wait(aldl,rec);
    } else {
      rec = next_record_wait(aldl,rec);
    }
    if(rec == NULL) {
      if(logger_be_quiet(aldl) == 0) {
        printf("datalogger: Connection state: %s.  Waiting for connection...\n",
                get_state_string(get_connstate(aldl)));
      }
      pause_until_connected(aldl);
      if(logger_be_quiet(aldl) == 0) {
        printf("datalogger: Reconnected.  Resuming logging...\n");
      } 
      continue;
    }
    if(last_timestamp + conf->rate >= rec->t) continue; /* skip record */
    cursor=linebuf; /* reset cursor */
    cursor += sprintf(cursor,"%lu",rec->t);
    for(x=0;x<aldl->n_defs;x++) {
      if(conf->log_all == 0) {
        if(aldl->def[x].log == 0) continue;
      }
      switch(aldl->def[x].type) {
        case ALDL_FLOAT:
          cursor += sprintf(cursor,",%.2f",rec->data[x].f);
          break;
        case ALDL_INT:
        case ALDL_BOOL:
          cursor += sprintf(cursor,",%i",rec->data[x].i);
          break;
        default:
          cursor += sprintf(cursor,",");
      }
    }
    cursor += sprintf(cursor,"\n");
    fwrite(linebuf,cursor - linebuf,1,conf->fdesc);
    if(conf->sync == 1) fflush(conf->fdesc);
    if(logger_be_quiet(aldl) == 0) {
      n_records++;
      if(n_records % 300 == 0) {
        lock_stats();
        pps = aldl->stats->packetspersecond;
        unlock_stats();
        printf("datalogger: Logged %u pkts @ %.2f/sec\n",n_records,pps);
      }
    }
    last_timestamp = rec->t; /* update timestamp */
  }

  fclose(conf->fdesc);
  free(conf);
  /* end ... */
  return NULL;
}

void datalogger_make_file(datalogger_conf_t *conf,aldl_conf_t *aldl) {
  /* alloc and fill filename buffer */
  int maxfnlength = strlen(conf->log_filename) * 2 + 50;
  struct tm *tm;
  time_t t;
  unsigned int suffix = 1;
  t = time(NULL);
  tm = localtime(&t);
  char *filename = smalloc(maxfnlength);
  strftime(filename,maxfnlength,conf->log_filename,tm);
  char *fnappend = filename;
  while(fnappend[0] != 0) fnappend++; /* find end of string */
  do {
    sprintf(fnappend,"%05d.csv",suffix);
    suffix++;
  } while(access(filename,F_OK) == 0);

  /* open file */
  conf->fdesc = fopen(filename, "a");
  if(conf->fdesc == NULL) error(1,ERROR_PLUGIN,"cannot append to log");

  /* print hello string if consoleif is disabled */
  if(logger_be_quiet(aldl) == 0) {
    printf("datalogger: Logging data to file: %s\n",filename);
  }

  free(filename); /* shouldn't need to re-open it */
}

datalogger_conf_t *datalogger_load_config(aldl_conf_t *aldl) {
  datalogger_conf_t *conf = smalloc(sizeof(datalogger_conf_t));
  if(aldl->datalogger_config == NULL) error(1,ERROR_CONFIG,
                               "no datalogger config file specified");
  conf->dconf = dfile_load(aldl->datalogger_config);
  if(conf->dconf == NULL) error(1,ERROR_CONFIG,
                                  "datalogger config file missing");
  dfile_t *config = conf->dconf;
  conf->autostart = configopt_int(config,"AUTOSTART",0,1,1);
  conf->log_all = configopt_int(config,"LOG_ALL",0,1,0);
  conf->log_filename = configopt_fatal(config,"LOG_FILENAME");
  conf->sync = configopt_int(config,"SYNC",0,1,1);
  conf->skip = configopt_int(config,"SKIP",0,1,1);
  conf->marker = configopt_int(config,"MARKER",0,10000,100);
  conf->rate = configopt_int(config,"RATE",1,10000,1);
  return conf;
}

int logger_be_quiet(aldl_conf_t *aldl) {
  if(aldl->consoleif_enable == 1) return 1;
  return 0;
}

