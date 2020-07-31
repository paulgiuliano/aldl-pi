
#ifndef _SERIO_H
#define _SERIO_H

#include "aldl-types.h"

/************ SCOPE *********************************
  Each serial module must contain these functions.
****************************************************/

/* initalize the serial handler */
int serial_init(char *port);

/* write buffer *str to the serial port, up to len bytes */
int serial_write(byte *str, int len);

/* read data from the serial port to buf, returns number of bytes read.
   only reads UP TO len, doesn't stick around waiting for more data if it
   isn't there. */
int serial_read(byte *str, int len);

/* clears any i/o buffers */
void serial_purge(); /* both buffers */
void serial_purge_rx(); /* rx only */
void serial_purge_tx(); /* tx only */

/* device search helper */
void serial_help_devs();

/* get serial status 1=OK */
int serial_get_status();

#endif

