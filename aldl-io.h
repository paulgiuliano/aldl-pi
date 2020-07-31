#ifndef ALDLIO_H
#define ALDLIO_H

#include "aldl-types.h"

/************ SCOPE *********************************
  Include this file.  All useful functions are
  declared here.
****************************************************/

/* diagnostic comms ------------------------------*/

int aldl_reconnect(); /* go into diagnostic mode, returns 1 on success */

/* fills the data section of the packet def with data, or sets it to zero if
   fail, and returns NULL */
byte *aldl_get_packet(aldl_packetdef_t *p);

/* generate request strings, returns allocated memory (free when finished) */
byte *generate_request(byte mode, byte message, aldl_commdef_t *comm);
byte *generate_mode(byte mode, aldl_commdef_t *comm);

/* use generate_request to fill a packet mode str */
byte *generate_pktcommand(aldl_packetdef_t *packet, aldl_commdef_t *comm);

/* add a command to the aux command queue, which will be sent to the datastream
   in between data acq iterations.  the command is RAW and must include all
   necessary prefixes, suffixes, and checksums. */
void aldl_add_command(byte *command, byte length, int delay);

/* pop a command from the aux command queue.  for use by the acq thread only */
aldl_comq_t *aldl_get_command();

/* buffer management --------------------------------------*/

/* WARNING: only the acquisition loop should use these functions */

/* initial configuration of the data structures used for record storage ... */
void aldl_data_init(aldl_conf_t *aldl);

/* allocate communications static buffer, call in main once and leave it */
void alloc_commbuf();

/* process data from all packets, create a record, and link it to the list */
aldl_record_t *process_data(aldl_conf_t *aldl);

/* set up lock structures */
void init_locks();

/* record selection ---------------------------------------*/

/* return the newest or next record in the linked list.  if there is no such
   record, return NULL. */
aldl_record_t *newest_record(aldl_conf_t *aldl);
aldl_record_t *next_record(aldl_record_t *rec);

/* return the newest or next record in the linked list.  if there is no such
   record, wait forever until one is available, unless the connection to the
   ECM is lost, in which case return NULL. */
aldl_record_t *newest_record_wait(aldl_conf_t *aldl, aldl_record_t *rec);
aldl_record_t *next_record_wait(aldl_conf_t *aldl, aldl_record_t *rec);

/* return the newest or next record in the linked list.  if there is no such
   record, wait forever until one is available.  never return anything but a
   valid record. */
aldl_record_t *next_record_waitf(aldl_conf_t *aldl, aldl_record_t *rec);
aldl_record_t *newest_record_waitf(aldl_conf_t *aldl, aldl_record_t *rec);

/* get definition or data array index, returns -1 if not found */
int get_index_by_name(aldl_conf_t *aldl, char *name);

/* connection state management ----------------------------*/

/* this pauses until a 'connected' state is detected */
void pause_until_connected(aldl_conf_t *aldl);

/* this pauses until the buffer is full */
void pause_until_buffered(aldl_conf_t *aldl);

/* get/set connection state */
aldl_state_t get_connstate(aldl_conf_t *aldl);
void set_connstate(aldl_state_t s, aldl_conf_t *aldl);

/* misc locking -------------------------------------------*/

/* lock and unlock the statistics structure */
void lock_stats();
void unlock_stats();

/* terminating functions -------------------------------*/

void serial_close(); /* close the serial port */

/* misc. useful functions ----------------------*/

/* generate a checksum byte */
byte checksum_generate(byte *buf, int len);

/* generate a msg length byte */
#define calc_msglength(LEN) (byte)(LEN + MSGLENGTH_MAGICNUMBER)

/* test checksum byte of buf, 1 if ok */
int checksum_test(byte *buf, int len);

/* compare a byte string n(eedle) in h(aystack), nonzero if found */
int cmp_bytestring(byte *h, int hsize, byte *n, int nsize);

/* print a string of bytes in hex format */
void printhexstring(byte *str, int length);

/* return a string that describes a connection state */
char *get_state_string(aldl_state_t s);

/* cleanup function in main */
void main_exit();

#endif
