#ifndef _ERROR_H
#define _ERROR_H

/************ SCOPE *********************************
  Error handling routines.
****************************************************/

#define N_ERRORCODES 13

typedef enum _errtype {
  EFATAL=1,
  ENOTICE=0
} errtype_t;

typedef enum _error {
  ERROR_GENERAL=0,
  ERROR_NULL=1,
  ERROR_MEMORY=2,
  ERROR_FTDI=3, /* legacy */
  ERROR_SERIAL=3,
  ERROR_RANGE=4,
  ERROR_TIMING=5,
  ERROR_CONFIG=6,
  ERROR_BUFFER=7,
  ERROR_CONFIG_MISSING=8,
  ERROR_PLUGIN=9,
  ERROR_LOCK=10,
  ERROR_NET=11,
  ERROR_RETARD=12
} error_t;

/* main error handler.  fatal=1 to force exit on error, otherwise the error
   is non-fatal... */

void error(errtype_t t,error_t code, char *str, ...);

/* retard check for a null pointer passed to a function where it shouldn't be */
void retardptr(void *p, char *note);

#endif
