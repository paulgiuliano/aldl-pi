#define MAX_PARAMETERS 65535

/* a struture with array index matched parameters and values from a cfg file */
typedef struct _dfile_t {
  unsigned int n; /* number of parameters */
  char **p;  /* parameter */
  char **v;  /* value */
} dfile_t;

/* loads file, strips quotes, shrinks, parses in one step.. */
dfile_t *dfile_load(char *filename);

/* split up data into parameters and values */
dfile_t *dfile(char *data);

/* reduce the data section and pointer arrays to reduce mem usage, returns
   pointer to new data to be freed later. */
char *dfile_shrink(dfile_t *d);

/* get a value by parameter string */
char *value_by_parameter(char *str, dfile_t *d);

/* strip all starting and ending quotes from quoted 'value' fields. */
void dfile_strip_quotes(dfile_t *d);

/* for debugging, print a list of all config pairs extracted. */
void print_config(dfile_t *d);

/* get various config options by name */
int configopt_int_fatal(dfile_t *config, char *str, int min, int max);
int configopt_int(dfile_t *config, char *str, int min, int max, int def);
float configopt_float(dfile_t *config, char *str, float def);
float configopt_float_fatal(dfile_t *config, char *str);
char *configopt_fatal(dfile_t *config, char *str);
char *configopt(dfile_t *config, char *str,char *def);

