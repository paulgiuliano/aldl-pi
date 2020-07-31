#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "error.h"
#include "aldl-io.h"

/************ SCOPE *********************************
  Error handing routines.
****************************************************/

/* list of error code to string, array index matches enumerated values in
   error_t.  these will have ERROR printed AFTER them, so GENERAL will come
   out as GENERAL ERROR. */

char errstr[N_ERRORCODES][24] = {
"GENERAL",
"NULL",
"OUT OF MEMORY",
"COMM DRIVER",
"OUT OF RANGE",
"TIMING",
"CONFIG",
"BUFFER",
"CONFIG OPTION MISSING",
"PLUGIN LOADING",
"THREADLOCKING",
"NETWORK",
"RETARD"
};

void error(errtype_t t, error_t code, char *str, ...) {
  va_list arg;
  fprintf(stderr,"ALDL-IO: ");
  fprintf(stderr,"%s ERROR (%i)\n",errstr[code],code);
  if(str != NULL) {
    fprintf(stderr,"NOTES: ");
    va_start(arg,str);
    vfprintf(stderr,str,arg);
    va_end(arg);
    fprintf(stderr,"\n");
  }
  #ifndef ALL_ERRORS_FATAL
  if(t == EFATAL) {
  #endif
    fprintf(stderr,"This error is fatal.  Exiting...\n");
    main_exit();
  #ifndef ALL_ERRORS_FATAL
  }
  #endif
}

#ifdef RETARDED
void retardptr(void *p, char *note) {
  error(1,ERROR_RETARD,"null pointer in %s");
}
#endif

