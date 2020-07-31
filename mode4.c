#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>
#include <string.h>

#include "error.h"
#include "aldl-io.h"
#include "config.h"
#include "loadconfig.h"
#include "useful.h"

enum {
  RED_ON_BLACK = 1,
  BLACK_ON_RED = 2,
  GREEN_ON_BLACK = 3,
  CYAN_ON_BLACK = 4,
  WHITE_ON_BLACK = 5,
  WHITE_ON_RED = 6
};

/* this is the current global status of the mode 4 command.  yes this is
   replication from the actual mode 4 command bytes, but it's easier to read. */
typedef struct {
  byte afr;
  int sparkabs;
  int sparkdelta;
  int rpm;
  int cyl;
} m4_status_t;
m4_status_t m4_status;

/* represention of the current engine status from the datastream */
typedef struct {
  float rpm;
  float idletarget;
  int iacsteps;
  float cooltemp;
  float map;
  float kr;
  int adv;
} engine_status_t;
engine_status_t engine_status;

/* cached index numbers of engine datastream elements */
typedef struct {
  int rpm;
  int idletarget;
  int iacsteps;
  int cooltemp;
  int map;
  int adv;
  int kr;
} p_index_t;
p_index_t p_idx;

/* the color of a status message */
#define COLOR_STATUSSCREEN RED_ON_BLACK

/* "usage" string */
#define M4_USE_STRING "SPK[q/w] RESET[SPC] RECORD[ENTER] IDLE[a/s] CUT[0-8]"

/* minimum and maximum spark delta and total (should be seperate) */
#define MODE4MINSPK -50
#define MODE4MAXSPK 50

/* --- variables ---------------------------- */

int w_height, w_width; /* width and height of window */

aldl_conf_t *aldl; /* global pointer to aldl conf struct */
aldl_record_t *rec; /* current record */

byte mfb[16]; /* mode four string buffer */
int m4_commrev; /* set this bit if m4 comm string was revised */

char *msgbuf; /* a big message buffer used for single log entry */
#define MSGBUFSIZE 2048

/* --- local functions ------------------------*/

void get_engine_status(); /* pull engine data from rec to engine_data struct */

/* center half-width of an element on the screen */
int m4_xcenter(int width);
int m4_ycenter(int height);
 
/* print a centered string */
void m4_statusmessage(char *str);

/* print engine status to msgbuf returns ptr to msgbuf */
char *print_engine_status();

/* clear screen and display waiting for connection messages */
void m4_cons_wait_for_connection();

/* handle ncurses input */
void m4_consoleif_handle_input();

#ifdef M4_PRINT_HEX
/* write mode 4 command to screen */
void m4_draw_cmd(int xpos, int ypos);
#endif

/* record current snapshot to disk */
void m4_record_status();

/* m4 comm */
void m4_init_status(); /* initialize the 'status' structure */
void m4_comm_init(); /* init the command array */
void m4_comm_cancel(); /* fast abort, cancel all command bytes and send */
void m4_comm_submit(); /* submit command, includes checksum generation */

void m4_comm_spark(int advance, int absolute); /* set spark, absolute=0 delta */
void m4_comm_afr(byte afr); /* set air fuel ratio, 0=disable */
void m4_comm_idle(int rpm, int mode); /* mode1=rpm mode0=steps rpm=0=disable */
void m4_drop_cyl(int cyl); /* drop a cylinder or 0 to enable all, overr.afr */

/* --------------------------------------------*/

void *mode4_init(void *aldl_in) {
  aldl = (aldl_conf_t *)aldl_in;

  /* sanity check pcm address to avoid using this with wrong ecm */
  if(aldl->comm->pcm_address != 0xF4) {
    error(EFATAL, ERROR_PLUGIN, "MODE4 Special plugin is for EE only.");
  };

  /* allocate main message buffer for log entries */
  msgbuf = malloc(sizeof(char) * MSGBUFSIZE);

  /* initialize root window */
  WINDOW *root;
  if((root = initscr()) == NULL) {
    error(1,ERROR_NULL,"could not init ncurses");
  }

  curs_set(0); /* remove cursor */
  cbreak(); /* dont req. line break for input */
  nodelay(root,true); /* non-blocking input */
  noecho();

  /* configure colors (even though this doesn't use much color) */
  start_color();
  init_pair(RED_ON_BLACK,COLOR_RED,COLOR_BLACK);
  init_pair(BLACK_ON_RED,COLOR_BLACK,COLOR_RED);
  init_pair(GREEN_ON_BLACK,COLOR_GREEN,COLOR_BLACK);
  init_pair(CYAN_ON_BLACK,COLOR_CYAN,COLOR_BLACK);
  init_pair(WHITE_ON_BLACK,COLOR_WHITE,COLOR_BLACK);
  init_pair(WHITE_ON_RED,COLOR_WHITE,COLOR_RED);

  /* fetch indexes (saves repeated lookups) */
  p_idx.rpm = get_index_by_name(aldl,"RPM");
  p_idx.idletarget = get_index_by_name(aldl,"IDLESPD");
  p_idx.iacsteps = get_index_by_name(aldl,"IAC");
  p_idx.cooltemp = get_index_by_name(aldl,"COOLTMP");
  p_idx.map = get_index_by_name(aldl,"MAP");
  p_idx.adv = get_index_by_name(aldl,"ADV");
  p_idx.kr = get_index_by_name(aldl,"KR");

  /* get initial screen size */
  getmaxyx(stdscr,w_height,w_width);

  /* wait for connection */
  m4_cons_wait_for_connection();

  /* prepare 'empty' mode4 command */
  m4_comm_init();

  while(1) {

    /* get newest record */
    rec = newest_record_wait(aldl,rec);
    if(rec == NULL) { /* disconnected */
      m4_cons_wait_for_connection();
      continue;
    }

    /* process engine status */
    get_engine_status();

    erase(); /* clear screen --------- */

    mvprintw(0,1,M4_USE_STRING); /* print usage */

    /* print eng status */
    mvprintw(2,1,"%s",print_engine_status());

    m4_commrev = 0; /* reset revision bit.  input handler may set it */
    m4_consoleif_handle_input(); /* get keyboard input and branch */
 
    #ifdef M4_PRINT_HEX
    /* print the m4 string in hex for debug */
    m4_draw_cmd(1,1);
    #endif

    if(m4_commrev == 1) { /* send command if revised */
      m4_comm_submit();
    }

    refresh();
    usleep(500);
  }

  sleep(4);
  delwin(root);
  endwin();
  refresh();

  pthread_exit(NULL);
  return NULL;
}

void get_engine_status() {
  engine_status.rpm = rec->data[p_idx.rpm].f;
  engine_status.idletarget = rec->data[p_idx.idletarget].f;
  engine_status.iacsteps = rec->data[p_idx.iacsteps].i;
  engine_status.cooltemp = rec->data[p_idx.cooltemp].f;
  engine_status.map = rec->data[p_idx.map].f;
  engine_status.adv = rec->data[p_idx.adv].i;
  engine_status.kr = rec->data[p_idx.kr].f;
}

char *print_engine_status() {
  char *c = msgbuf;

  /* engine status */
  c += sprintf(c,"---------------------------------\n");
  c += sprintf(c,"Engine Speed: %.0f RPM\n",engine_status.rpm);
  c += sprintf(c,"Manifold Pressure: %.0f KPA\n",engine_status.map);
  c += sprintf(c,"Spark Advance: %i Deg\n",engine_status.adv);
  c += sprintf(c,"Knock Ret: %.1f\n",engine_status.kr);
  c += sprintf(c,"Idle Target: %.0f RPM (%i Steps)\n",
               engine_status.idletarget,engine_status.iacsteps);
  c += sprintf(c,"Engine Temp: %.0f c\n",engine_status.cooltemp);
  c += sprintf(c,"\n");

  /* modifier status */
  if(m4_status.afr != 0) {
    c += sprintf(c,"Forced AFR: %.1f:1\n",((float)m4_status.afr/10));
  }
  if(m4_status.sparkabs != 0) {
    c += sprintf(c,"Absolute Spark: %i DEG\n",m4_status.sparkabs);
  }
  if(m4_status.sparkdelta > 0) {
    c += sprintf(c,"Added Spark: +%i DEG\n",m4_status.sparkdelta);
  }
  if(m4_status.sparkdelta < 0) {
    c += sprintf(c,"Subtracted Spark: %i DEG\n",m4_status.sparkdelta);
  } 
  if(m4_status.rpm != 0) {
    c += sprintf(c,"Force Idle: %i RPM\n",m4_status.rpm);
  }
  if(m4_status.cyl > 0) {
    c += sprintf(c,"Disabled Cylinder: %i\n",m4_status.cyl);
  }
  return msgbuf;
}

int m4_xcenter(int width) {
  return ( w_width / 2 ) - ( width / 2 );
}

int m4_ycenter(int height) {
  return ( w_height / 2 ) - ( height / 2 );
}

void m4_statusmessage(char *str) {
  clear();
  attron(COLOR_PAIR(COLOR_STATUSSCREEN));
  mvaddstr(m4_ycenter(0),m4_xcenter(strlen(str)),str);
  mvaddstr(1,1,VERSION);
  attroff(COLOR_PAIR(COLOR_STATUSSCREEN));
  refresh();
  usleep(5000);
}

void m4_cons_wait_for_connection() {
  aldl_state_t s = ALDL_LOADING;
  aldl_state_t s_cache = ALDL_CONNECTED; /* cache to avoid redraws */
  while(s > 10) { /* messages >10 are non-connected */
    s = get_connstate(aldl);
    if(s != s_cache) { /* status message has changed */
      m4_statusmessage(get_state_string(s)); /* disp. msg */
      s_cache = s; /* reset cache */
    } else {
      usleep(10000); /* checking conn state too fast is bad */
    }
  }

  m4_statusmessage("Buffering...");
  pause_until_buffered(aldl);

  clear();
}

#ifdef M4_PRINT_HEX
void m4_draw_cmd(int xpos, int ypos) {
  move(ypos,xpos); 
  int x;
  for(x=0;x<16;x++) printw("%X ",(unsigned int)mfb[x]);
}
#endif

void m4_mode4_exit() {
  endwin();
}

void m4_consoleif_handle_input() {
  int c;
  while((c = getch()) != ERR) {
    switch(c) {
      case 'q': /* timing down */
        m4_status.sparkdelta--;
        m4_comm_spark(m4_status.sparkdelta,0);
        m4_commrev = 1;
      break;
      case 'w': /* timing up */
        m4_status.sparkdelta++;
        m4_comm_spark(m4_status.sparkdelta,0);
        m4_commrev = 1;
      break;
      case 'a': /* idle down */
        if(m4_status.rpm == 0) { /* init from current idle */
           m4_status.rpm = engine_status.idletarget;
        };
        m4_comm_idle(m4_status.rpm - 25,1);
        m4_commrev = 1;
      break;
      case 's': /* idle up */
        if(m4_status.rpm == 0) { /* init from current idle */
           m4_status.rpm = engine_status.idletarget;
        };
        m4_comm_idle(m4_status.rpm + 25,1);
        m4_commrev = 1;
      break;
      case '0': /* cyl cut disable */
      case '1': /* cyl cut 1, etc */
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
        m4_drop_cyl(c - '0');
        m4_commrev = 1;
      break;
      case ' ': /* reset */
        m4_comm_init();
        m4_commrev = 1;
      break;
      case '\n': /* snapshot */
        m4_record_status();
      break;
    };
  }
}

void m4_comm_spark(int advance, int absolute) {
  if(advance == 0 || MODE4MAXSPK > 60) { /* no advance, disable */
    clrbit(mfb[13],0); /* disable spark control */
    clrbit(mfb[13],2); /* clear mode (redundant ...) */
    clrbit(mfb[13],1); /* clear advret (redundant ...) */
    mfb[14] = 0; /* clear adv */
    m4_status.sparkabs = 0;
    m4_status.sparkdelta = 0;

  } else { /* enable advance */
    setbit(mfb[13],0); /* set spark control enable bit */

    /* WARNING absolute mode is kinda fucking dangerous, dont use it */
    if(absolute == 1) { /* configure for absolute spark */
      clrbit(mfb[13],2); /* abs mode */
      mfb[14] = rf_clamp_int(MODE4MINSPK,MODE4MAXSPK,advance); /* set advance */
      m4_status.sparkabs = advance;
      m4_status.sparkdelta = 0;
      /* don't bother with retard/advance, abs negative spark is useless */

    } else { /* configure for delta spark */
      setbit(mfb[13],2); /* delta mode */
      if(advance > 0) {
        clrbit(mfb[13],1); /* advance */
        mfb[14] = rf_clamp_int(MODE4MINSPK,MODE4MAXSPK,advance);
      } else {
        setbit(mfb[13],1); /* retard */
        mfb[14] = rf_clamp_int(MODE4MINSPK,MODE4MAXSPK,( 1 - advance ));
      }
      m4_status.sparkabs = 0;
      m4_status.sparkdelta = advance;
    }
  }
}

void m4_comm_idle(int rpm, int mode) {
  if(rpm == 0) { /* disable idle command */
    clrbit(mfb[9],4); /* clear iac enable */
    clrbit(mfb[9],5); /* clear steps/rpm */
    m4_status.rpm = 0;
  } else if(mode == 1) { /* RPM mode */
    setbit(mfb[9],4); /* set iac enable */
    setbit(mfb[9],5); /* set rpm mode */
    m4_status.rpm = rf_clamp_int(0,3000,rpm); /* clamp and set stat */ 
    mfb[11] = m4_status.rpm * 0.08; /* set rpm */
  } else { /* steps mode */
    setbit(mfb[9],4); /* set iac enable */
    clrbit(mfb[9],5); /* set rpm mode */
    m4_status.rpm = rf_clamp_int(0,255,rpm); /* clamp and set stat */
    mfb[11] = rpm;
  }
}

void m4_drop_cyl(int cyl) {
  if(cyl == 0) { /* cyl 0 = disable */
    clrbit(mfb[9],7); /* disable cut */
    mfb[12] = 0x00; /* disable cyl */
    m4_status.cyl = 0;
  } else {
    m4_comm_afr(0); /* disable afr (conflict) */ 
    mfb[12] = rf_clamp_int(1,8,cyl);
    setbit(mfb[9],7); /* enable cut */
    m4_status.cyl = mfb[12];
  }
}

void m4_comm_afr(byte afr) {
  if(afr > 0) {
    setbit(mfb[9],6);
    mfb[12] = afr;
    m4_status.afr = afr;
  } else {
    clrbit(mfb[9],6);
    mfb[12] = 0x00;
    m4_status.afr = 0;
  }
}

void m4_comm_init() {
  /* zero entire string */
  int x;
  for(x=3;x<16;x++) mfb[x] = 0x00;
  /* add prefix */
  mfb[0] = 0xF4; /* ecm address */
  mfb[1] = 0x62; /* message length */
  mfb[2] = 0x04; /* mode byte */
  m4_init_status(); /* reset status to match */
}

void m4_comm_cancel() {
  m4_comm_init(); /* blank out the command */
  m4_comm_submit(); /* re-send */
}

void m4_comm_submit() {
  mfb[15] = checksum_generate(mfb,15); /* gen checksum @ last byte */
  aldl_add_command(mfb, 16, 16); /* queue command, the acq thread sends it */
}

void m4_init_status() {
  m4_status.afr = 0;
  m4_status.sparkabs = 0;
  m4_status.sparkdelta = 0;
  m4_status.rpm = 0;
  m4_status.cyl = 0;
}

void m4_record_status() {
  /* open file */
  FILE *fdesc = fopen("/var/log/aldl/MODE4_LOG","a");
  if(fdesc == NULL) error(1,ERROR_PLUGIN,"cant open mode4 snapfile !");

  /* write engine status */
  fwrite(print_engine_status(),strlen(msgbuf),1,fdesc);
  
  /* close file */
  fclose(fdesc);
}

