#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "error.h"

/************ SCOPE *********************************
  Error handing routines.
****************************************************/

void error(char *str, ...) {
  va_list arg;
  fprintf(stderr,"ALDL-ANALYZER: ");
  if(str != NULL) {
    fprintf(stderr,"ERROR: ");
    va_start(arg,str);
    vfprintf(stderr,str,arg);
    va_end(arg);
    fprintf(stderr,"\n");
  }
}

#endif

