#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* local objects */
#include "error.h"
#include "config.h"
#include "config.h"
#include "aldl-io.h"
#include "acquire.h"
#include "useful.h"
#include "serio.h"

/************ SCOPE *********************************
  This object contains one event loop, that drives
  the data aquisition thread.  Maintaining connection
  statefulness and retrieving all data is done here.
****************************************************/

void *aldl_acq(void *aldl_in) {
  #ifdef VERBLOSITY
  printf("aldl_acq thread active\n");
  int ttlpkts = 0;
  #endif
  /* ---- main variables --------------- */
  aldl_conf_t *aldl = (aldl_conf_t *)aldl_in;
  aldl_commdef_t *comm = aldl->comm; /* direct reference to commdef */
  aldl_packetdef_t *pkt = NULL; /* temporary pointer to the packet def */
  aldl_comq_t *auxcommand = NULL;
  int pktfail = 0; /* marker for a failed packet in event loop */
  int npkt = 0; /* array index of packet to operate on */
  int buffered = 0;
  int serialdowntime = 0;
  aldl->ready = 0;

  /* sanity checks */
  if(aldl->rate > 200000) error(1,ERROR_TIMING,
                                    "acq delay (%i) too high",aldl->rate);
  if(comm->n_packets < 1) error(1,ERROR_RANGE,"no packets specified");

  /* timestamp for lag check */
  #ifdef LAGCHECK
  timespec_t lagtime;
  #endif

  /* prepare array for packet retrieval frequency tracking */
  int *freq_counter = smalloc(sizeof(int) * comm->n_packets);
  int freq_init;
  for(freq_init=0;freq_init < comm->n_packets; freq_init++) {
    /* if we init the frequency with freq max, that will ensure that each
       packet is iterated once at the beginning of the acq. routine */
    freq_counter[freq_init] = comm->packet[freq_init].frequency;
  }

  /* set timestamp */
  aldl->uptime = time(NULL);

  /* config vars and get initial stamp if packet rate tracking is enabled */
  #ifdef TRACK_PKTRATE
  timespec_t timestamp = get_time();
  int pktcounter = 0; /* how many packets between timestamps */
  #endif

  /* intial connection state */
  set_connstate(ALDL_CONNECTING,aldl);

  /* loop infinitely until ALDL_QUIT is set */
  while(get_connstate(aldl) != ALDL_QUIT) {

    /* iterate through all packets */
    for(npkt=0;npkt < comm->n_packets;npkt++) {

    /* ---- frequency select routine ---- */
    /* skip packet if frequency is 0 to match spec */
    if(comm->packet[npkt].frequency == 0) continue;
    if(freq_counter[npkt] < comm->packet[npkt].frequency) {
      /* frequency requirement not met */
      freq_counter[npkt]++;
      continue; /* go to next pkt */
    } else {
      /* reached frequency, reset to 1 */
      freq_counter[npkt] = 1;
    }

    pkt = &comm->packet[npkt]; /* pointer to the correct packet */

    /* this is a jump point for packet retry that skils the for loop and
       packet selector */
    PKTRETRY:

    /* handle pause condition */
    while(get_connstate(aldl) == ALDL_PAUSE) msleep(250);

    /* handle serial error */
    if(serial_get_status() != 1) {
      set_connstate(ALDL_SERIALERROR,aldl);
      while (serial_get_status() != 1) {
        /* keep track of how long we're down, since we're outside of lagcheck
           loop. */
        msleep(250);
        serialdowntime += 250;
      }
      /* assume comm ok */
      if(aldl->comm->shutup_time > 0 &&
         serialdowntime < aldl->comm->shutup_time) {
        set_connstate(ALDL_CONNECTED,aldl);
      }
    }

    /* this would seem an appropriate time to maintain the connection if it
       drops, or if it never existed ... if not, time for a delay */
    if(get_connstate(aldl) >= 10) { /* if in any sort of disconnected state */
      aldl_reconnect(comm); /* main connection happens here */
      set_connstate(ALDL_CONNECTED,aldl);
    #ifndef AGGRESSIVE
    } else {
      /* delay between data collection iterations */
      usleep(aldl->rate);
    #endif
    }

    /* reset lag check timer, note that the above instructions are not covered
       in lagtime measurement, so they need to be FAST .... */
    #ifdef LAGCHECK
    lagtime = get_time(); 
    #endif

    /* check if we're @ duration, and average the number of packets for
       statistical purposes */
    #ifdef TRACK_PKTRATE
    if(get_elapsed_ms(timestamp) >= PKTRATE_DURATION * 1000) {
      lock_stats();
      aldl->stats->packetspersecond = (float)pktcounter / PKTRATE_DURATION;
      unlock_stats();
      timestamp = get_time();
      pktcounter = 0;
    }
    #endif

    /* print debugging info */
    #ifdef VERBLOSITY
    printf("ACQUIRE pkt# %i, total %i\n",npkt,ttlpkts);
    ttlpkts++;
    #endif

    /* ------- command insertion routine -------------------- */

    auxcommand = aldl_get_command();
    if(auxcommand != NULL) { /* a command was found */
      serial_write(auxcommand->command, auxcommand->length); 
      #ifdef AUXCOMMAND_RETRY
      /* since aux commands are stateless, optional resend ... */
      serial_write(auxcommand->command, auxcommand->length);
      serial_write(auxcommand->command, auxcommand->length);
      #endif
      msleep(auxcommand->delay);
      serial_purge(); /* flush after delay to discard? */
      /* FIXME need more logic, maybe callbacks? */
      free(auxcommand->command);
      free(auxcommand);
      goto noquerypkt; 
    }

    /* ------- sanity checks and retrieve packet ------------ */

    /* send request and get packet data (from aldlcomm.c); if NULL is
       returned, it's because it timed out waiting for data. */
    if(aldl_get_packet(pkt) == NULL) {
      lock_stats();
      aldl->stats->packetrecvtimeout++;
      unlock_stats();
      pktfail = 1;
      #ifdef VERBLOSITY
      printf("packet %i failed due to timeout...\n",npkt);
      #endif

    /* optional check for pcm address bit in the header, to see if we're
       even in the ballpark of a legit packet.  this may avoid an expensive
       checksumming run if the packet is total garbage. */
    #ifdef CHECK_HEADER_SANITY
    } else if (pkt->data[0] != comm->pcm_address ||
       pkt->data[1] != calc_msglength(pkt->length)) {
      pktfail = 1;
      lock_stats();
      aldl->stats->packetheaderfail++;
      unlock_stats();
      #ifdef VERBLOSITY
      printf("header failed @ pkt %i...\n",npkt);
      #endif
    #endif

    /* verify checksum if that option is enabled in the commdef. */
    } else if(comm->checksum_enable == 1 &&
       checksum_test(pkt->data, pkt->length) == 0) {
      pktfail = 1;
      lock_stats();
      aldl->stats->packetchecksumfail++;
      unlock_stats();
      #ifdef VERBLOSITY
      printf("checksum failed @ pkt %i...\n",npkt);
      #endif
    }

    /* handle condition of a bad packet */
    if(pktfail == 1) {
      lock_stats();
      aldl->stats->failcounter++; /* increment failed pkt counter */
      #ifdef VERBLOSITY
      printf("packet fail counter: %i\n",aldl->stats->failcounter);
      #endif

      /* --- set a desync state if we're getting lots of fails in a row */
      if(aldl->stats->failcounter > aldl->maxfail) {
        set_connstate(ALDL_DESYNC,aldl);
      }
      unlock_stats();

      pktfail = 0; /* reset fail state */
      goto PKTRETRY; /* jump back to earlier in the loop, no increment */

    /* packet is good to go */
    } else {
      #ifdef TRACK_PKTRATE
      pktcounter++; /* increment packet counter */
      #endif
      lock_stats();
      aldl->stats->failcounter = 0; /* reset failcounter */
      unlock_stats();
    }

    /* check if lagtime exceeded, and set lag state. */
    #ifdef LAGCHECK
    if(aldl->comm->shutup_time > 0 &&
       get_elapsed_ms(lagtime) >= aldl->comm->shutup_time) {
      set_connstate(ALDL_LAGGY,aldl);
    }
    #endif

    } /* end packet iterator */

    /* all packets should be complete here */

    /* process the packet */
    process_data(aldl);

    noquerypkt:

    /* set readiness bit */
    if(aldl->ready == 0) {
      if(buffered >= aldl->bufstart) {
        aldl->ready = 1;
      } else {
        buffered++;
      }
    }
  }
  return NULL;
}

