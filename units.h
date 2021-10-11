/*
 *  units, a program for units conversion
 *  Copyright (C) 1996, 1997, 1999, 2000, 2001, 2014, 2017
 *  Free Software Foundation, Inc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *     
 *  This program was written by Adrian Mariano (adrianm@gnu.org)
 */

#include <math.h>
#include <errno.h>

/* Apparently popen and pclose require leading _ under windows */
#if defined(_MSC_VER) || defined(__MINGW32__)
#  define popen _popen
#  define pclose _pclose
#endif


#ifdef NO_ISFINITE
#  if defined _WIN32 && defined _MSC_VER
#    define isfinite(x)  (!_isnan(x) && _finite(x))
#  else
#    define isfinite(x) ( -DBL_MAX <= (x) && (x) <= DBL_MAX )
#  endif
#endif

#ifdef STRINGS_H
#  include <strings.h>
#else
#  include <string.h>
#endif 

#ifndef NO_STDLIB_H
#  include <stdlib.h>
#else
   char *malloc(),  *realloc(),  *getenv();
#endif

#ifndef strchr
#  ifdef NO_STRCHR
#    define strchr(a,b) index((a),(b))
#  else
     char *strchr();
#  endif
#endif /* !strchr */

#define E_NORMAL 0
#define E_PARSE 1
#define E_PRODOVERFLOW 2
#define E_REDUCE 3
#define E_BADSUM 4
#define E_NOTANUMBER 5
#define E_NOTROOT 6
#define E_UNKNOWNUNIT 7
#define E_FUNC 8         /* If errno is set after calling a function */
#define E_BADFUNCTYPE 9
#define E_BADFUNCARG 10
#define E_NOTINDOMAIN 11
#define E_BADFUNCDIMEN 12
#define E_NOINVERSE 13
#define E_PARSEMEM 14
#define E_FUNARGDEF 15
#define E_FILE 16
#define E_BADFILE 17
#define E_MEMORY 18
#define E_BADNUM 19
#define E_UNITEND 20 
#define E_LASTUNSET 21
#define E_IRRATIONAL_EXPONENT 22
#define E_BASE_NOTROOT 23
#define E_DIMEXPONENT 24
#define E_NOTAFUNC 25

extern char *errormsg[];

/* 
   Data type used to store a single unit being operated on. 

   The numerator and denominator arrays contain lists of units
   (strings) which are terminated by a null pointer.  The special
   string NULLUNIT is used to mark blank units that occur in the
   middle of the list.  
*/

extern char *NULLUNIT;

#define MAXSUBUNITS 100         /* Size of internal unit reduction buffer */

struct unittype {
   char *numerator[MAXSUBUNITS];
   char *denominator[MAXSUBUNITS];
   double factor;
};


struct functype {
  char *param;
  char *def;
  char *dimen;
  double *domain_min, *domain_max;
  int domain_min_open, domain_max_open;
};

struct pair {
  double location, value;
};

struct func {
  char *name;
  struct functype forward;
  struct functype inverse;
  struct pair *table;
  int tablelen;
  char *tableunit;
  struct func *next;
  int skip_error_check;    /* do not check for errors when running units -c */
  int linenumber;
  char *file;              /* file where defined */ 
};

struct parseflag {
  int oldstar;      /* Does '*' have higher precedence than '/' */
  int minusminus;   /* Does '-' character give subtraction */
};
extern struct parseflag parserflags;

extern struct unittype *parameter_value;
extern char *function_parameter;

extern int lastunitset;
extern struct unittype lastunit;

void *mymalloc(int bytes, const char *mesg);
int hassubscript(const char *str);
void initializeunit(struct unittype *theunit);
void freeunit(struct unittype *theunit);
void unitcopy(struct unittype *dest,struct unittype *src);
int divunit(struct unittype *left, struct unittype *right);
void invertunit(struct unittype *theunit);
int multunit(struct unittype *left, struct unittype *right);
int expunit(struct unittype *theunit, int  power);
int addunit(struct unittype *unita, struct unittype *unitb);
int rootunit(struct unittype *inunit,int n);
int unitpower(struct unittype *base, struct unittype *exponent);
char *dupstr(const char *str);
char *dupnstr(const char *string, int length);
int unit2num(struct unittype *input);
struct func *fnlookup(const char *str);
int evalfunc(struct unittype *theunit, struct func *infunc, int inverse, 
             int allerror);
int parseunit(struct unittype *output, const char *input, char **errstr,
              int *errloc);
char* unitsHandler(int argc, char **argv);

