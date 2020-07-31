#ifndef _ALDLCOMM_H
#define _ALDLCOMM_H

/* sends a request, delays for a calculated time, and waits for an echo.  if
   the request is successful, returns 1, otherwise 0. */
int aldl_request(byte *pkt, int len);

/* read the number of bytes specified, into str.  waits until the correct
   number of bytes were read, and returns 1, or the timeout (in ms)
   has expired, and returns 0. */
inline int read_bytes(byte *str, int bytes, int timeout);

/* the same as serial_read_bytes, but discards the bytes.  useful for
   ignoring a known-length string of bytes. */
inline int skip_bytes(int bytes, int timeout);

/* look for str in the serial stream up to a length of max, or a time of
   timeout. */
int listen_bytes(byte *str, int len, int max, int timeout);

#endif
