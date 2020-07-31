#ifndef _USEFUL_H
#define _USEFUL_H

/* --- STRING MANIPULATION ------------- */

/* quickly check if string a is identical to string b.  return 1 if identical,
   return 0 if different.  this is faster than strcmp in most cases because
   when a difference is found, it bails immediately. */
int rf_strcmp(char *a, char *b);

/* quickly see if any of the chars in *filter exist in *str.  returns 0 or the
   first char found.  useful for quickly discarding strings w/ illegal chars */
char rf_listcmp(char *str, char *filter);

/* replace instances of any char from *filter in *str with repl, useful for
   filtering 'bad' chars from a string.  return number of chars replaced. */
int rf_chfilter(char *str, char *filter, char repl);

/* --- MATH ---------------------------- */

/* clamp an int or float */
int rf_clamp_int(int min, int max, int in);
float rf_clamp_float(float min, float max, float in);

/* --- UNIX FILE I/O ------------------- */

/* return a pointer to the entire contents of a file loaded into memory. if
   for some reason the file can't be loaded, return NULL */
char *rf_loadfile(char *filename);

/* END INCLUDE GUARD */

#endif
