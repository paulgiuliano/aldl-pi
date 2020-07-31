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
#include "error.h"
#include "aldl-io.h"
#include "useful.h"

/************ SCOPE *********************************
  This object contains configuration file loading
  routines, all based around two symbols, the equals
  sign and the double quote.
****************************************************/

/* ------- GLOBAL----------------------- */

aldl_conf_t *aldl; /* aldl data structure */
aldl_commdef_t *comm; /* comm specs */

/* ------- LOCAL FUNCTIONS ------------- */

/* is a char whitespace ...? */
inline int is_whitespace(char ch);

/* following functions use internally stored config and should never be
   exported ... */

/* get a packet config string */
char *pktconfig(char *buf, char *parameter, int n);
char *dconfig(char *buf, char *parameter, int n);

/* initial memory allocation routines */
void aldl_alloc_a(); /* fixed structures */
void aldl_alloc_b(); /* definition arrays */
void aldl_alloc_c(); /* more data space */

/* config file loading */
void load_config_a(dfile_t *config); /* load data to alloc_a structures */
void load_config_b(dfile_t *config); /* load data to alloc_b structures */
void load_config_c(dfile_t *config);
char *load_config_root(dfile_t *config); /* returns path to sub config */

aldl_conf_t *aldl_setup() {
  /* load root config file ... */
  dfile_t *config = dfile_load(ROOT_CONFIG_FILE);
  if(config == NULL) error(1,ERROR_CONFIG,
                        "cant load root config file: %s", ROOT_CONFIG_FILE);
  #ifdef DEBUGCONFIG
  print_config(config);
  #endif

  /* allocate main (predictable) structures */
  aldl_alloc_a(); /* creates aldl_conf_t structure ... */
  #ifdef DEBUGCONFIG
  printf("loading root config...\n");
  #endif

  char *configfile = load_config_root(config);

  /* load def config file ... */
  config = dfile_load(configfile); /* re-use pointer */
  if(config == NULL) error(1,ERROR_CONFIG,
                        "cant load definition file: %s",configfile);
  #ifdef DEBUGCONFIG
  print_config(config);
  printf("configuration, stage A...\n");
  #endif
  load_config_a(config);

  #ifdef DEBUGCONFIG
  printf("configuration, stage B...\n");
  #endif
  aldl_alloc_b();
  load_config_b(config);
  #ifdef DEBUGCONFIG
  printf("configuration, stage C...\n");
  #endif
  aldl_alloc_c();
  load_config_c(config);
  #ifdef DEBUGCONFIG
  printf("configuration complete.\n");
  #endif
  return aldl;
}

void aldl_alloc_a() {
  /* primary aldl configuration structure */
  aldl = smalloc(sizeof(aldl_conf_t));
  memset(aldl,0,sizeof(aldl_conf_t));

  #ifdef DEBUGMEM
  printf("aldl_conf_t: %i bytes\n",(int)sizeof(aldl_conf_t));
  #endif

  /* communication definition */
  comm = smalloc(sizeof(aldl_commdef_t));
  memset(comm,0,sizeof(aldl_commdef_t));
  aldl->comm = comm; /* link to conf */

  #ifdef DEBUGMEM
  printf("aldl_commdef_t: %i bytes\n",(int)sizeof(aldl_commdef_t));
  #endif

  /* stats tracking structure */
  aldl->stats = smalloc(sizeof(aldl_stats_t));
  if(aldl->stats == NULL) error(1,ERROR_MEMORY,"stats alloc");
  memset(aldl->stats,0,sizeof(aldl_stats_t));

  #ifdef DEBUGMEM
  printf("aldl_stats_t: %i bytes\n",(int)sizeof(aldl_stats_t));
  #endif

}

char *load_config_root(dfile_t *config) {
  aldl->serialstr = configopt(config,"PORT",NULL);
  aldl->bufsize = configopt_int(config,"BUFFER",10,10000,200);
  aldl->bufstart = configopt_int(config,"START",10,10000,aldl->bufsize / 2);
  aldl->minmax = configopt_int(config,"MINMAX",0,1,1);
  aldl->maxfail = configopt_int(config,"MAXFAIL",1,1000,6);
  aldl->rate = configopt_int(config,"ACQRATE",0,100000,0);
  /* plugins */
  aldl->consoleif_enable = configopt_int(config,"CONSOLEIF_ENABLE",0,1,0);
  aldl->datalogger_enable = configopt_int(config,"DATALOGGER_ENABLE",0,1,0);
  aldl->remote_enable = configopt_int(config,"REMOTE_ENABLE",0,1,0);
  aldl->dataserver_enable = configopt_int(config,"DATASERVER_ENABLE",0,1,0);
  aldl->datalogger_config = configopt(config,"DATALOGGER_CONFIG",NULL);
  aldl->consoleif_config = configopt(config,"CONSOLEIF_CONFIG",NULL);
  aldl->dataserver_config = configopt(config,"DATASERVER_CONFIG",NULL);
  /* return definition file path */
  return configopt_fatal(config,"DEFINITION"); /* path not stored ... */
}

void load_config_a(dfile_t *config) {
  comm->checksum_enable = configopt_int(config,"CHECKSUM_ENABLE",0,1,1);;
  comm->pcm_address = configopt_byte_fatal(config,"PCM_ADDRESS");
  comm->chatterwait = configopt_int(config,"IDLE_ENABLE",0,1,1);
  if(comm->chatterwait == 1) { /* idle chatter enabled */
    comm->idledelay = configopt_int(config,"IDLE_DELAY",0,5000,10);
  }
  comm->shutuprepeat = configopt_int(config,"SHUTUP_REPEAT",0,5000,1);
  if(comm->shutuprepeat > 0) { /* shutup enabled */
    comm->shutupcommand = generate_mode(configopt_byte_fatal(config,"SHUTUP_MODE"),comm);
    comm->returncommand = generate_mode(configopt_byte_fatal(config,"RETURN_MODE"),comm);
    comm->shutuprepeatdelay = configopt_int(config,"SHUTUP_DELAY",0,5000,75);
    comm->shutup_time = configopt_int(config,"SHUTUP_TIME",0,65535,2500);
  }
  comm->n_packets = configopt_int(config,"N_PACKETS",1,99,1);
  comm->byteorder = configopt_int(config,"BYTEORDER",0,1,0);
  aldl->n_defs = configopt_int_fatal(config,"N_DEFS",1,512);
}

void aldl_alloc_b() {
  /* allocate space to store packet definitions */
  comm->packet = smalloc(sizeof(aldl_packetdef_t) * comm->n_packets);

  #ifdef DEBUGMEM
  printf("aldl_commdef_t: %i bytes\n",(int)sizeof(aldl_commdef_t));
  #endif
}

void load_config_b(dfile_t *config) {
  int x;
  char *pktname = smalloc(50);
  for(x=0;x<comm->n_packets;x++) {
    comm->packet[x].id = configopt_byte_fatal(config,pktconfig(pktname,"ID",x));
    comm->packet[x].length = configopt_int_fatal(config,pktconfig(pktname,
                                                "SIZE",x),1,255);
    comm->packet[x].offset = configopt_int(config,pktconfig(pktname,
                                                 "OFFSET",x),0,254,3);
    comm->packet[x].frequency = configopt_int(config,pktconfig(pktname,
                                                 "FREQUENCY",x),0,1000,1);
    generate_pktcommand(&comm->packet[x],comm);
    #ifdef DEBUGCONFIG
    printf("loaded packet %i\n",x);
    #endif
  }
  free(pktname);
}

void aldl_alloc_c() {
  /* storage for raw packet data */
  int x = 0;
  for(x=0;x<comm->n_packets;x++) {
    comm->packet[x].data = smalloc(comm->packet[x].length);
    #ifdef DEBUGMEM
    printf("packet %i raw storage: %i bytes\n",x,comm->packet[x].length);
    #endif
    if(comm->packet[x].data == NULL) error(1,ERROR_MEMORY,"pkt data");
  }

  /* storage for data definitions */
  aldl->def = smalloc(sizeof(aldl_define_t) * aldl->n_defs);
  #ifdef DEBUGMEM
  printf("aldl_define_t definition storage: %i bytes\n",
              (int)sizeof(aldl_define_t) * aldl->n_defs);
  #endif
}

void load_config_c(dfile_t *config) {
  int x=0;
  char *configstr = smalloc(50);
  char *tmp;
  char f; /* filter tmp */
  aldl_define_t *d;
  int z;

  for(x=0;x<aldl->n_defs;x++) {
    d = &aldl->def[x]; /* shortcut to def */
    tmp=configopt(config,dconfig(configstr,"TYPE",x),"FLOAT");
    if(rf_strcmp(tmp,"BINARY") == 1 || rf_strcmp(tmp,"ERROR") == 1) {
      d->type=ALDL_BOOL;
      d->binary=configopt_int_fatal(config,dconfig(configstr,"BINARY",x),0,7);
      d->invert=configopt_int(config,dconfig(configstr,"INVERT",x),0,1,0);
      d->uom=NULL;
      if(rf_strcmp(tmp,"ERROR") == 1) d->err = 1;
    } else {
      if(rf_strcmp(tmp,"FLOAT") == 1) {
        d->type=ALDL_FLOAT;
        d->precision=configopt_int(config,dconfig(configstr,"PRECISION",x),0,1000,0);
        d->min.f=configopt_float(config,dconfig(configstr,"MIN",x),0);
        d->max.f=configopt_float(config,dconfig(configstr,"MAX",x),9999999);
        d->adder.f=configopt_float(config,dconfig(configstr,"ADDER",x),0);
        d->multiplier.f=configopt_float(config,dconfig(configstr,"MULTIPLIER",x),1);
        d->alarm_low.f=configopt_float(config,dconfig(configstr,"ALARM_LOW",x),0);
        d->alarm_high.f=configopt_float(config,dconfig(configstr,"ALARM_HIGH",x),0);
      } else if(rf_strcmp(tmp,"INT") == 1) {
        d->type=ALDL_INT; 
        d->min.i=configopt_int(config,dconfig(configstr,"MIN",x),-32678,32767,0);
        d->max.i=configopt_int(config,dconfig(configstr,"MAX",x),-32678,32767,65535);
        d->adder.i=configopt_int(config,dconfig(configstr,"ADDER",x),-32678,32767,0);
        d->multiplier.i=configopt_int(config,dconfig(configstr,"MULTIPLIER",x),
                                         -32678,32767,1);
        d->alarm_low.i=configopt_int(config,dconfig(configstr,"ALARM_LOW",x),
                                                  0,32767,0);
        d->alarm_high.i=configopt_int(config,dconfig(configstr,"ALARM_HIGH",x),
                                                  0,32767,0);
      } else {
        error(1,ERROR_CONFIG,"invalid data type %s in def %i",tmp,x);
      }
      d->uom=configopt(config,dconfig(configstr,"UOM",x),NULL);
      /* check for illegal chars in uom */
      if(d->uom != NULL) {
        f = rf_listcmp(d->uom, CONFIG_BAD_CHARS);
        if(f != 0) {
          error(1,ERROR_CONFIG,"bad char %c in UOM of def %i",f,x);
        }
      }
      d->size=configopt_int(config,dconfig(configstr,"SIZE",x),1,32,8);     
      /* FIXME no support for signed input type */
    }
    d->alarm_low_enable=configopt_int(config,dconfig(configstr,
                        "ALARM_LOW_ENABLE",x),0,1,0);
    d->alarm_high_enable=configopt_int(config,dconfig(configstr,
                        "ALARM_HIGH_ENABLE",x),0,1,0);
    d->offset=configopt_byte_fatal(config,dconfig(configstr,"OFFSET",x));
    d->packet=configopt_byte(config,dconfig(configstr,"PACKET",x),0x00);
    if(d->packet > comm->n_packets - 1) error(1,ERROR_CONFIG,
                        "packet %i out of range in def %i",d->packet,x);
    d->name=configopt_fatal(config,dconfig(configstr,"NAME",x));
    /* check for illegal chars in name */
    f = rf_listcmp(d->name, CONFIG_BAD_CHARS);
    if(f != 0) {
      error(1,ERROR_CONFIG,"bad char %c in NAME of def %i",f,x);
    }
    for(z=x-1;z>=0;z--) { /* check for duplicate name */
      if(rf_strcmp(aldl->def[z].name,d->name) == 1) error(1,ERROR_CONFIG,
                    "duplicate name %s at id %i and %i",
                     d->name,x,z);
    }
    d->description=configopt_fatal(config,dconfig(configstr,"DESC",x));
    d->log=configopt_int(config,dconfig(configstr,"LOG",x),0,1,0);
    d->display=configopt_int(config,dconfig(configstr,"DISPLAY",x),0,1,0);
    #ifdef DEBUGCONFIG
    printf("loaded definition %i\n",x);
    #endif
  }
  free(configstr);
}

char *configopt_fatal(dfile_t *config, char *str) {
  char *val = configopt(config,str,NULL);
  if(val == NULL) error(1,ERROR_CONFIG_MISSING,str);
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
  if(x < min || x > max) error(1,ERROR_CONFIG,
                  "%s must be between %i and %i",str,min,max);
  return x;
}

int configopt_int_fatal(dfile_t *config,char *str, int min, int max) {
  int x = atoi(configopt_fatal(config,str));
  if(x < min || x > max) error(1,ERROR_CONFIG,
                  "%s must be between %i and %i",str,min,max);
  return x;
}

byte configopt_byte(dfile_t *config,char *str, byte def) {
  char *in = configopt(config,str,NULL);
  #ifdef DEBUGCONFIG
  if(in == NULL) {
    printf("caught default value for %s: ",str);
    printhexstring(&def,1);
    return def;
  }
  #else
  if(in == NULL) return def;
  #endif
  return hextobyte(in);
}

byte configopt_byte_fatal(dfile_t *config,char *str) {
  char *in = configopt_fatal(config,str);
  return hextobyte(in);
}

char *pktconfig(char *buf, char *parameter, int n) {
  sprintf(buf,"P%i.%s",n,parameter);
  return buf;
}

char *dconfig(char *buf, char *parameter, int n) {
  sprintf(buf,"D%i.%s",n,parameter);
  return buf;
}

dfile_t *dfile_load(char *filename) {
  char *data = load_file(filename);
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
      while(*c != '"') {
         if(*c == EOF) {
           error(1,ERROR_CONFIG,"Unterminated quote in config file");
         }
         c++;
      }
      c[0] = 0;
    }
  }
}

dfile_t *dfile(char *data) {
  /* allocate base structure */
  dfile_t *out = smalloc(sizeof(dfile_t));

  /* initial allocation of pointer array */
  out->p = smalloc(sizeof(char*) * MAX_PARAMETERS);
  out->v = smalloc(sizeof(char*) * MAX_PARAMETERS);
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
      /* find parameter value */
      while(is_whitespace(*cx) != 1) {
        if(*cx == '"') { /* skip quoted string */
          cx++;
          while(cx[0] != '"') {
            if(cx[1] == 0) error(1,ERROR_CONFIG,"Unterminated quote in config");
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
      /* find parameter name */
      while(is_whitespace(*cx) != 1) {
        if(*cx == '"') { /* skip quoted string */
          cx--;
          if(cx < data) error(1,ERROR_CONFIG,"Unterminated quote in config");
          while(cx[0] != '"') {
            if(cx == data) error(1,ERROR_CONFIG,"Unterminated quote in config"); 
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
  char *newdata = smalloc(ttl); /* new storage */
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
  if(ch == 0 || ch == ' ' || ch == '\n' || ch == '=') return 1;
  return 0;
}

/* read file into memory */
char *load_file(char *filename) {
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
  char *buf = smalloc(sizeof(char) * ( flength + 1));
  if(fread(buf,1,flength,fdesc) != flength) {
    free(buf);
    fclose(fdesc);
    return NULL;
  }
  fclose(fdesc);
  buf[flength] = 0;
  return buf;
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
