#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <termios.h>

#include "serio.h"
#include "aldl-io.h"
#include "error.h"
#include "config.h"
#include "useful.h"

/************ SCOPE *********************************
  Alternate serial driver, uses standard linux dev
  for non-ftdi devices.
****************************************************/

/* bail with fatal error since driver doesn't work yet. */
#define SERIAL_DRIVER_BROKEN

/****************GLOBALS****************************************/

int fd; /* file descriptor of serial port */
struct termios term_old,term_new;

/****************FUNCTIONS**************************************/

void serial_close() {
  /* close serial port */
}

int serial_init(char *port) {
  #ifdef SERIAL_DRIVER_BROKEN
  /* this error is fatal. */
  error(EFATAL,ERROR_SERIAL,"The serial driver doesn't work yet.  Use FTDI.");
  #endif

  /* open serial port */
  fd = open(port, O_RDWR | O_NOCTTY);
  if(fd < 0) { /* failed to open port */ 
    error(EFATAL,ERROR_SERIAL,"Couldn't open file descriptor for device %s",
          port);
  }

  tcgetattr(fd,&term_old); /* save old attr */
  bzero(&term_new,sizeof(term_new)); /* clear new struct */

  /* configure serial device here */

  return 1;
}

void serial_purge() {
  /* purge all buffers */
}

void serial_purge_rx() {
  /* purge rx buffer */
}

void serial_purge_tx() {
  /* purge tx buffer */
}

int serial_write(byte *str, int len) {
  /* write string */
  return 1;
}

int serial_read(byte *str, int len) {
  /* read bytes into str */
  return 0; /* return number of bytes read, or zero */
}

void serial_help_devs() {
  printf("The tty serial driver doesn't support this command.\n");
}

int serial_get_status() {
  return 0;
}
