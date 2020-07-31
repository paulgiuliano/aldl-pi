#define VERSION "ALDL-IO 1.6a"

/************ SCOPE *********************************
  Static #define's that apply to the entire program.
****************************************************/

/* ----------- NOTES ----------------------------------*/

/* An effort has been made to keep nearly all static configuration in this
   file, however there are some exceptions, see useful.h for some other
   constants and defines... */

/* ----------- FILE CONFIG ----------------------------*/

/* path to the root config file */
/* TODO need to make an override on command line option in main.c */
#define ROOT_CONFIG_FILE "/etc/aldl/aldl.conf"

/* ----------- DEBUG OUTPUT --------------------------*/

/* the debug master switch, enables all available debugging and verbosity
   routines.  or, undef this and set individual options below... */
#undef DEBUGMASTER

/* enable some checks for retarded values being passed to things */
#undef RETARDED

/* generally verbose behavior */
#undef VERBLOSITY

/* debug structural functions, such as record link list management */
#undef DEBUGSTRUCT

/* print debugging info for memory */
#undef DEBUGMEM

/* verbosity levels in raw serial handlers */
#undef SERIAL_VERBOSE
#undef SERIAL_SUPERVERBOSE

/* all errors are fatal error conditions */
#undef ALL_ERRORS_FATAL

/* verbose aldl protocol level comms on stdout */
#undef ALDL_VERBOSE

/* verbose networking */
#define NET_VERBOSE

#ifdef DEBUGMASTER
  #define NET_VERBOSE
  #define ALDL_VERBOSE
  #define SERIAL_VERBOSE
  #define SERIAL_SUPERVERBOSE
  #define DEBUGMEM
  #define ALL_ERRORS_FATAL
  #define RETARDED
  #define VERBLOSITY
  #define DEBUGSTRUCT
#endif

/* --------- GLOBAL FEATURE CONFIG -----------------*/

/* maximum size of listen/skip buffer.   if a listen or skip attempt is larger
   than this value, an emergency realloc is done, which is a waste of time, but
   fairly safe.  set this larger than any possible listen/skip req. to trade
   memory for cpu time. */
#define ALDL_COMMBUFFER 2048

/* --------- TIMING CONSTANTS ------------------------*/

/* define for more aggressive timing behavior in an attempt to increase packet
   rate, at a higher risk of dropped packets and increased cpu usage */
#undef AGGRESSIVE

/* a static delay in microseconds.  used for waiting in between grabbing
   serial chunks, and other throttling.  if AGGRESSIVE is defined, this is
   generally ignored ... */
#define SLEEPYTIME 200

/* a theoretical maximum multiplier per byte that the ECM may take to generate
   data under any circumstance ... */
#define ECMLAGTIME 0.35

/* a constant theoretical amount of bytes per millisecond that can be
   moved at the baud rate; generally 1 / baud * 1000 */
#define SERIAL_BYTES_PER_MS 0.98

/* defining this provides a linear decrease in the frequency of reconnect
   attempts.  this is for 'always-on' dashboard systems that might just sit
   there for hours at a time with no connection available.  this only works
   with 'chatterwait' mode enabled ... */
#define NICE_RECONNECT

/* max delay in milliseconds */
#define NICE_RECON_MAXDELAY 1000

/* when waiting for idle chatter, give up after this many iterations.  this is
   times 10 milliseconds of wait time if NICE_RECONNECT isn't set, and becomes
   variable if it is.  undef this to wait forever.  this has no effect if the
   wait for idle chatter routine is disabled. */
#define GIVEUPWAITING 1500

/* this is added to actual message length, incl. header and checksum, to
   determine packet length byte (byte 2 of most aldl messages).  so far, no
   known ecms use a constant other than 0x52 */
#define MSGLENGTH_MAGICNUMBER 0x52

/* --------- DATA ACQ. CONFIG ----------------------*/

/* increase the priority of the main acq thread so plugins dont screw with
   it.  may reduce tendancy for timing errors on some platforms, or may not
   be supported on your platform at all.  undefine if you want equal
   thread priority. */
#define ACQ_PRIORITY 1

/* track packet retrieval rate */
#define TRACK_PKTRATE

/* number of seconds to average retrieval rate.  reccommend at least 5. */
#define PKTRATE_DURATION 5

/* extra check for bad message header.  checksum should be sufficient.. */
#define CHECK_HEADER_SANITY

/* track connection state for expiry of disable comms mode */
#define LAGCHECK

/* attempt to handle cases where runtime may exceed ULONG_MAX by wrapping back
   to zero.  this is at least 49 days of runtime or more depending on host
   system; so the check should not be necessary ... and results are undefined */
#define TIMESTAMP_WRAPAROUND

/* when sending auxiliary commands such as EE MODE4 stuff, send the message
   three times instead of one to help ensure success.  there's no real way to
   tell if the command succeeded, so this is probably a good idea?
   if commands are cumulative this is obviously broken, though. */
#define AUXCOMMAND_RETRY

/* ------- FTDI DRIVER CONFIG ------------------------*/

/* the baud rate to set for the ftdi usb userland driver.  reccommend 8192. */
#define FTDI_BAUD 8192

/* the maximum number of ftdi devices attached to a system, it'll puke if more
   than this number of devices is found ... */
#define FTDI_AUTO_MAXDEVS 100

/* if the device isn't connected, try again.  this is mostly for systems with
   no keyboard or input device, so you can go 'oh shit, it's unpluged' without
   rebooting. */
#define FTDI_RETRY_USB
#define FTDI_RETRY_DELAY 3

/* if this many io operations fail, consider the interface failed and attempt
   reconnection. */
#define FTDI_ATTEMPT_RECOVERY
#define FTDI_MAXFAIL 3

/* ------- DUMMY DRIVER CONFIG ----------------------*/

/* simulate random corruption in dummy packets */
#define DUMMY_CORRUPTION_ENABLE

/* 0-100, rough percentage of corrupted packets */
#define DUMMY_CORRPUTION_RATE 3

/* 0-100, strength of corruption */
#define DUMMY_CORRUPTION_AMOUNT 3

/* ------- MISC CONSTANTS ---------------------------*/

/* bad chars that can't be used in things such as unit of measure strings or
   unique identifier names. */
#define CONFIG_BAD_CHARS "(),\"'"


/* ------- DISPLAY CONFIG ---------------------------*/

/* use units of measure in consoleif */
#undef CONSOLEIF_UOM

/* the char used for ncurses progress bar stuff */
#define CONSOLEIF_BAR_CHAR '*'

/* print hex M4 string */
#define M4_PRINT_HEX
