#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* local objects */
#include "error.h"
#include "aldl-io.h"
#include "loadconfig.h"
#include "useful.h"

void *remote_init(void *aldl_in) {
  aldl_conf_t *aldl = aldl_in;
  int ran_connected_script = 0;
  while(1) {

    /* loop throttling */
    sleep(1);

    /* exit entire program if this file is present ... */
    if(access("/etc/aldl/aldl-stop",F_OK) != -1) {
      exit(1);
    }

    /* if /etc/aldl/aldl-connected.sh exists, run it when buffered... */
    if(aldl->ready == 1 && ran_connected_script == 0) {
      if(access("/etc/aldl/aldl-connected.sh",X_OK) != -1) {
        system("/etc/aldl/aldl-connected.sh");
      }
      ran_connected_script = 1;
    } 

    /* if connection drops after being connected, run aldl-disconnected.sh */
    if(ran_connected_script == 1 && get_connstate(aldl) > 10) {
      if(access("/etc/aldl/aldl-disconnected.sh",X_OK) != -1) {
        system("/etc/aldl/aldl-disconnected.sh");
      }
      ran_connected_script = 0;
    }
  }
  return NULL;
}

