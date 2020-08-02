#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <ftdi.h>

#include "serio.h"
#include "aldl-io.h"
#include "error.h"
#include "config.h"
#include "useful.h"

/************ SCOPE *********************************
  Primary serial driver, uses libftdi for raw usb
  access to ftdi serial adaptors.  Reccommended.
****************************************************/

/****************GLOBALSn'STRUCTURES*****************************/

/* global ftdi context pointer */
struct ftdi_context *ftdi;

/* simple connection state status bit */
byte ftdistatus;

/* number of failed io attempts */
int iofail;

/* storage for serial init string */
char *serialstr;

/***************FUNCTION DEFS************************************/

/* init the ftdi driver by a special port description:
   d:devicenode (usually at /proc/bus/usb)
   i:vendor:product
   i:vendor:product:index
   s:vendor:product:serial
*/

/* special ftdi error handlers */
inline int ftdierror(int loc,int errno); /* prints error but continues */
inline void ftdifatal(int loc,int errno); /* bails entirely on error */
inline int ftdierror_counter(int loc,int errno); /* counts errors + recovery */

/* enter recovery mode */
inline void ftdi_recovery();

/****************FUNCTIONS**************************************/

void serial_close() {
  if(ftdistatus > 0) {
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);
  }
}

int serial_init(char *port) {
  #ifdef SERIAL_VERBOSE
  printf("serial_init opening port @ %s with method ftdi\n",port);
  #endif

  ftdistatus = 0;
  iofail = 0;
  int res = -1;
  serialstr = port;

  /* new ftdi instance */
  if((ftdi = ftdi_new()) == NULL) {
    error(1,ERROR_FTDI,"ftdi_new failed");
  }
  
  res = ftdi_usb_open_string(ftdi,port);
  #ifdef FTDI_RETRY_USB
  if(res<-5) ftdifatal(2,res); /* fatal open errors */
  if(res<0) {
    fprintf(stderr,"FTDI Device @ %s isn't connected.  Retrying...\n",port);
    while(res<0) { /* device is probably just disconnected */
      ftdierror(2,res); /* if SERIAL_VERBOSE set, display actual err */
      sleep(FTDI_RETRY_DELAY);
      res = ftdi_usb_open_string(ftdi,port);
    }
  }
  #else
  ftdifatal(2,res);
  #endif

  #ifdef SERIAL_VERBOSE
  printf("init ftdi userland driver appears sucessful...\n");
  #endif

  /* set baud rate */
  ftdierror(3,ftdi_set_baudrate(ftdi,FTDI_BAUD));

  /* set latency timer */
  ftdierror(3,ftdi_set_latency_timer(ftdi,2));

  ftdistatus = 1;
  return 1;
}

void serial_purge() {
  ftdierror_counter(88,ftdi_usb_purge_buffers(ftdi));
  #ifdef SERIAL_VERBOSE
  printf("SERIAL PURGE RX/TX\n");
  #endif
}

void serial_purge_rx() {
  ftdierror_counter(88,ftdi_usb_purge_rx_buffer(ftdi));
  #ifdef SERIAL_VERBOSE
  printf("SERIAL PURGE RX\n");
  #endif
}

void serial_purge_tx() {
  ftdierror_counter(88,ftdi_usb_purge_tx_buffer(ftdi));
  #ifdef SERIAL_VERBOSE
  printf("SERIAL PURGE TX\n");
  #endif
}

int serial_write(byte *str, int len) {
  #ifdef RETARDED
    /* check for 0 length or null string */
    if(str == NULL || len == 0) {
      #ifdef SERIAL_VERBOSE
        printf("non-fatal, attempted serial write of 0 len or null string\n");
      #endif
      return 1;
    }
  #endif
  #ifdef SERIAL_SUPERVERBOSE
  printf("WRITE: ");
  printhexstring(str,len);
  #endif

  ftdierror_counter(6,ftdi_write_data(ftdi,(unsigned char *)str,len));
  return 0;
}

int serial_read(byte *str, int len) {
  #ifdef RETARDED
    /* check for null string or 0 length */
    if(str == NULL || len == 0) {
      #ifdef SERIAL_VERBOSE
        printf("non-fatal, attempted serial read to NULL buffer or 0 len\n");
      #endif
      return 0;
    }
  #endif
  int resp = 0; /* to store response from whatever read */
  resp = ftdi_read_data(ftdi,(unsigned char *)str,len);
  ftdierror_counter(22,resp);
  #ifdef SERIAL_SUPERVERBOSE
  if(resp > 0) {
    printf("READ %i of %i bytes: ",resp,len);
    printhexstring(str,resp);
  } else {
    printf("EMPTY\n");
  }
  #endif

  return resp; /* return number of bytes read, or zero */
}

inline void ftdifatal(int loc,int errno) {
  if(ftdierror(loc,errno) > 0) {
    error(1,ERROR_FTDI,"*** See above FTDI DRIVER error message @ stderr");
  }
}

inline int ftdierror(int loc,int errno) {
  if(errno>=0) { /* no error */
    return 0;
  } else {
    #ifdef SERIAL_VERBOSE
    error(0,ERROR_FTDI,"FTDI DRIVER: %i, %s\n",errno,ftdi_get_error_string(ftdi));
    #endif
    return 1;
  }
}

inline int ftdierror_counter(int loc,int errno) {
  if(errno>=0) { /* no error */
    iofail = 0;
    return 0;
  } else {
    iofail++;
    #ifdef SERIAL_VERBOSE
    fprintf(stderr,"FTDI DRIVER: %i, %s\n",errno,ftdi_get_error_string(ftdi));
    #endif
    if(iofail > FTDI_MAXFAIL) ftdi_recovery();
    return 1;
  }
}

inline void ftdi_recovery() {
  #ifdef FTDI_ATTEMPT_RECOVERY
  ftdistatus=0;
    #ifdef SERIAL_VERBOSE
    fprintf(stderr,"FTDI DRIVER: Triggered recovery mode...\n");
    #endif
  serial_close();
  msleep(500);
  serial_init(serialstr);
  #endif
}

void serial_help_devs() {
  int ret, i;
  struct ftdi_device_list *devlist, *curdev;
  char mfr[128], desc[128];

  if((ftdi = ftdi_new()) == NULL) error(1,ERROR_FTDI,"ftdi_new failed");

  ret = ftdi_usb_find_all(ftdi, &devlist, 0x0403, 0x6001);
  ftdifatal(99,ret);
  printf("Number of FTDI devices found: %d\n", ret);

  i=0;
  for (curdev = devlist; curdev != NULL; i++) {
    printf("Checking device: %d\n", i);
    ftdifatal(101,ftdi_usb_get_strings(ftdi, curdev->dev, mfr, 128,
                  desc,128,NULL,0));
    printf("Manufacturer: %s, Description: %s\n\n", mfr, desc);
    curdev = curdev->next;
  }

  ftdi_list_free(&devlist);
  ftdi_deinit(ftdi);
}

int serial_get_status() {
  return ftdistatus;
}
