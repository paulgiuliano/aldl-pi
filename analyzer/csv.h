/* return a pointer to the start of the numbered field, or null if you run
   out of fields */
char *field_start(char *in, int f);

/* return a pointer to the start of the numbered line, or null if you run
   out of lines */
char *line_start(char *in, int ln);

/* return an array index for the end of the specified field */
int field_end(char *in);

/* get int/float/whatever */
int csv_get_int(char *start);
float csv_get_float(char *start);
char *csv_get_string(char *start);

