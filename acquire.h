#ifndef _ACQUIRE_H
#define _ACQUIRE_H

/************ SCOPE *********************************
  This object contains one event loop, that drives
  the data aquisition thread.  Maintaining connection
  statefulness and retrieving all data is done here.
****************************************************/

void *aldl_acq(void *aldl_in);

#endif
