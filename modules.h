/* the ncurses based console interface */
void *consoleif_init(void *aldl_in);
void consoleif_exit();

/* the standard full-time datalogger */
void *datalogger_init(void *aldl_in);

/* the 'remote' scripting interface */
void *remote_init(void *aldl_in);

/* lt1 tuning special module */
void *mode4_init(void *aldl_in);
void mode4_exit();
