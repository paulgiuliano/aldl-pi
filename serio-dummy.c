#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "serio.h"
#include "aldl-io.h"
#include "error.h"
#include "config.h"

/************ SCOPE *********************************
  A dummy serial handler object that pretends to be
  a fake LT1 (EE) ECM and just send random garbage
  (00-FF) as a datastream.
****************************************************/

/****************GLOBALSn'STRUCTURES*****************************/

unsigned char *databuff;
char txmode;

void gen_pkt();

/****************FUNCTIONS**************************************/

void serial_close() {
  #ifdef SERIAL_VERBOSE
  printf("SERIAL CLOSE (discarded)\n");
  #endif
  return;
}

void gen_pkt() {
  int x;
  databuff[0]=0xF4;
  databuff[1]=0x92;
  databuff[2]=0x01;
  for(x=3;x<63;x++) databuff[x] = ( (byte)rand() % 256 ) - 1;
  databuff[63] = checksum_generate(databuff,63);
  #ifdef DUMMY_CORRUPTION_ENABLE
  /* insert random bullshit sometimes */
  if( ( (byte)rand() % 100 ) < DUMMY_CORRPUTION_RATE ) {
    #ifdef SERIAL_VERBOSE
    printf("serial dummy driver - inserting random corruption!\n");
    #endif
    for(x=0;x<=DUMMY_CORRUPTION_AMOUNT;x++) {
      databuff[(byte)rand() % 60] = ( (byte)rand() % 256 ) - 1;
    }
  }
  #endif
}

int serial_init(char *port) {
  #ifdef SERIAL_VERBOSE
  printf("Serial dummy driver initialized!\n");
  #endif
  txmode=0;
  databuff=malloc(64);
  return 1;
}

void serial_purge() {
  #ifdef SERIAL_VERBOSE
  printf("SERIAL PURGE RX/TX (Dummy Ignored)\n");
  #endif
  return;
}

void serial_purge_rx() {
  #ifdef SERIAL_VERBOSE
  printf("SERIAL PURGE RX (Dummy Ignored)\n");
  #endif
  return;
}

void serial_purge_tx() {
  #ifdef SERIAL_VERBOSE
  printf("SERIAL PURGE TX (Dummy Ignored)\n");
  #endif
  return;
}

int serial_write(byte *str, int len) {
  #ifdef SERIAL_VERBOSE
  printf("WRITE: ");
  printhexstring(str,len); 
  #endif
  /* determine mode */
  if(len == 4 && str[0] == 0xF4 && str[1] == 0x56 && \
     str[2] == 0x08 && str[3] == 0xAE) {
     txmode = 1;
  }
  return 0;
}

inline int serial_read(byte *str, int len) {
  if(txmode == 0) { /* idle traffic req */
    usleep(SERIAL_BYTES_PER_MS * 64 * 1000); /* fake baud delay */
    str[0] = 0x33;
    txmode++;
    #ifdef SERIAL_VERBOSE
    printf("DUMMY MODE: Idle Traffic Req: ");
    printhexstring(str,1);
    #endif
    return 1;
  } if(txmode == 1) { /* shutup req */
    usleep(SERIAL_BYTES_PER_MS * 5 * 1000); /* fake baud delay */
    str[0] = 0xF4;
    str[1] = 0x56;
    str[2] = 0x08;
    str[3] = 0xAE;
    txmode++;
    #ifdef SERIAL_VERBOSE
    printf("DUMMY MODE: Silence Request: ");
    printhexstring(str,4);
    #endif
    return 4;
  } if(txmode == 2) { /* data request reply */
    usleep(SERIAL_BYTES_PER_MS * 5 * 1000); /* fake baud delay */
    txmode = 3; 
    str[0] = 0xF4;
    str[1] = 0x57;
    str[2] = 0x01;
    str[3] = 0x00;
    str[4] = 0xB4;
    #ifdef SERIAL_VERBOSE
    printf("DUMMY MODE: Data Req. Reply: ");
    printhexstring(str,5);
    #endif
    return 5;
  } if(txmode == 3) { /* data send */
    usleep(SERIAL_BYTES_PER_MS * len * 1000); /* fake baud delay */
    txmode = 2;
    gen_pkt();
    #ifdef SERIAL_VERBOSE
    printf("DUMMY MODE: Generated packet...\n");
    #endif
    int x;
    for(x=0;x<len;x++) {
      str[x] = databuff[x]; 
    }
    return len;
  }
  return 0;
}

void serial_help_devs() {
  error(1,ERROR_GENERAL,"this serial driver has no devices......");
}

int serial_get_status() {
  return 1;
}
