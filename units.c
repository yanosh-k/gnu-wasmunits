#define VERSION "2.21"
/*
 *  units, a program for units conversion
 *  Copyright (C) 1996, 1997, 1999, 2000-2007, 2009, 2011-2020
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA 02110-1301 USA
 *     
 *  This program was written by Adrian Mariano (adrianm@gnu.org)
 */

#define LICENSE "\
Copyright (C) 2020 Free Software Foundation, Inc.\n\
GNU Units comes with ABSOLUTELY NO WARRANTY.\n\
You may redistribute copies of GNU Units\n\
under the terms of the GNU General Public License."

#define _XOPEN_SOURCE 600

#if defined (_WIN32) && defined (_MSC_VER)
# include <windows.h>
# include <winbase.h>
#endif
#if defined (_WIN32) && defined (HAVE_MKS_TOOLKIT)
# include <sys/types.h>
#endif
# include <sys/stat.h>

#include <ctype.h>
#include <float.h>

#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>

#if defined (_WIN32) && defined (_MSC_VER)
# include <io.h>
# define fileno _fileno
# define isatty _isatty
# define stat   _stat
#endif

#ifdef HAVE_IOCTL
#  include <sys/ioctl.h>
#  include <fcntl.h>
#endif

#ifndef NO_SETLOCALE
#  include<locale.h>
#endif

#ifdef SUPPORT_UTF8
/* Apparently this define is needed to get wcswidth() prototype */
#  include <wchar.h>
#  include <langinfo.h>
#  define UTF8VERSTR "with utf8"
#else
#  define UTF8VERSTR "without utf8"
#endif

#ifdef READLINE
#  define RVERSTR "with readline"
#  include <readline/readline.h>
#  include <readline/history.h>
#  define HISTORY_FILE ".units_history"
#else
#  define RVERSTR "without readline"
#endif

#include "getopt.h"
#include "units.h"

#ifndef UNITSFILE
#  define UNITSFILE "definitions.units"
#endif

#ifndef LOCALEMAP
#  define LOCALEMAP "locale_map.txt"
#endif

#ifndef DATADIR
#  ifdef _WIN32
#    define DATADIR   "..\\share\\units"
#  else
#    define DATADIR   "../share/units"
#  endif
#endif

#if defined (_WIN32) && defined (_MSC_VER)
#  include <direct.h>
#  define getcwd _getcwd
#else
#  include <unistd.h>
#endif

#ifdef _WIN32
#  define EXE_EXT ".exe"
#  define PATHSEP ';'
#  define DIRSEP '\\'
#  define DIRSEPSTR "\\"        /* for building pathnames */
#else
#  define EXE_EXT ""
#  define PATHSEP ':'
#  define DIRSEP '/'
#  define DIRSEPSTR "/"         /* for building pathnames */
#endif

#define PRIMITIVECHAR '!'       /* Character that marks irreducible units */
#define COMMENTCHAR '#'         /* Comments marked by this character */
#define COMMANDCHAR '!'         /* Unit database commands marked with this */
#define UNITSEPCHAR ';'         /* Separator for unit lists.  Include this */
                                /*    char in rl_basic_word_break_characters */
                                /*    and in nonunitchars defined in parse.y */
#define FUNCSEPCHAR ';'         /* Separates forward and inverse definitions */
#define REDEFCHAR '+'           /* Mark unit as redefinition to suppress warning message */
#define DEFAULTPAGER "more"     /* Default pager program */
#define DEFAULTLOCALE "en_US"   /* Default locale */
#define MAXINCLUDE 5            /* Max depth of include files */
#define MAXFILES 25             /* Max number of units files on command line */
#define NODIM "!dimensionless"  /* Marks dimensionless primitive units, such */
                                /*    as the radian, which are ignored when */
                                /*    doing unit comparisons */
#define NOPOINT -1              /* suppress display of pointer in processunit*/
#define NOERRMSG -2             /* no error messages in checkunitlist() */
#define ERRMSG -3
#define SHOWFILES -4

#define MAXHISTORYFILE 5000     /* max length of history file for readline */

/* values for output number format */
#define BASE_FORMATS "gGeEf"    /* printf() format types recognized pre-C99 */
#define DEFAULTPRECISION 8      /* default significant digits for printf() */
#define DEFAULTTYPE 'g'         /* default number format type for printf() */
#define MAXPRECISION DBL_DIG    /* maximum number precision for printf() */

#define HOME_UNITS_ENV "MYUNITSFILE"  /* Personal units file environment var */

#define NOERROR_KEYWORD "noerror "   /* The trailing space is important */
#define CO_NOARG -1

#define HELPCOMMAND "help"      /* Command to request help at prompt */
#define SEARCHCOMMAND "search"  /* Command to request text search of units */
#define UNITMATCH "?"           /* Command to request conformable units */
char *exit_commands[]={"quit","exit",0};
char *all_commands[]={"quit","exit",HELPCOMMAND,SEARCHCOMMAND,UNITMATCH,0};

/* Key words for function definitions */
struct {
  char *word;
  char delimit;
  int checkopen;  /* allow open intervals with parentheses */
} fnkeywords[]={ {"units=",  FUNCSEPCHAR, 0}, 
                 {"domain=", ',', 1}, 
                 {"range=", ',',1}, 
                 {NOERROR_KEYWORD, ' ',CO_NOARG}, 
                 {0,0}};
#define FN_UNITS 0
#define FN_DOMAIN 1
#define FN_RANGE 2
#define FN_NOERROR 3

char *builtins[] = {"sin", "cos", "tan","ln", "log", "exp", 
                    "acos", "atan", "asin", "sqrt", "cuberoot", "per",
                    "sinh", "cosh", "tanh", "asinh", "atanh", "acosh", 0};

struct {
  char *format;         /* printf() format specification for numeric output */
  int width;            /* printf() width from format */
  int precision;        /* printf() precision from format */
  char type;            /* printf() type from format */
} num_format;


struct {    /* Program command line option flags */
 int
   interactive,
   unitlists,     /* Perform unit list output if set */
   oneline,       /* Suppresses the second line of output */
   quiet,         /* Supress prompting (-q option) */
   round,         /* Round the last of unit list output to nearest integer */
   showconformable, /*   */
   showfactor,    /*   */
   strictconvert, /* Strict conversion (disables reciprocals) */
   unitcheck,     /* Enable unit checking: 1 for regular check, 2 for verbose*/
   verbose,       /* Flag for output verbosity */
   readline;      /* Using readline library? */
} flags;


#define UTF8MARKER "\xEF\xBB\xBF"   /* UTF-8 marker inserted by some Windows */
                                    /* programs at start of a UTF-8 file */

struct parseflag parserflags;   /* parser options */


char *homeunitsfile = ".units"; /* Units filename in home directory */
char *homedir = NULL;           /* User's home direcotry */
char *homedir_error = NULL;     /* Error message for home directory search */
char *pager;                    /* Actual pager (from PAGER environment var) */
char *mylocale;                 /* Locale in effect (from LC_CTYPE or LANG) */
int utf8mode;                   /* Activate UTF8 support */
char *powerstring = "^";        /* Exponent character used in output */
char *unitsfiles[MAXFILES+1];   /* Null terminated list of units file names */
char *logfilename=NULL;         /* Filename for logging */
FILE *logfile=NULL;             /* File for logging */
char *promptprefix=NULL;        /* Prefix added to prompt */
char *progname;                 /* Used in error messages */
char *fullprogname;             /* Full path of program; printversion() uses */
char *progdir;                  /* Used to find supporting files */
char *datadir;                  /* Used to find supporting files */
char *deftext="        Definition: ";/* Output text when printing definition */
char *digits = "0123456789.,";


#define QUERYHAVE  "You have: "  /* Prompt text for units to convert from */
#define QUERYWANT  "You want: "  /* Prompt text for units to convert to */

#define LOGFROM    "From: "     /* tag for log file */
#define LOGTO      "To:   "     /* tag for log file */


#define  HASHSIZE 101           /* Values from K&R 2nd ed., Sect. 6.6 */
#define  HASHNUMBER 31

#define  SIMPLEHASHSIZE 128
#define  simplehash(str) (*(str) & 127)    /* "hash" value for prefixes */

char *errormsg[]={"Successful completion", 
                  "Parse error",           
                  "Product overflow",      
                  "Unit reduction error (bad unit definition)",
                  "Invalid sum or difference of non-conformable units",
                  "Unit not dimensionless",
                  "Unit not a root",
                  "Unknown unit",
                  "Bad argument",
                  "Weird nonlinear unit type (bug in program)",
                  "Function argument has wrong dimension",
                  "Argument of function outside domain",
                  "Nonlinear unit definition has unit error",
                  "No inverse defined",
                  "Parser memory overflow (recursive function definition?)",
                  "Argument wrong dimension or bad nonlinear unit definition",
                  "Cannot open units file",
                  "Units file contains errors",
                  "Memory allocation error",
                  "Malformed number",
                  "Unit name ends with a digit other than 0 or 1 without preceding '_'",
                  "No previous result; '_' not set",
                  "Base unit not dimensionless; rational exponent required",
                  "Base unit not a root",
                  "Exponent not dimensionless",
                  "Unknown function name"
                  };

char *invalid_utf8 = "invalid/nonprinting UTF-8";

char *irreducible=0;            /* Name of last irreducible unit */


/* Hash table for unit definitions. */

struct unitlist {
   char *name;                  /* unit name */
   char *value;                 /* unit value */
   int linenumber;              /* line in units data file where defined */
   char *file;                  /* file where defined */ 
   struct unitlist *next;       /* next item in list */
} *utab[HASHSIZE];

char hasLoadedUnits = 0;

/* Table for prefix definitions. */

struct prefixlist {
   int len;                     /* length of name string */
   char *name;                  /* prefix name */
   char *value;                 /* prefix value */
   int linenumber;              /* line in units data file where defined */
   char *file;                  /* file where defined */ 
   struct prefixlist *next;     /* next item in list */
} *ptab[SIMPLEHASHSIZE];


struct wantalias {
  char *name;
  char *definition;
  struct wantalias *next;
  int linenumber;
  char *file;
};

struct wantalias *firstalias = 0;
struct wantalias **aliaslistend = &firstalias; /* Next list entry goes here */

/* Table for function definitions */

struct func *ftab[SIMPLEHASHSIZE];

/* 
   Used for passing parameters to the parser when we are in the process
   of parsing a unit function.  If function_parameter is non-nil, then 
   whenever the text in function_parameter appears in a unit expression
   it is replaced by the unit value stored in parameter_value.
*/

char *function_parameter = 0; 
struct unittype *parameter_value = 0;

/* Stores the last result value for replacement with '_' */

int lastunitset = 0;
struct unittype lastunit;

char *NULLUNIT = "";  /* Used for units that are canceled during reduction */

#define startswith(string, prefix) (!strncmp(string, prefix, strlen(prefix)))
#define lastchar(string) (*((string)+strlen(string)-1))
#define emptystr(string) (*(string)==0)
#define nonempty(list)  ((list) && *(list))


#ifdef READLINE

char *historyfile;           /* Filename for readline history */
int init_history_length;      /* Length of history read from the history file*/
int init_history_base;

void
save_history(void)
{
  int newentries;
  int err;

  newentries = history_length-init_history_length;
  if (history_max_entries > 0){
    newentries += history_base - init_history_base;
    if (newentries > history_max_entries)
      newentries = history_max_entries;
  }
  
  err = append_history(newentries,historyfile);
  if (err){
    if (err == ENOENT)
      err = write_history(historyfile);
    if (err) {
      printf("Unable to write history to '%s': %s\n",historyfile,strerror(err));
      return;
    }
  } 
  history_truncate_file(historyfile,MAXHISTORYFILE);
}
#endif


/* Increases the buffer by BUFGROW bytes and leaves the new pointer in buf
   and the new buffer size in bufsize. */

#define BUFGROW 100

void
growbuffer(char **buf, int *bufsize)
{
  int usemalloc;

  usemalloc = !*buf || !*bufsize;
  *bufsize += BUFGROW;
  if (usemalloc)
    *buf = malloc(*bufsize);
  else
    *buf = realloc(*buf,*bufsize);
  if (!*buf){
    fprintf(stderr, "%s: memory allocation error (growbuffer)\n",progname);  
    exit(EXIT_FAILURE); 
  }
}


FILE *
openfile(char *file,char *mode)
{
  FILE *fileptr;
  int ret;

  struct stat statbuf;
  if (stat(file, &statbuf)==0 && statbuf.st_mode & S_IFDIR){
    errno=EISDIR;
    return NULL;
  }
  fileptr = fopen(file,mode);
  return fileptr;
}  



void
logprintf(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  if (logfile) {
    va_start(args, format);
    vfprintf(logfile, format, args);
    va_end(args);
  }
}

void
logputchar(char c)
{
  putchar(c);
  if (logfile) fputc(c, logfile);
}

void
logputs(const char *s)
{
  fputs(s, stdout);
  if (logfile) fputs(s, logfile);
}
    

/* Look for a subscript in the input string.  A subscript starts with
   '_' and is followed by a sequence of only digits (matching the
   regexp "_[0-9]+").  The function returns 1 if it finds a subscript
   and zero otherwise.  Note that it returns 1 for an input that is 
   entirely subscript, with the '_' appearing in the first position. */

int
hassubscript(const char *str)
{
  const char *ptr = &lastchar(str);
  while (ptr>str){
    if (!strchr(digits, *ptr))
      return 0;
    ptr--;
    if (*ptr=='_')
      return 1;
  }
  return 0;
}


/* replace various Unicode minus symbols with ASCII hyphen-minus U+002D */
void
replace_minus(char *input)
{
  char *unicode_minus[] = {
    "\xE2\x80\x92", /* U+2012: figure dash */
    "\xE2\x80\x93", /* U+2013: en dash */
    "\xE2\x88\x92",  /* U+2212: minus */
    0
  };
  char *inptr, *outptr, *ptr, **minus;

  for (minus=unicode_minus; *minus; minus++) {
    inptr = outptr = input;
    do {
      ptr = strstr(inptr, *minus);  /* find next unicode minus */
      if (ptr) {
        while (inptr < ptr)   /* copy the input up to the minus symbol */
          *outptr++ = *inptr++;
        *outptr++ = '-';        /* U+002D: hyphen-minus */
        inptr = ptr + strlen(*minus);
      }
    } while (ptr);
    /* if no replacements were made, the input isn't changed */
    if (inptr > input) {
      while (*inptr)
        *outptr++ = *inptr++;
      *outptr = '\0';
    }
  }
}



/* Replace all control chars with a space */

void
replacectrlchars(char *string)
{
  for(;*string;string++)
    if (iscntrl(*string))
      *string = ' ';
}

/* 
   Fetch a line of data with backslash for continuation.  The
   parameter count is incremented to report the number of newlines
   that are read so that line numbers can be accurately reported. 
*/

char *
fgetscont(char *buf, int size, FILE *file, int *count)
{
  if (!fgets(buf,size,file))
    return 0;
  (*count)++;
  while(strlen(buf)>=2 && 0==strcmp(buf+strlen(buf)-2,"\\\n")){
    (*count)++;
    buf[strlen(buf)-2] = 0; /* delete trailing \n and \ char */
    if (strlen(buf)>=size-1) /* return if the buffer is full */
      return buf;
    if (!fgets(buf+strlen(buf), size - strlen(buf), file))
      return buf;  /* already read some data so return success */
  }
  if (lastchar(buf) == '\\') {   /* If last char of buffer is \ then   */
    ungetc('\\', file);           /* we don't know if it is followed by */
    lastchar(buf) = 0;           /* a \n, so put it back and try again */
  }
  return buf;
}


/* 
   Gets arbitrarily long input data into a buffer using growbuffer().
   Returns 0 if no data is read.  Increments count by the number of
   newlines read unless it points to NULL. 
 
   Replaces tabs and newlines with spaces before returning the result.
*/

char *
fgetslong(char **buf, int *bufsize, FILE *file, int *count)
{
  int dummy;
  if (!count)
    count = &dummy;
  if (!*bufsize) growbuffer(buf,bufsize);
  if (!fgetscont(*buf, *bufsize, file, count))
    return 0;
  while (lastchar(*buf) != '\n' && !feof(file)){
    growbuffer(buf, bufsize);
    fgetscont(*buf+strlen(*buf), *bufsize-strlen(*buf), file, count);
    (*count)--;
  }  
  /* These nonprinting characters must be removed so that the test
     for UTF-8 validity will work. */ 
  replacectrlchars(*buf);
  return *buf;
}

/* Allocates memory and aborts if malloc fails. */

void *
mymalloc(int bytes,const char *mesg)
{
   void *pointer;

   pointer = malloc(bytes);
   if (!pointer){
     fprintf(stderr, "%s: memory allocation error %s\n", progname, mesg);
     exit(EXIT_FAILURE);
   }
   return pointer;
}


/* Duplicates a string */

char *
dupstr(const char *str)
{
   char *ret;

   ret = mymalloc(strlen(str) + 1,"(dupstr)");
   strcpy(ret, str);
   return ret;
}

/* Duplicates a string that is not null-terminated, 
   adding the null to the copy */

char *
dupnstr(const char *string, int length)
{  
  char *newstr;
  newstr = mymalloc(length+1,"(dupnstr)");
  strncpy(newstr, string, length);
  newstr[length]=0;
  return newstr;
}
  

#ifdef SUPPORT_UTF8

/* 
   The strwidth function gives the printed width of a UTF-8 byte sequence.
   It will return -1 if the sequence is an invalid UTF-8 sequence or
   if the sequence contains "nonprinting" characters.  Note that \n and \t are 
   "nonprinting" characters. 
*/

int 
strwidth(const char *str)
{
  wchar_t *widestr;
  int len;

  if (!utf8mode)
    return strlen(str);
  len = strlen(str)+1;
  widestr = mymalloc(sizeof(wchar_t)*len, "(strwidth)");
  len = mbsrtowcs(widestr, &str, len, NULL);

  if (len==-1){
    free(widestr);
    return -1; /* invalid multibyte sequence */
  }

  len=wcswidth(widestr, len);
  free(widestr);
  return len;
}
#else
#  define strwidth strlen
#endif 



/* hashing algorithm for units */

unsigned
uhash(const char *str)
{
   unsigned hashval;

   for (hashval = 0; *str; str++)
      hashval = *str + HASHNUMBER * hashval;
   return (hashval % HASHSIZE);
}


/* Lookup a unit in the units table.  Returns the definition, or NULL
   if the unit isn't found in the table. */

struct unitlist *
ulookup(const char *str)
{
   struct unitlist *uptr;

   for (uptr = utab[uhash(str)]; uptr; uptr = uptr->next)
      if (strcmp(str, uptr->name) == 0)
         return uptr;
   return NULL;
}

/* Lookup a prefix in the prefix table.  Finds the longest prefix that
   matches the beginning of the input string.  Returns NULL if no
   prefixes match. */

struct prefixlist *
plookup(const char *str)
{
   struct prefixlist *prefix;
   struct prefixlist *bestprefix=NULL;
   int bestlength=0;

   for (prefix = ptab[simplehash(str)]; prefix; prefix = prefix->next) {
     if (prefix->len > bestlength && !strncmp(str, prefix->name, prefix->len)){
       bestlength = prefix->len;
       bestprefix = prefix;
     }
   }
   return bestprefix;
}

/* Look up function in the function linked list */

struct func *
fnlookup(const char *str)
{ 
  struct func *funcptr;

  for(funcptr=ftab[simplehash(str)];funcptr;funcptr = funcptr->next)
    if (!strcmp(funcptr->name, str))
      return funcptr;
  return 0;
}

struct wantalias *
aliaslookup(const char *str)
{
  struct wantalias *aliasptr;
  for(aliasptr = firstalias; aliasptr; aliasptr=aliasptr->next)
    if (!strcmp(aliasptr->name, str))
      return aliasptr;
  return 0;
}


/* Insert a new function into the linked list of functions */

void
addfunction(struct func *newfunc)
{
  int val;

  val = simplehash(newfunc->name);
  newfunc->next = ftab[val];
  ftab[val] = newfunc;
}


/* Free the fields in the function except for the name so that it
   can be redefined.  It remains in position in the linked list. */
void
freefunction(struct func *funcentry)
{
  if (funcentry->table){
    free(funcentry->table);
    free(funcentry->tableunit);
  } else {
    free(funcentry->forward.param);
    free(funcentry->forward.def);
    if (funcentry->forward.domain_min) free(funcentry->forward.domain_min);
    if (funcentry->forward.domain_max) free(funcentry->forward.domain_max);
    if (funcentry->inverse.domain_min) free(funcentry->inverse.domain_min);
    if (funcentry->inverse.domain_max) free(funcentry->inverse.domain_max);
    if (funcentry->forward.dimen) free(funcentry->forward.dimen);
    if (funcentry->inverse.dimen) free(funcentry->inverse.dimen);
    if (funcentry->inverse.def) free(funcentry->inverse.def);    
    if (funcentry->inverse.param) free(funcentry->inverse.param);
  }
}

/* Remove leading and trailing spaces from the input */

void
removespaces(char *in)
{
  char *ptr;
  if (*in) {
    for(ptr = &lastchar(in); *ptr==' '; ptr--); /* Last non-space */
    *(ptr+1)=0;
    if (*in==' '){
      ptr = in + strspn(in," ");
      memmove(in, ptr, strlen(ptr)+1);
    }
  }
}


/* 
   Looks up an inverse function given as a ~ character followed by
   spaces and then the function name.  The spaces will be deleted as a
   side effect.  If an inverse function is found returns the function
   pointer, otherwise returns null.
*/

struct func *
invfnlookup(char *str)
{
  if (*str != '~')
    return 0;
  removespaces(str+1);
  return fnlookup(str+1);
}


char *
strip_comment(char *line)
{
  char *comment = 0;
  
  if ((line = strchr(line,COMMENTCHAR))) {
    comment = line+1;
    *line = 0;
  }
  return comment;
}


/* Print string but replace two consecutive spaces with one space. */

void
tightprint(FILE *outfile, char *string)
{
  while(*string){
    fputc(*string, outfile);
    if (*string != ' ') string++;
    else while(*string==' ') string++;
  }
}


#define readerror (goterr=1) && errfile && fprintf

#define VAGUE_ERR "%s: error in units file '%s' line %d\n", \
                       progname, file, linenum

/* Print out error message encountered while reading the units file. */


/*
  Splits the line into two parts.  The first part is space delimited.
  The second part is everything else.  Removes trailing spaces from
  the second part.  Returned items are null if no parameter was found.
*/
  
void
splitline(char *line, char **first, char **second)
{
  *second = 0;
  *first = strtok(line, " ");
  if (*first){
    *second = strtok(0, "\n");
    if (*second){
      removespaces(*second);
      if (emptystr(*second))
        *second = 0;
    }
  }
}


/* see if character is part of a valid decimal number */

int
isdecimal(char c)
{
  return strchr(digits, c) != NULL;
}


/* 
   Check for some invalid unit names.  Print error message.  Returns 1 if 
   unit name is bad, zero otherwise.  
*/
int
checkunitname(char *name, int linenum, char *file, FILE *errfile)
{
  char nonunitchars[] = "~;+-*/|^)";  /* Also defined in parse.y with a few more characters */
  char **ptr;
  char *cptr;

  if ((cptr=strpbrk(name, nonunitchars))){
    if (errfile) fprintf(errfile,
         "%s: unit '%s' in units file '%s' on line %d ignored.  It contains invalid character '%c'\n",
              progname, name, file, linenum, *cptr);
    return 1;
  }
  if (strchr(digits, name[0])){
    if (errfile) fprintf(errfile,
         "%s: unit '%s' in units file '%s' on line %d ignored.  It starts with a digit\n", 
              progname, name, file, linenum);
    return 1;
  }
  for(ptr=builtins;*ptr;ptr++)
    if (!strcmp(name, *ptr)){
      if (errfile) fprintf(errfile,
           "%s: redefinition of built-in function '%s' in file '%s' on line %d ignored.\n",
                           progname, name, file, linenum);
      return 1;
    }
  for(ptr=all_commands;*ptr;ptr++)
    if (!strcmp(name, *ptr)){
      if (errfile) fprintf(errfile,
           "%s: unit name '%s' in file '%s' on line %d may be hidden by command with the same name.\n",
                           progname, name, file, linenum);
    }
  return 0;
}


int
newunit(char *unitname, char *unitdef, int *count, int linenum, 
        char *file,FILE *errfile, int redefine)
{
  struct unitlist *uptr;
  unsigned hashval; 

  /* units ending with '_' create ambiguity for exponents */

  if (unitname[0]=='_' || lastchar(unitname)=='_'){
    if (errfile) fprintf(errfile,
       "%s: unit '%s' on line %d of '%s' ignored.  It starts or ends with '_'\n",
              progname, unitname, linenum, file);
    return E_BADFILE;
  }

  /* Units that end in [2-9] can never be accessed */
  if (strchr(".,23456789", lastchar(unitname)) && !hassubscript(unitname)){
    if (errfile) fprintf(errfile,
       "%s: unit '%s' on line %d of '%s' ignored.  %s\n",
        progname, unitname, linenum, file,errormsg[E_UNITEND]);
    return E_BADFILE;
  }

  if (checkunitname(unitname, linenum, file, errfile))
    return E_BADFILE;

  if ((uptr=ulookup(unitname))) {    /* Is it a redefinition? */
    if (flags.unitcheck && errfile && !redefine)
      fprintf(errfile,
      "%s: unit '%s' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
              progname, unitname, uptr->linenumber,uptr->file,
              linenum, file);
    free(uptr->value);
  } else {       
    /* make new units table entry */

    uptr = (struct unitlist *) mymalloc(sizeof(*uptr),"(newunit)");
    uptr->name = dupstr(unitname);

    /* install unit name/value pair in list */

    hashval = uhash(uptr->name);
    uptr->next = utab[hashval];
    utab[hashval] = uptr;
    (*count)++;
  }
  uptr->value = dupstr(unitdef);
  uptr->linenumber = linenum;
  uptr->file = file;
  return 0;
}


int 
newprefix(char *unitname, char *unitdef, int *count, int linenum, 
          char *file,FILE *errfile, int redefine)
{
  struct prefixlist *pfxptr;
  unsigned pval;

  lastchar(unitname) = 0;
  if (checkunitname(unitname,linenum,file,errfile))
    return E_BADFILE;
  if ((pfxptr = plookup(unitname))     /* already there: redefinition */
      && !strcmp(pfxptr->name, unitname)){
    if (flags.unitcheck && errfile && !redefine)
      fprintf(errfile,
             "%s: prefix '%s-' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
              progname, unitname, pfxptr->linenumber,pfxptr->file,
              linenum, file);
    free(pfxptr->value);
  } else {  
    pfxptr = (struct prefixlist *) mymalloc(sizeof(*pfxptr),"(newprefix)");  
    pfxptr->name = dupstr(unitname);
    pfxptr->len = strlen(unitname);
    pval = simplehash(unitname);
    pfxptr->next = ptab[pval];
    ptab[pval] = pfxptr;
    (*count)++;
  }
  pfxptr->value = dupstr(unitdef);
  pfxptr->linenumber = linenum;
  pfxptr->file = file;
  return 0;
}


/* 
   parsepair() looks for data of the form [text1,text2] where the ',' is a 
   specified delimiter.  The second argument, text2, is optional and if it's 
   missing then second is set to NULL.  The parameters are allowed to be 
   empty strings.  The function returns the first character after the 
   closing bracket if no errors occur or the NULL pointer on error.
*/

char *
parsepair(char *input, char **first, char **second, 
          int *firstopen, int *secondopen, char delimiter, int checkopen,
          char *unitname, int linenum, char *file,FILE *errfile)
{
  char *start, *end, *middle;

  start = strpbrk(input, checkopen?"[(":"[");
  if (!start){
    if (errfile) fprintf(errfile,
             "%s: expecting '[' %s in definition of '%s' in '%s' line %d\n",
             progname, checkopen ? "or '('":"", unitname, file, linenum);
    return 0;
  }
  if (*start=='(') *firstopen=1;
  else *firstopen=0;
  *start++=0;
  removespaces(input);
  if (!emptystr(input)){
    if (errfile) fprintf(errfile,
        "%s: unexpected characters before '%c' in definition of '%s' in '%s' line %d\n",
        progname, *firstopen?'(':'[',unitname, file, linenum);
    return 0;
  }
  end = strpbrk(start, checkopen?"])":"]");
  if (!end){
    if (errfile) fprintf(errfile,
             "%s: expecting ']' %s in definition of '%s' in '%s' line %d\n",
             progname, checkopen?"or ')'":"",unitname, file, linenum);
    return 0;
  }
  if (*end==')') *secondopen=1;
  else *secondopen=0;
  *end++=0;

  middle = strchr(start,delimiter);
  
  if (middle){
    *middle++=0;
    removespaces(middle);
    *second = middle;
  } else
    *second = 0;

  removespaces(start);
  *first = start;
  return end;
}


/* 
   Extract numbers from two text strings and place them into pointers. 
   Has two error codes for decreasing interval or bad numbers in the 
   text strings.  Returns 0 on success.  
*/

#define EI_ERR_DEC 1    /* Decreasing interval */
#define EI_ERR_MALF 2   /* Malformed number */

int
extract_interval(char *first, char *second, 
                 double **firstout, double **secondout)
{
  double val;
  char *end;

  if (!emptystr(first)){
    val = strtod(first, &end);
    if (*end)
      return EI_ERR_MALF;
    else {
      *firstout=(double *)mymalloc(sizeof(double), "(extract_interval)");
      **firstout = val;
    }
  }
  if (nonempty(second)) {
    val = strtod(second, &end);
    if (*end)
      return EI_ERR_MALF;
    else if (*firstout && **firstout>=val)
      return EI_ERR_DEC;
    else {
      *secondout=(double *)mymalloc(sizeof(double), "(extract_interval)");
      **secondout = val;
    }
  }
  return 0;
}


 
void 
copyfunctype(struct functype *dest, struct functype *src)
{
  dest->domain_min_open = src->domain_min_open;
  dest->domain_max_open = src->domain_max_open;
  dest->param = dest->def = dest->dimen = NULL;
  dest->domain_min = dest->domain_max = NULL;
  if (src->param) dest->param = dupstr(src->param); 
  if (src->def) dest->def = dupstr(src->def);
  if (src->dimen) dest->dimen = dupstr(src->dimen);
  if (src->domain_min){
    dest->domain_min = (double *) mymalloc(sizeof(double), "(copyfunctype)");
    *dest->domain_min = *src->domain_min;
  }
  if (src->domain_max){
    dest->domain_max = (double *) mymalloc(sizeof(double), "(copyfunctype)");
    *dest->domain_max = *src->domain_max;
  }
}


int
copyfunction(char *unitname, char *funcname, int *count, int linenum, 
             char *file, FILE *errfile)
{
  struct func *source, *funcentry;
  int i;
  if (checkunitname(unitname, linenum, file, errfile))
    return E_BADFILE;
  removespaces(funcname);
  i = strlen(funcname)-2;                /* strip trailing () if present */
  if (i>0 && !strcmp(funcname+i,"()"))  
    funcname[i]=0;
  source = fnlookup(funcname);
  if (!source) {
    if (errfile){ 
      if (!strpbrk(funcname," ;][()+*/-^")) 
        fprintf(errfile,"%s: bad definition for '%s' in '%s' line %d, function '%s' not defined\n",
                progname, unitname, file, linenum, funcname);
      else
        fprintf(errfile,"%s: bad function definition of '%s' in '%s' line %d\n",
                progname,unitname,file,linenum);
    }
    return E_BADFILE;
  }
  if ((funcentry=fnlookup(unitname))){
    if (flags.unitcheck && errfile)          
      fprintf(errfile,
             "%s: function '%s' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
              progname, unitname, funcentry->linenumber,funcentry->file,
              linenum, file);
    freefunction(funcentry);
  } else {
    funcentry = (struct func*)mymalloc(sizeof(struct func),"(newfunction)");    
    funcentry->name = dupstr(unitname);
    addfunction(funcentry);
    (*count)++;
  }
  funcentry->linenumber = linenum;
  funcentry->file = file;
  funcentry->skip_error_check = source->skip_error_check;
  if (source->table){
    funcentry->tablelen = source->tablelen;
    funcentry->tableunit = dupstr(source->tableunit);
    funcentry->table = (struct pair *)
        mymalloc(sizeof(struct pair)*funcentry->tablelen, "(copyfunction)");
    for(i=0;i<funcentry->tablelen;i++){
      funcentry->table[i].location = source->table[i].location;
      funcentry->table[i].value = source->table[i].value;
    }
  } else {
    funcentry->table = 0;
      copyfunctype(&funcentry->forward, &source->forward);
      copyfunctype(&funcentry->inverse, &source->inverse);
  }
  return 0;
}


#define FREE_STUFF {if (forward_dim) free(forward_dim);\
                    if (inverse_dim) free(inverse_dim);\
                    if (domain_min) free(domain_min);\
                    if (domain_max) free(domain_max);\
                    if (range_min) free(range_min);\
                    if (range_max) free(range_max);}

#define REPEAT_ERR \
  if (errfile) fprintf(errfile, \
     "%s: keyword '%s' repeated in definition of '%s' on line %d of '%s'.\n",\
                      progname,fnkeywords[i].word,unitname, linenum, file)

int
newfunction(char *unitname, char *unitdef, int *count, 
            int linenum, char *file,FILE *errfile, int redefine)
{
  char *start, *end, *inv, *forward_dim, *inverse_dim, *first, *second;
  double *domain_min, *domain_max, *range_min, *range_max;
  struct func *funcentry;
  int looking_for_keywords,i, firstopen, secondopen;
  int domain_min_open, domain_max_open, range_min_open, range_max_open;
  int noerror = 0;

  if (*unitname=='('){
    if (errfile) fprintf(errfile,
         "%s: unit '%s' on line %d of '%s' ignored.  It starts with a '('\n", 
         progname, unitname, linenum, file);
    return E_BADFILE;
  }
                                   /* coverity[returned_null] */
  start = strchr(unitname,'(');
  end = strchr(unitname,')');
  *start++ = 0;
    
  if (checkunitname(unitname,linenum,file,errfile))
    return E_BADFILE;
  
  if (start==end)  /* no argument: function() so make a function copy */
    return copyfunction(unitname, unitdef, count, linenum, file, errfile);

  if (!end || strlen(end)>1){
    if (errfile) fprintf(errfile,
              "%s: bad function definition of '%s' in '%s' line %d\n",
              progname,unitname,file,linenum);
    return E_BADFILE;
  }
  *end=0;
  forward_dim = NULL;
  inverse_dim = NULL;
  domain_min = NULL;
  domain_max = NULL;
  range_min = NULL;
  range_max = NULL;
  domain_min_open = 0;
  domain_max_open = 0;
  range_min_open = 0;
  range_max_open = 0;
  looking_for_keywords=1;
  while (looking_for_keywords) {
    looking_for_keywords = 0;
    for(i=0;fnkeywords[i].word;i++){
      if (startswith(unitdef, fnkeywords[i].word)){
        looking_for_keywords = 1;  /* found keyword so keep looking */
        unitdef+=strlen(fnkeywords[i].word);
        if (fnkeywords[i].checkopen!=CO_NOARG){
          unitdef = parsepair(unitdef,&first, &second, &firstopen, &secondopen, 
                            fnkeywords[i].delimit, fnkeywords[i].checkopen,
                            unitname, linenum, file,errfile);
          if (!unitdef){
            FREE_STUFF;
            return E_BADFILE;
          }
          removespaces(unitdef);
        }
        if (i==FN_NOERROR)
          noerror = 1;
        if (i==FN_UNITS){
          if (forward_dim || inverse_dim){
            REPEAT_ERR;
            return E_BADFILE;
          }
          forward_dim = dupstr(first);
          if (second)
            inverse_dim = dupstr(second);
        }
        if (i==FN_DOMAIN){
          int err=0;
          if (domain_min || domain_max){
            REPEAT_ERR;
            return E_BADFILE;
          }
          err = extract_interval(first,second,&domain_min, &domain_max);
          domain_min_open = firstopen;
          domain_max_open = secondopen;
          if (err)
            FREE_STUFF;
          if (err==EI_ERR_DEC){
            if (errfile) fprintf(errfile,
               "%s: second endpoint for domain must be greater than the first\n       in definition of '%s' in '%s' line %d\n",
                progname, unitname, file, linenum);
            return E_BADFILE;
          }
          if (err==EI_ERR_MALF){
            if (errfile) fprintf(errfile,
              "%s: malformed domain in definition of '%s' in '%s' line %d\n",
              progname, unitname, file, linenum);
            return E_BADFILE;
          }
        } 
        if (i==FN_RANGE){
          int err=0;
          if (range_min || range_max){
            REPEAT_ERR;
            FREE_STUFF;
            return E_BADFILE;
          }
          err = extract_interval(first,second,&range_min, &range_max);
          range_min_open = firstopen;
          range_max_open = secondopen;
          if (err)
            FREE_STUFF;
          if (err==EI_ERR_DEC){
            if (errfile) fprintf(errfile,
               "%s: second endpoint for range must be greater than the first\n       in definition of '%s' in '%s' line %d\n",
                progname, unitname, file, linenum);
            return E_BADFILE;
          }
          if (err==EI_ERR_MALF){
            if (errfile) fprintf(errfile,
              "%s: malformed range in definition of '%s' in '%s' line %d\n",
              progname, unitname, file, linenum);
            return E_BADFILE;
          }
        }
      }
    }
  }

  if (emptystr(unitdef)){
    if (errfile) fprintf(errfile,
                 "%s: function '%s' lacks a definition at line %d of '%s'\n",
                 progname, unitname, linenum, file);
    FREE_STUFF;
    return E_BADFILE;
  }

  if (*unitdef=='['){
    if (errfile) fprintf(errfile,
         "%s: function '%s' missing keyword before '[' on line %d of '%s'\n", 
         progname, unitname, linenum, file);
    FREE_STUFF;
    return E_BADFILE;
  }

  /* Check that if domain and range are specified and nonzero then the 
     units are given.  Otherwise these are meaningless.  */
  if (!forward_dim &&
      ((domain_min && *domain_min) || (domain_max && *domain_max))){ 
    if (errfile)
      fprintf(errfile,"%s: function '%s' defined on line %d of '%s' has domain with no units.\n", 
              progname, unitname, linenum, file);
    FREE_STUFF;
    return E_BADFILE;
  }
  if (!inverse_dim &&
      ((range_min && *range_min) || (range_max && *range_max))){ 
    if (errfile)
      fprintf(errfile,"%s: function '%s' defined on line %d of '%s' has range with no units.\n", 
              progname, unitname, linenum, file);
    FREE_STUFF;
    return E_BADFILE;
  }
  if ((funcentry=fnlookup(unitname))){
    if (flags.unitcheck && errfile && !redefine)
      fprintf(errfile,
             "%s: function '%s' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
              progname, unitname, funcentry->linenumber,funcentry->file,
              linenum, file);
    freefunction(funcentry);
  } else {
    funcentry = (struct func*)mymalloc(sizeof(struct func),"(newfunction)");    
    funcentry->name = dupstr(unitname);
    addfunction(funcentry);
    (*count)++;
  }
  funcentry->table = 0;
  funcentry->skip_error_check = noerror;
  funcentry->forward.dimen = forward_dim;
  funcentry->inverse.dimen = inverse_dim;
  funcentry->forward.domain_min = domain_min;
  funcentry->forward.domain_max = domain_max;
  funcentry->inverse.domain_min = range_min;
  funcentry->inverse.domain_max = range_max;
  funcentry->forward.domain_min_open = domain_min_open;
  funcentry->forward.domain_max_open = domain_max_open;
  funcentry->inverse.domain_min_open = range_min_open;
  funcentry->inverse.domain_max_open = range_max_open;
  inv = strchr(unitdef,FUNCSEPCHAR);
  if (inv)
    *inv++ = 0;
  funcentry->forward.param = dupstr(start);
  removespaces(unitdef);
  funcentry->forward.def = dupstr(unitdef);
  if (inv){
    removespaces(inv);
    funcentry->inverse.def = dupstr(inv);
    funcentry->inverse.param = dupstr(unitname);
  } 
  else {
    funcentry->inverse.def = 0;
    funcentry->inverse.param = 0;
  }
  funcentry->linenumber = linenum;
  funcentry->file = file;
  return 0;
}


int 
newtable(char *unitname,char *unitdef, int *count,
         int linenum, char *file,FILE *errfile, int redefine)
{
  char *start, *end;
  char *tableunit;
  int tablealloc, tabpt;
  struct pair *tab;
  struct func *funcentry;
  int noerror = 0;

                                      /* coverity[returned_null] */
  tableunit = strchr(unitname,'[');
  end = strchr(unitname,']');
  *tableunit++=0;
  if (checkunitname(unitname, linenum, file, errfile))
    return E_BADFILE;
  if (!end){
    if (errfile) fprintf(errfile,"%s: missing ']' in units file '%s' line %d\n",
              progname,file,linenum);
    return E_BADFILE;
  } 
  if (strlen(end)>1){
    if (errfile) fprintf(errfile,
              "%s: unexpected characters after ']' in units file '%s' line %d\n",
              progname,file,linenum);
    return E_BADFILE;
  }
  *end=0;
  tab = (struct pair *)mymalloc(sizeof(struct pair)*20, "(newtable)");
  tablealloc=20;
  tabpt = 0;
  start = unitdef;
  if (startswith(start, NOERROR_KEYWORD)) {
    noerror = 1;
    start += strlen(NOERROR_KEYWORD);
    removespaces(start);
  }
  while (1) {
    if (tabpt>=tablealloc){
      tablealloc+=20;
      tab = (struct pair *)realloc(tab,sizeof(struct pair)*tablealloc);
      if (!tab){
        if (errfile) fprintf(errfile, "%s: memory allocation error (newtable)\n",
                  progname);  
        return E_MEMORY;
      }
    }
    tab[tabpt].location = strtod(start,&end);
    if (start==end || (!emptystr(end) && *end !=' ')){ 
      if (!emptystr(start)) {  
        if (strlen(start)>15) start[15]=0;  /* Truncate for error msg display */
        if (errfile) fprintf(errfile, 
             "%s: cannot parse table definition %s at '%s' on line %d of '%s'\n", 
             progname, unitname, start, linenum, file);
        free(tab);
        return E_BADFILE;
      }
      break;
    }
    if (tabpt>0 && tab[tabpt].location<=tab[tabpt-1].location){
      if (errfile)
        fprintf(errfile,"%s: points don't increase (%.8g to %.8g) in units file '%s' line %d\n",
                progname, tab[tabpt-1].location, tab[tabpt].location,
                file, linenum);
      free(tab);
      return E_BADFILE;
    }
    start=end+strspn(end," ");
    tab[tabpt].value = strtod(start,&end);
    if (start==end){
      if (errfile)
        fprintf(errfile,"%s: missing value after %.8g in units file '%s' line %d\n",
                progname, tab[tabpt].location, file, linenum);
      free(tab);
      return E_BADFILE;
    }
    tabpt++;
    start=end+strspn(end," ,");
  }
  if ((funcentry=fnlookup(unitname))){
    if (flags.unitcheck && errfile && !redefine)
      fprintf(errfile,
      "%s: unit '%s' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
                    progname, unitname, funcentry->linenumber,funcentry->file,
                                         linenum, file);
    freefunction(funcentry);
  } else {
    funcentry = (struct func *)mymalloc(sizeof(struct func),"(newtable)");
    funcentry->name = dupstr(unitname);
    addfunction(funcentry);
    (*count)++;
  }
  funcentry->tableunit = dupstr(tableunit);
  funcentry->tablelen = tabpt;
  funcentry->table = tab;
  funcentry->skip_error_check = noerror;
  funcentry->linenumber = linenum;
  funcentry->file = file;
  return 0;
}


int
newalias(char *unitname, char *unitdef,int linenum, char *file,FILE *errfile)
{
  struct wantalias *aliasentry;

  if (!strchr(unitdef, UNITSEPCHAR)){
    if (errfile) fprintf(errfile,
            "%s: unit list missing '%c' on line %d of '%s'\n",
            progname, UNITSEPCHAR, linenum, file);
    return E_BADFILE;
  }
  if ((aliasentry=aliaslookup(unitname))){   /* duplicate alias */
    if (flags.unitcheck && errfile)
      fprintf(errfile,
              "%s: unit list '%s' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
              progname, unitname, aliasentry->linenumber,
              aliasentry->file, linenum, file);
    free(aliasentry->definition);
  } else { 
    aliasentry = (struct wantalias *)
      mymalloc(sizeof(struct wantalias),"(newalias)");
    aliasentry->name = dupstr(unitname);
    aliasentry->next = 0;
    *aliaslistend = aliasentry;
    aliaslistend = &aliasentry->next;
  }
  aliasentry->definition = dupstr(unitdef);
  aliasentry->linenumber = linenum;
  aliasentry->file = file;
  return 0;
}


/* 
   Check environment variable name to see if its value appears on the
   space delimited text string pointed to by list.  Returns 2 if the
   environment variable is not set, return 1 if its value appears on
   the list and zero otherwise.
*/

int
checkvar(char *name, char *list)
{
  char *listitem;
  name = getenv(name);
  if (!name)
    return 2;
  listitem = strtok(list," ");
  while (listitem){
    if (!strcmp(name, listitem))
      return 1;
    listitem = strtok(0," ");
  }
  return 0;
}


#ifdef NO_SETENV
int
setenv(const char *name, const char *val, int overwrite)
{
  char *environment;

  if (!overwrite && getenv(name) != NULL)
    return 0;
  environment = (char *) malloc(strlen(name) + strlen(val) + 2);
  if (!environment)
    return 1;
  strcpy(environment, name);
  strcat(environment, "=");
  strcat(environment, val);

  /* putenv() doesn't copy its argument, so don't free environment */

#if defined (_WIN32) && defined (_MSC_VER)
  return _putenv(environment);
#else
  return putenv(environment);
#endif
}
#endif

#ifdef _WIN32
#  define  isdirsep(c)  ((c) == '/' || (c) == '\\')
#  define  hasdirsep(s) strpbrk((s),"/\\")
#else
#  define  isdirsep(c)  ((c) == '/')
#  define  hasdirsep(s) strchr((s),'/')
#endif
#define  isexe(s) ((strlen(s) == 4) && (tolower(s[1]) == 'e') \
                 && (tolower(s[2]) == 'x') && (tolower(s[3]) == 'e'))

/* Returns a pointer to the end of the pathname part of the 
   specified filename */

char *
pathend(char *filename)
{
  char *pointer;

  for(pointer=filename+strlen(filename);pointer>filename;pointer--){
    if (isdirsep(*pointer)) {
      pointer++;
      break;
    }
  }
  return pointer;
}

int
isfullpath(char *path)
{
#ifdef _WIN32
  /* handle Windows drive specifier */
  if (isalpha(*path) && *(path + 1) == ':')
      path += 2;
#endif
  return isdirsep(*path);
}

/* 
   Read in units data.  

   file - Filename to load
   errfile - File to receive messages about errors in the units database.  
             Set it to 0 to suppress errors. 
   unitcount, prefixcount, funccount - Return statistics to the caller.  
                                       Must initialize to zero before calling.
   depth - Used to prevent recursive includes.  Call with it set to zero.

   The global variable progname is used in error messages.  
*/

int
readunits(char *file, FILE *errfile, 
          int *unitcount, int *prefixcount, int *funccount, int depth)
{
   FILE *unitfile;
   char *line = 0, *lineptr, *unitdef, *unitname, *permfile;
   int linenum, linebufsize, goterr, retcode;
   int locunitcount, locprefixcount, locfunccount, redefinition;
   int wronglocale = 0;   /* If set then we are currently reading data */
   int inlocale = 0;      /* for the wrong locale so we should skip it */
   int in_utf8 = 0;       /* If set we are reading utf8 data */
   int invar = 0;         /* If set we are in data for an env variable.*/
   int wrongvar = 0;      /* If set then we are not processing */

   locunitcount = 0;
   locprefixcount = 0;
   locfunccount  = 0;
   linenum = 0;
   linebufsize = 0;
   goterr = 0;

   unitfile = openfile(file, "rt");
   
   if (!unitfile){
     if (errfile)
       fprintf(errfile, "%s: Unable to read units file '%s': %s\n", progname, file, strerror(errno));
     return E_FILE;
   }
   growbuffer(&line,&linebufsize);
                                            /* coverity[alloc_fn] */
   permfile = dupstr(file);    /* This is a permanent copy to reference in */
                               /* the database. It is never freed. */
   while (!feof(unitfile)) {
      if (!fgetslong(&line, &linebufsize, unitfile, &linenum)) 
        break;
      if (linenum==1 && startswith(line, UTF8MARKER)){ 
        int i;
        for(lineptr=line,i=0;i<strlen(UTF8MARKER);i++, lineptr++)
          *lineptr=' ';
      }
      strip_comment(line);
      if (-1 == strwidth(line)){
        readerror(errfile, "%s: %s on line %d of '%s'\n",
                      progname, invalid_utf8, linenum, file);
        continue;
      }
      replace_minus(line);

      if (*line == COMMANDCHAR) {         /* Process units file commands */
        unitname = strtok(line+1, " ");
        if (!unitname){
          readerror(errfile, VAGUE_ERR);
          continue;
        }

        /* Check for locale and utf8 declarations.  Any other commands
           most likely belong after these tests. */
        if (!strcmp(unitname,"var") || !strcmp(unitname,"varnot")){
          int not = 0;
          if (unitname[3]=='n')
            not = 1;
          unitname=strtok(0," ");
          unitdef=strtok(0,"");  /* Get rest of the line */
          if (!unitname)
            readerror(errfile, 
                      "%s: no variable name specified on line %d of '%s'\n",
                      progname, linenum, file);
          else if (!unitdef)
            readerror(errfile, 
                      "%s: no value specified on line %d of '%s'\n",
                      progname, linenum, file);
          else if (invar)
            readerror(errfile,
                 "%s: nested var statements not allowed, line %d of '%s'\n",
                      progname, linenum, file);
          else {
            int check;
            invar = 1;
            check = checkvar(unitname, unitdef);
            if (check==2){
              readerror(errfile,
                 "%s: environment variable %s not set at line %d of '%s'\n",
                         progname, unitname, linenum, file);
              wrongvar = 1;
            }
            else if (!(not^check))
              wrongvar = 1;
          }         
          continue;
        }
        else if (!strcmp(unitname, "endvar")){
          if (!invar)
            readerror(errfile, 
                      "%s: unmatched !endvar on line %d of '%s'\n",
                      progname, linenum, file);
          wrongvar = 0;
          invar = 0;
          continue;
        }
        else if (!strcmp(unitname,"locale")){ 
          unitname = strtok(0, " "); 
          if (!unitname)
            readerror(errfile, 
                      "%s: no locale specified on line %d of '%s'\n",
                      progname, linenum, file);
          else if (inlocale)
            readerror(errfile,
                      "%s: nested locales not allowed, line %d of '%s'\n",
                      progname, linenum, file);
          else {
            inlocale = 1;
            if (strcmp(unitname,mylocale))  /* locales don't match           */
              wronglocale = 1;
          }
          continue;
        } 
        else if (!strcmp(unitname, "endlocale")){
          if (!inlocale)
            readerror(errfile, 
                      "%s: unmatched !endlocale on line %d of '%s'\n",
                      progname, linenum, file);
          wronglocale = 0;
          inlocale = 0;
          continue;
        }
        else if (!strcmp(unitname, "utf8")){
          if (in_utf8)
            readerror(errfile,"%s: nested utf8 not allowed, line %d of '%s'\n",
                              progname, linenum, file);
          else in_utf8 = 1;
          continue;
        } 
        else if (!strcmp(unitname, "endutf8")){
          if (!in_utf8)
            readerror(errfile,"%s: unmatched !endutf8 on line %d of '%s'\n",
                      progname, linenum, file);
          in_utf8 = 0;
          continue;
        }
        if (in_utf8 && !utf8mode) continue;
        if (wronglocale || wrongvar) continue;

        if (!strcmp(unitname,"prompt")){
          unitname = strtok(0,"");     /* Rest of the line */
          if (promptprefix)
            free(promptprefix);
          if (!unitname)
            promptprefix=0;
          else
            promptprefix = dupstr(unitname);
          continue;
        }
        if (!strcmp(unitname,"message")){
          unitname = strtok(0,"");     /* Rest of the line */
          if (!flags.quiet){
            if (unitname) logputs(unitname);
            logputchar('\n');
          }
          continue; 
        }
        else if (!strcmp(unitname,"set")) {
          unitname = strtok(0," ");
          unitdef = strtok(0," ");
          if (!unitname)
            readerror(errfile, 
                      "%s: no variable name specified on line %d of '%s'\n",
                      progname, linenum, file);
          else if (!unitdef)
            readerror(errfile, 
                      "%s: no value specified on line %d of '%s'\n",
                      progname, linenum, file);
          else 
            setenv(unitname, unitdef, 0);
          continue;
        }
        else if (!strcmp(unitname,"unitlist")){
          splitline(0,&unitname, &unitdef); /* 0 continues strtok call */
          if (!unitname || !unitdef)
            readerror(errfile,VAGUE_ERR);
          else {
            if (newalias(unitname, unitdef, linenum, permfile, errfile))
              goterr = 1;
          }
          continue;
        }
        else if (!strcmp(unitname, "include")){
          if (depth>MAXINCLUDE){
            readerror(errfile, 
                "%s: max include depth of %d exceeded in file '%s' line %d\n",
                progname, MAXINCLUDE, file, linenum);
          } else {
            int readerr;
            char *includefile;

            unitname = strtok(0, " "); 
            if (!unitname){
              readerror(errfile,
                        "%s: missing include filename on line %d of '%s'\n",
                        progname, linenum, file);
              continue;
            }
            includefile = mymalloc(strlen(file)+strlen(unitname)+1, "(readunits)");
            if (isfullpath(unitname))
              strcpy(includefile,unitname);
            else {
              strcpy(includefile,file);
              strcpy(pathend(includefile), unitname);
            }

            readerr = readunits(includefile, errfile, unitcount, prefixcount, 
                                funccount, depth+1);
            if (readerr == E_MEMORY){
              fclose(unitfile);
              free(line);
              free(includefile);
              return readerr;
            }
            if (readerr == E_FILE) {
              readerror(errfile, "%s:   file was included at line %d of file '%s'\n", progname,linenum, file);
            }
            
            if (readerr)
              goterr = 1;
            free(includefile);
          }
        } else                             /* not a valid command */
          readerror(errfile,VAGUE_ERR);
        continue;
      } 
      if (in_utf8 && !utf8mode) continue;
      if (wronglocale || wrongvar) continue;
      splitline(line, &unitname, &unitdef);
      if (!unitname) continue;
      if (!unitdef){
        readerror(errfile,
              "%s: unit '%s' lacks a definition at line %d of '%s'\n",
                  progname, unitname, linenum, file);
        continue;
      }

      if (*unitname == REDEFCHAR){
        unitname++;
        redefinition=1;
      } else
        redefinition=0;

      if (lastchar(unitname) == '-'){      /* it's a prefix definition */
        if (newprefix(unitname,unitdef,&locprefixcount,linenum,
                      permfile,errfile,redefinition))
          goterr=1;
      }
      else if (strchr(unitname,'[')){     /* table definition  */
        retcode=newtable(unitname,unitdef,&locfunccount,linenum,
                         permfile,errfile,redefinition);
        if (retcode){
          if (retcode != E_BADFILE){
            fclose(unitfile);
            free(line);
            return retcode;
          }
          goterr=1;
        }
      }
      else if (strchr(unitname,'(')){     /* function definition */
        if (newfunction(unitname,unitdef,&locfunccount,linenum,
                        permfile,errfile,redefinition))
          goterr = 1;
      }
      else {                              /* ordinary unit definition */
        if (newunit(unitname,unitdef,&locunitcount,linenum,permfile,errfile,redefinition))
          goterr = 1;
      }
   }
   fclose(unitfile);
   free(line);
   if (unitcount)
     *unitcount+=locunitcount;
   if (prefixcount)
     *prefixcount+=locprefixcount;
   if (funccount)
     *funccount+=locfunccount;
   if (goterr)
     return E_BADFILE;
    else { 
	  hasLoadedUnits += 1;
	  return 0;
	};
}

/* Initialize a unit to be equal to 1. */

void
initializeunit(struct unittype *theunit)
{
   theunit->factor = 1.0;
   theunit->numerator[0] = theunit->denominator[0] = NULL;
}


/* Free a unit: frees all the strings used in the unit structure.
   Does not free the unit structure itself.  */

void
freeunit(struct unittype *theunit)
{
   char **ptr;

   for(ptr = theunit->numerator; *ptr; ptr++)
     if (*ptr != NULLUNIT) free(*ptr);
   for(ptr = theunit->denominator; *ptr; ptr++)
     if (*ptr != NULLUNIT) free(*ptr);

   /* protect against repeated calls to freeunit() */

   theunit->numerator[0] = 0;  
   theunit->denominator[0] = 0;
}


/* Print out a unit  */

void
showunit(struct unittype *theunit)
{
   char **ptr;
   int printedslash;
   int counter = 1;

   logprintf(num_format.format, theunit->factor);

   for (ptr = theunit->numerator; *ptr; ptr++) {
      if (ptr > theunit->numerator && **ptr &&
          !strcmp(*ptr, *(ptr - 1)))
         counter++;
      else {
         if (counter > 1)
           logprintf("%s%d", powerstring, counter);
         if (**ptr)
           logprintf(" %s", *ptr);
         counter = 1;
      }
   }
   if (counter > 1)
     logprintf("%s%d", powerstring, counter);
   counter = 1;
   printedslash = 0;
   for (ptr = theunit->denominator; *ptr; ptr++) {
      if (ptr > theunit->denominator && **ptr &&
          !strcmp(*ptr, *(ptr - 1)))
         counter++;
      else {
         if (counter > 1)
           logprintf("%s%d", powerstring, counter);
         if (**ptr) {
            if (!printedslash)
              logprintf(" /");
            printedslash = 1;
            logprintf(" %s", *ptr);
         }
         counter = 1;
      }
   }
   if (counter > 1)
     logprintf("%s%d", powerstring, counter);
}


/* qsort comparison function */

int
compare(const void *item1, const void *item2)
{
   return strcmp(*(char **) item1, *(char **) item2);
}

/* Sort numerator and denominator of a unit so we can compare different
   units */

void
sortunit(struct unittype *theunit)
{
   char **ptr;
   int count;

   for (count = 0, ptr = theunit->numerator; *ptr; ptr++, count++);
   qsort(theunit->numerator, count, sizeof(char *), compare);
   for (count = 0, ptr = theunit->denominator; *ptr; ptr++, count++);
   qsort(theunit->denominator, count, sizeof(char *), compare);
}


/* Cancels duplicate units that appear in the numerator and
   denominator.  The input unit must be sorted. */

void
cancelunit(struct unittype *theunit)
{
   char **den, **num;
   int comp;

   den = theunit->denominator;
   num = theunit->numerator;

   while (*num && *den) { 
      comp = strcmp(*den, *num);
      if (!comp) { /* units match, so cancel them */
         if (*den!=NULLUNIT) free(*den);
         if (*num!=NULLUNIT) free(*num);  
         *den++ = NULLUNIT;
         *num++ = NULLUNIT;
      } else if (comp < 0) /* Move up whichever pointer is alphabetically */
         den++;            /* behind to look for future matches */
      else
         num++;
   }
}


/*
   Looks up the definition for the specified unit including prefix processing
   and plural removal.

   Returns a pointer to the definition or a null pointer
   if the specified unit does not appear in the units table.

   Sometimes the returned pointer will be a pointer to the special
   buffer created to hold the data.  This buffer grows as needed during 
   program execution.  

   Note that if you pass the output of lookupunit() back into the function
   again you will get correct results, but the data you passed in may get
   clobbered if it happened to be the internal buffer.  
*/

static int bufsize=0;
static char *buffer;  /* buffer for lookupunit answers with prefixes */


/* 
  Plural rules for english: add -s
  after x, sh, ch, ss   add -es
  -y becomes -ies except after a vowel when you just add -s as usual
*/
  

char *
lookupunit(char *unit,int prefixok)
{
   char *copy;
   struct prefixlist *pfxptr;
   struct unitlist *uptr;

   if ((uptr = ulookup(unit)))
      return uptr->value;

   if (strwidth(unit)>2 && lastchar(unit) == 's') {
      copy = dupstr(unit);
      lastchar(copy) = 0;
      if (lookupunit(copy,prefixok)){
         while(strlen(copy)+1 > bufsize) {
            growbuffer(&buffer, &bufsize);
         }
         strcpy(buffer, copy);  /* Note: returning looked up result seems   */
         free(copy);            /*   better but it causes problems when it  */
         return buffer;         /*   contains PRIMITIVECHAR.                */
      }
      if (strlen(copy)>2 && lastchar(copy) == 'e') {
         lastchar(copy) = 0;
         if (lookupunit(copy,prefixok)){
            while (strlen(copy)+1 > bufsize) {
               growbuffer(&buffer,&bufsize);
            }
            strcpy(buffer,copy);
            free(copy);
            return buffer;
         }
      }
      if (strlen(copy)>2 && lastchar(copy) == 'i') {
         lastchar(copy) = 'y';
         if (lookupunit(copy,prefixok)){
            while (strlen(copy)+1 > bufsize) {
               growbuffer(&buffer,&bufsize);
            }
            strcpy(buffer,copy);
            free(copy);
            return buffer;
         }
      }
      free(copy);
   }
   if (prefixok && (pfxptr = plookup(unit))) {
      copy = unit + pfxptr->len;
      if (emptystr(copy) || lookupunit(copy,0)) {
         char *tempbuf;
         while (strlen(pfxptr->value)+strlen(copy)+2 > bufsize){
            growbuffer(&buffer, &bufsize);
         }
         tempbuf = dupstr(copy);   /* copy might point into buffer */
         strcpy(buffer, pfxptr->value);
         strcat(buffer, " ");
         strcat(buffer, tempbuf);
         free(tempbuf);
         return buffer;
      }
   }
   return 0;
}


/* Points entries of product[] to the strings stored in tomove[].  
   Leaves tomove pointing to a list of NULLUNITS.  */

int
moveproduct(char *product[], char *tomove[])
{
   char **dest, **src;

   dest=product;
   for(src = tomove; *src; src++){
     if (*src == NULLUNIT) continue;
     for(; *dest && *dest != NULLUNIT; dest++);
     if (dest - product >= MAXSUBUNITS - 1) {
       return E_PRODOVERFLOW;
     }
     if (!*dest)
        *(dest + 1) = 0;
     *dest = *src;
     *src=NULLUNIT;
   }
   return 0;
}

/* 
   Make a copy of a product list.  Note that no error checking is done
   for overflowing the product list because it is assumed that the
   source list doesn't overflow, so the destination list shouldn't
   overflow either.  (This assumption could be false if the
   destination is not actually at the start of a product buffer.) 
*/

void
copyproduct(char **dest, char **source)
{
   for(;*source;source++,dest++) {
     if (*source==NULLUNIT)
       *dest = NULLUNIT;
     else
       *dest=dupstr(*source);
   }
   *dest=0;
}

/* Make a copy of a unit */ 

void
unitcopy(struct unittype *dest, struct unittype *source)
{
  dest->factor = source->factor;
  copyproduct(dest->numerator, source->numerator);
  copyproduct(dest->denominator, source->denominator);
}


/* Multiply left by right.  In the process, all of the units are
   deleted from right (but it is not freed) */

int 
multunit(struct unittype *left, struct unittype *right)
{
  int myerr;
  left->factor *= right->factor;
  myerr = moveproduct(left->numerator, right->numerator);
  if (!myerr)
    myerr = moveproduct(left->denominator, right->denominator);
  return myerr;
}

int 
divunit(struct unittype *left, struct unittype *right)
{
  int myerr;
  left->factor /= right->factor;
  myerr = moveproduct(left->numerator, right->denominator);
  if (!myerr)
    myerr = moveproduct(left->denominator, right->numerator);
  return myerr;
}


/*
   reduces a product of symbolic units to primitive units.
   The three low bits are used to return flags:

   bit 0 set if reductions were performed without error.
   bit 1 set if no reductions are performed.
   bit 2 set if an unknown unit is discovered.

   Return values from multiple calls will be ORed together later.
 */

#define DIDREDUCTION (1<<0)
#define NOREDUCTION  (1<<1)
#define REDUCTIONERROR        (1<<2)

int
reduceproduct(struct unittype *theunit, int flip)
{

   char *toadd;
   char **product;
   int didsomething = NOREDUCTION;
   struct unittype newunit;
   int ret;

   if (flip)
      product = theunit->denominator;
   else
      product = theunit->numerator;

   for (; *product; product++) {
      for (;;) {
         if (!strlen(*product))
            break;
         toadd = lookupunit(*product,1);
         if (!toadd) {
            if (!irreducible)
              irreducible = dupstr(*product);
            return REDUCTIONERROR;
         }
         if (strchr(toadd, PRIMITIVECHAR))
            break;
         didsomething = DIDREDUCTION;
         if (*product != NULLUNIT) {
            free(*product);
            *product = NULLUNIT;
         }
         if (parseunit(&newunit, toadd, 0, 0))
            return REDUCTIONERROR;
         if (flip) ret=divunit(theunit,&newunit);
         else ret=multunit(theunit,&newunit);
         freeunit(&newunit);
         if (ret) 
            return REDUCTIONERROR;
      }
   }
   return didsomething;
}


#if 0
void showunitdetail(struct unittype *foo)
{
  char **ptr;

  printf("%.17g ", foo->factor);

  for(ptr=foo->numerator;*ptr;ptr++)
    if (*ptr==NULLUNIT) printf("NULL ");
    else printf("`%s' ", *ptr);
  printf(" / ");
  for(ptr=foo->denominator;*ptr;ptr++)
    if (*ptr==NULLUNIT) printf("NULL ");
    else printf("`%s' ", *ptr);
  putchar('\n');
}
#endif


/*
   Reduces numerator and denominator of the specified unit.
   Returns 0 on success, or 1 on unknown unit error.
 */

int
reduceunit(struct unittype *theunit)
{
   int ret;

   if (irreducible)
     free(irreducible);
   irreducible=0;
   ret = DIDREDUCTION;

   /* Keep calling reduceproduct until it doesn't do anything */

   while (ret & DIDREDUCTION) {
      ret = reduceproduct(theunit, 0);
      if (!(ret & REDUCTIONERROR))
        ret |= reduceproduct(theunit, 1);
      if (ret & REDUCTIONERROR){
         if (irreducible) 
           return E_UNKNOWNUNIT;
         else
           return E_REDUCE;
      }
   }
   return 0;
}

/* 
   Returns one if the argument unit is defined in the data file as a   
  dimensionless unit.  This is determined by comparing its definition to
  the string NODIM.  
*/

int
ignore_dimless(char *name)
{
  struct unitlist *ul;
  if (!name) 
    return 0;
  ul = ulookup(name);
  if (ul && !strcmp(ul->value, NODIM))
    return 1;
  return 0;
}

int 
ignore_nothing(char *name)
{
  return 0;
}


int
ignore_primitive(char *name)
{
  struct unitlist *ul;
  if (!name) 
    return 0;
  ul = ulookup(name);
  if (ul && strchr(ul->value, PRIMITIVECHAR))
    return 1;
  return 0;
}


/* 
   Compare two product lists, return zero if they match and one if
   they do not match.  They may contain embedded NULLUNITs which are
   ignored in the comparison. Units defined as NODIM are also ignored
   in the comparison.
*/

int
compareproducts(char **one, char **two, int (*isdimless)(char *name))
{
   int oneblank, twoblank;
   while (*one || *two) {
      oneblank = (*one==NULLUNIT) || isdimless(*one);    
      twoblank = (*two==NULLUNIT) || isdimless(*two);    
      if (!*one && !twoblank)
         return 1;
      if (!*two && !oneblank)
         return 1;
      if (oneblank)
         one++;
      else if (twoblank)
         two++;
      else if (strcmp(*one, *two))
         return 1;
      else
         one++, two++;
   }
   return 0;
}


/* Return zero if units are compatible, nonzero otherwise.  The units
   must be reduced, sorted and canceled for this to work.  */

int
compareunits(struct unittype *first, struct unittype *second, 
             int (*isdimless)(char *name))
{
   return
      compareproducts(first->numerator, second->numerator, isdimless) ||
      compareproducts(first->denominator, second->denominator, isdimless);
}


/* Reduce a unit as much as possible */

int
completereduce(struct unittype *unit)
{
   int err;

   if ((err=reduceunit(unit)))
     return err;
   sortunit(unit);
   cancelunit(unit);
   return 0;
}


/* Raise theunit to the specified power.  This function does not fill
   in NULLUNIT gaps, which could be considered a deficiency. */

int
expunit(struct unittype *theunit, int  power)
{
  char **numptr, **denptr;
  double thefactor;
  int i, uind, denlen, numlen;

  if (power==0){
    freeunit(theunit);
    initializeunit(theunit);
    return 0;
  }
  numlen=0;
  for(numptr=theunit->numerator;*numptr;numptr++) numlen++;
  denlen=0;
  for(denptr=theunit->denominator;*denptr;denptr++) denlen++;
  thefactor=theunit->factor;
  for(i=1;i<power;i++){
    theunit->factor *= thefactor;
    for(uind=0;uind<numlen;uind++){
      if (theunit->numerator[uind]!=NULLUNIT){
        if (numptr-theunit->numerator>=MAXSUBUNITS-1) {
           *numptr=*denptr=0;
           return E_PRODOVERFLOW;
        }
        *numptr++=dupstr(theunit->numerator[uind]);
      }
    }
    for(uind=0;uind<denlen;uind++){
      if (theunit->denominator[uind]!=NULLUNIT){
        *denptr++=dupstr(theunit->denominator[uind]);
        if (denptr-theunit->denominator>=MAXSUBUNITS-1) {
          *numptr=*denptr=0;
          return E_PRODOVERFLOW;
        }
      }   
    }
  }
  *numptr=0;
  *denptr=0;
  return 0;
}


int
unit2num(struct unittype *input)
{
  struct unittype one;
  int err;

  initializeunit(&one);
  if ((err=completereduce(input)))
    return err;
  if (compareunits(input,&one,ignore_nothing))
    return E_NOTANUMBER;
  freeunit(input);
  return 0;
}


int
unitdimless(struct unittype *input)
{
  struct unittype one;
  initializeunit(&one);
  if (compareunits(input, &one, ignore_dimless))
    return 0;
  freeunit(input);  /* Eliminate dimensionless units from list */
  return 1;
}
                       

/* 
   The unitroot function takes the nth root of an input unit which has
   been completely reduced.  Returns 1 if the unit is not a power of n. 
   Input data can contain NULLUNITs.  
*/

int 
subunitroot(int n,char *current[], char *out[])
{
  char **ptr;
  int count=0;

  while(*current==NULLUNIT) current++;  /* skip past NULLUNIT entries */
  ptr=current;
  while(*ptr){
    while(*ptr){
      if (*ptr!=NULLUNIT){
        if (strcmp(*current,*ptr)) break;
        count++;
      }
      ptr++;
    }
    if (count % n != 0){  /* If not dimensionless generate error, otherwise */
      if (!ignore_dimless(*current))    /* just skip over it */
        return E_NOTROOT;
    } else {
      for(count /= n;count>0;count--) *(out++) = dupstr(*current);
    }
    current=ptr;
  }
  *out = 0;
  return 0;
}
  

int 
rootunit(struct unittype *inunit,int n)
{
   struct unittype outunit;
   int err;

   initializeunit(&outunit);
   if ((err=completereduce(inunit)))
     return err;
   /* Roots of negative numbers fail in pow(), even odd roots */
   if (inunit->factor < 0)
     return E_NOTROOT;
   outunit.factor = pow(inunit->factor,1.0/(double)n);
   if ((err = subunitroot(n, inunit->numerator, outunit.numerator)))
     return err;
   if ((err = subunitroot(n, inunit->denominator, outunit.denominator)))
     return err;
   freeunit(inunit);
   initializeunit(inunit);
   return multunit(inunit,&outunit);
}


/* Compute the inverse of a unit (1/theunit) */

void
invertunit(struct unittype *theunit)
{
  char **ptr, *swap;
  int numlen, length, ind;

  theunit->factor = 1.0/theunit->factor;  
  length=numlen=0;
  for(ptr=theunit->denominator;*ptr;ptr++,length++);
  for(ptr=theunit->numerator;*ptr;ptr++,numlen++);
  if (numlen>length)
    length=numlen;
  for(ind=0;ind<=length;ind++){
    swap = theunit->numerator[ind];
    theunit->numerator[ind] = theunit->denominator[ind];
    theunit->denominator[ind] = swap;
  }
}


int
float2rat(double y, int *p, int *q)
{
  int coef[20];           /* How long does this buffer need to be? */
  int i,termcount,saveq;
  double fracpart,x;

  /* Compute continued fraction approximation */

  x=y;
  termcount=0;
  while(1){
    coef[termcount] = (int) floor(x);
    fracpart = x-coef[termcount];
    if (fracpart < .001 || termcount==19) break;
    x = 1/fracpart;
    termcount++;
  }

  /* Compress continued fraction into rational p/q */
 
  *p=0;
  *q=1;
  for(i=termcount;i>=1;i--) {
    saveq=*q;
    *q = coef[i] * *q + *p;
    *p = saveq;
  }
  *p+=*q*coef[0];
  return *q<MAXSUBUNITS && fabs((double)*p / (double)*q - y) < DBL_EPSILON;
}


/* Raise a unit to a power */

int
unitpower(struct unittype *base, struct unittype *exponent)
{
  int errcode, p, q;

  errcode = unit2num(exponent);
  if (errcode == E_NOTANUMBER)
    return E_DIMEXPONENT;
  if (errcode) 
    return errcode;
  errcode = unit2num(base);
  if (!errcode){                     /* Exponent base is dimensionless */
    base->factor = pow(base->factor,exponent->factor);
    if (errno)
      return E_FUNC;
  }
  else if (errcode==E_NOTANUMBER) {          /* Base not dimensionless */
    if (!float2rat(exponent->factor,&p,&q)){ /* Exponent must be rational */
      if (unitdimless(base)){
        base->factor = pow(base->factor,exponent->factor);
        if (errno)
          return E_FUNC;
      }
      else
        return E_IRRATIONAL_EXPONENT;
    } else {
      if (q!=1) {
        errcode = rootunit(base, q);
        if (errcode == E_NOTROOT) 
          return E_BASE_NOTROOT;
        if (errcode)
          return errcode;
      }
      errcode = expunit(base, abs(p));
      if (errcode)
        return errcode;
      if (p<0)
        invertunit(base);
    }
  } 
  else return errcode;
  return 0;
}


/* Old units program would give message about what each operand
   reduced to, showing that they weren't conformable.  Can this
   be achieved here?  */

int
addunit(struct unittype *unita, struct unittype *unitb)
{
  int err;

  if ((err=completereduce(unita)))
    return err;
  if ((err=completereduce(unitb)))
    return err;
  if (compareunits(unita,unitb,ignore_nothing))
    return E_BADSUM;
  unita->factor += unitb->factor;
  freeunit(unitb);
  return 0;
}


double
linearinterp(double  a, double b, double aval, double bval, double c)
{
  double lambda;

  lambda = (b-c)/(b-a);
  return lambda*aval + (1-lambda)*bval;
}


/* evaluate a user function */

#define INVERSE 1
#define FUNCTION 0
#define ALLERR 1
#define NORMALERR 0

int
evalfunc(struct unittype *theunit, struct func *infunc, int inverse, 
         int allerrors)
{
   struct unittype result;
   struct functype *thefunc;
   int err;
   double value;
   int foundit, count;
   struct unittype *save_value;
   char *save_function;

   if (infunc->table) {  /* Tables are short, so use dumb search algorithm */
     err = parseunit(&result, infunc->tableunit, 0, 0);
     if (err)
       return E_BADFUNCDIMEN;
     if (inverse){
       err = divunit(theunit, &result);
       if (err)
         return err;
       err = unit2num(theunit);
       if (err==E_NOTANUMBER)
         return E_BADFUNCARG;
       if (err)
         return err;
       value = theunit->factor;
       foundit=0;
       for(count=0;count<infunc->tablelen-1;count++)
         if ((infunc->table[count].value<=value &&
              value<=infunc->table[count+1].value) ||
             (infunc->table[count+1].value<=value &&
              value<=infunc->table[count].value)){
           foundit=1;
           value  = linearinterp(infunc->table[count].value,
                                 infunc->table[count+1].value,
                                 infunc->table[count].location,
                                 infunc->table[count+1].location,
                                 value);
           break;
         }
       if (!foundit)
         return E_NOTINDOMAIN;
       freeunit(&result);
       freeunit(theunit);
       theunit->factor = value;
       return 0;
     } else {
       err=unit2num(theunit);
       if (err)
         return err;
       value=theunit->factor;
       foundit=0;
       for(count=0;count<infunc->tablelen-1;count++)
         if (infunc->table[count].location<=value &&
             value<=infunc->table[count+1].location){
           foundit=1;
           value =  linearinterp(infunc->table[count].location,
                        infunc->table[count+1].location,
                        infunc->table[count].value,
                        infunc->table[count+1].value,
                        value);
           break;
         } 
       if (!foundit)
         return E_NOTINDOMAIN;
       result.factor *= value;
     }
   } else {  /* it's a function */
     if (inverse){
       thefunc=&(infunc->inverse);
       if (!thefunc->def)
         return E_NOINVERSE;
     }
     else
       thefunc=&(infunc->forward);
     err = completereduce(theunit);
     if (err)
       return err;
     if (thefunc->dimen){
       err = parseunit(&result, thefunc->dimen, 0, 0);
       if (err)
         return E_BADFUNCDIMEN;
       err = completereduce(&result);
       if (err)
         return E_BADFUNCDIMEN;
       if (compareunits(&result, theunit, ignore_nothing))
         return E_BADFUNCARG;
       value = theunit->factor/result.factor;
     } else 
       value = theunit->factor;
     if (thefunc->domain_max && 
         (value > *thefunc->domain_max || 
          (thefunc->domain_max_open && value == *thefunc->domain_max)))
       return E_NOTINDOMAIN;
     if (thefunc->domain_min && 
         (value < *thefunc->domain_min ||
          (thefunc->domain_min_open && value == *thefunc->domain_min)))
       return E_NOTINDOMAIN;
     save_value = parameter_value;
     save_function = function_parameter;
     parameter_value = theunit;
     function_parameter = thefunc->param;
     err = parseunit(&result, thefunc->def, 0,0);
     function_parameter = save_function;
     parameter_value = save_value;
     if (err && (allerrors == ALLERR || err==E_PARSEMEM || err==E_PRODOVERFLOW 
                 || err==E_NOTROOT || err==E_BADFUNCTYPE))
       return err;
     if (err)
       return E_FUNARGDEF;
   }
   freeunit(theunit);
   initializeunit(theunit);
   multunit(theunit, &result);
   return 0;
}


/* 
   If the given character string has only one unit name in it, then print out
   the rule for that unit.  In any case, print out the reduced form for
   the unit.
*/

void
showdefinition(char *unitstr, struct unittype *theunit)
{
  logputs(deftext);
  while((unitstr = lookupunit(unitstr,1))
        && strspn(unitstr,digits) != strlen(unitstr)
        && !strchr(unitstr,PRIMITIVECHAR)) {
    tightprint(stdout,unitstr);
    if (logfile) tightprint(logfile,unitstr);
    logputs(" = ");
  } 
  showunit(theunit);
  logputchar('\n');
}


void 
showfunction(struct functype *func)
{
  struct unittype unit;
  int not_dimensionless, i;

  if (!func->def) {
    logputs(" is undefined");
    return;
  }

  if (func->dimen){                       /* coverity[check_return] */
    parseunit(&unit,func->dimen,0,0);     /* unit2num returns 0 for */
    not_dimensionless = unit2num(&unit);  /* dimensionless units    */
  } 
    
  logprintf("(%s) = %s", func->param, func->def);
  if (func->domain_min || func->domain_max){
    logputchar('\n');
    for(i=strwidth(deftext);i;i--) logputchar(' ');
    logputs("defined for ");
    if (func->domain_min && func->domain_max) {
      logprintf(num_format.format, *func->domain_min);
      if (func->dimen && (not_dimensionless || unit.factor != 1)){
        if (isdecimal(*func->dimen))
          logputs(" *");
        logprintf(" %s",func->dimen);
      }
      logputs(func->domain_min_open?" < ":" <= ");
    }
    logputs(func->param);
    if (func->domain_max){
      logputs(func->domain_max_open?" < ":" <= ");
      logprintf(num_format.format, *func->domain_max);
    }
    else {
      logputs(func->domain_min_open?" > ":" >= ");
      logprintf(num_format.format, *func->domain_min);
    }
    if (func->dimen && (not_dimensionless || unit.factor != 1)){
      if (isdecimal(*func->dimen))
        logputs(" *");
      logprintf(" %s",func->dimen);
    }
    if (!func->dimen) logputs(" (any units)");
  } else if (func->dimen){
    logputchar('\n');
    for(i=strwidth(deftext);i;i--) logputchar(' ');
    if (not_dimensionless) 
      logprintf("%s has units %s",func->param, func->dimen);
    else
      logprintf("%s is dimensionless",func->param);
  }    
  logputchar('\n');
}

void
showtable(struct func *fun, int inverse)
{
  int i;

  logprintf("%sinterpolated table with points\n",deftext);
  if (inverse){
    int reverse, j;
    reverse = (fun->table[0].value > fun->table[fun->tablelen-1].value);
    for(i=0;i<fun->tablelen;i++){
      if (reverse) j = fun->tablelen-i-1;
      else j=i;
      if (flags.verbose>0)
        logputs("\t\t    ");
      logprintf("~%s(", fun->name);
      logprintf(num_format.format, fun->table[j].value);
      if (isdecimal(fun->tableunit[0]))
        logputs(" *");
      logprintf(" %s",fun->tableunit);
      logputs(") = ");
      logprintf(num_format.format, fun->table[j].location);
      logputchar('\n');
    }
  } else {
    for(i=0;i<fun->tablelen;i++){
      if (flags.verbose>0)
        logputs("\t\t    ");
      logprintf("%s(", fun->name);
      logprintf(num_format.format, fun->table[i].location);
      logputs(") = ");
      logprintf(num_format.format, fun->table[i].value);
      if (isdecimal(fun->tableunit[0]))
        logputs(" *");
      logprintf(" %s\n",fun->tableunit);
    }
  }
}


void
showfuncdefinition(struct func *fun, int inverse)
{
  if (fun->table)  /* It's a table */
    showtable(fun, inverse);
  else { 
    logprintf("%s%s%s", deftext,inverse?"~":"", fun->name);
    if (inverse)
      showfunction(&fun->inverse);
    else
      showfunction(&fun->forward);
  }
}


void
showunitlistdef(struct wantalias *alias)
{
  logprintf("%sunit list, ",deftext);
  tightprint(stdout,alias->definition);
  if (logfile) tightprint(logfile,alias->definition);
  logputchar('\n');
}


/* Show conversion to a function.  Input unit 'have' is replaced by the 
   function inverse and completely reduced. */

int
showfunc(char *havestr, struct unittype *have, struct func *fun)
{
   int err;
   char *dimen;

   err = evalfunc(have, fun, INVERSE, NORMALERR);
   if (!err)
     err = completereduce(have);
   if (err) {
     if (err==E_BADFUNCARG){
       logputs("conformability error");
       if (fun->table)
         dimen = fun->tableunit;
       else if (fun->inverse.dimen)
         dimen = fun->inverse.dimen;
       else 
         dimen = 0;
       if (!dimen)
         logputchar('\n');
       else {
         struct unittype want;
         
         if (emptystr(dimen))
           dimen = "1";
         logprintf(": conversion requires dimensions of '%s'\n",dimen);
         if (flags.verbose==2) logprintf("\t%s = ",havestr);
         else if (flags.verbose==1) logputchar('\t');
         showunit(have);
         if (flags.verbose==2) logprintf("\n\t%s = ",dimen);
         else if (flags.verbose==1) logprintf("\n\t");
         else logputchar('\n');            /* coverity[check_return] */
         parseunit(&want, dimen, 0, 0);    /* coverity[check_return] */
         completereduce(&want);         /* dimen was already checked for */
         showunit(&want);               /* errors so no need to check here */
         logputchar('\n');
         }
     } else if (err==E_NOTINDOMAIN)
       logprintf("Value '%s' is not in the function's range\n",havestr);
     else
       logputs("Function evaluation error (bad function definition)\n");
     return 1;
   }
   if (flags.verbose==2)
     logprintf("\t%s = %s(", havestr, fun->inverse.param);
   else if (flags.verbose==1) 
     logputchar('\t');
   showunit(have);
   if (flags.verbose==2)
     logputchar(')');
   logputchar('\n');
   return 0;
}

/* Print the conformability error message */

void
showconformabilityerr(char *havestr,struct unittype *have,
                      char *wantstr,struct unittype *want)
{
  logputs("conformability error\n");
  if (flags.verbose==2)
    logprintf("\t%s = ",havestr);
  else if (flags.verbose==1)
    logputchar('\t');
  showunit(have);
  if (flags.verbose==2)
    logprintf("\n\t%s = ",wantstr);
  else if (flags.verbose==1)
    logputs("\n\t");
  else
    logputchar('\n');
  showunit(want);
  logputchar('\n');
} /* end showconformabilityerr */



/*
  determine whether a unit string begins with a fraction; assume it
  does if it starts with an integer, '|', and another integer
*/
int
isfract(const char *unitstr)
{
  char *enddouble=0, *endlong=0;

  while (isdigit(*unitstr))
    unitstr++;
  if (*unitstr++ == '|') {
    (void)strtod(unitstr, &enddouble);
    (void)strtol(unitstr, &endlong, 10);
    if (enddouble == endlong)
      return 1;
  }
  return 0;
}

int
checksigdigits(char *arg)
{
  int errors, ival;
  char *nonum;

  errors = 0;

  if (!strcmp(arg, "max"))
    num_format.precision = MAXPRECISION;
  else {
    ival = (int) strtol(arg, &nonum, 10);
    if (!emptystr(nonum)) {
      fprintf(stderr,
      "%s: invalid significant digits (%s)--integer value or 'max' required\n",
              progname, arg);
      errors++;
    }
    else if (ival <= 0) {
      fprintf(stderr, "%s: number of significant digits must be positive\n",
              progname);
      errors++;
    }
    else if (ival > MAXPRECISION) {
      fprintf(stderr,
           "%s: too many significant digits (%d)--using maximum value (%d)\n",
           progname, ival, MAXPRECISION);
      num_format.precision = MAXPRECISION;
    }
    else
      num_format.precision = ival;
  }
  if (errors)
    return -1;
  else
    return 0;
}

/* set output number format specification from significant digits and type */

int
setnumformat()
{
  size_t len;

  if (strchr("Ee", num_format.type))
    num_format.precision--;

  len = 4;      /* %, decimal point, type, terminating NUL */
  if (num_format.precision > 0)
    len += (size_t) floor(log10((double) num_format.precision))+1;
  num_format.format = (char *) mymalloc(len, "(setnumformat)");
  sprintf(num_format.format, "%%.%d%c", num_format.precision, num_format.type);
  return 0;
}

/*
    parse and validate the output number format specification and
    extract its type and precision into the num_format structure.
    Returns nonzero for invalid format.
*/

int
parsenumformat()
{
  static char *format_types = NULL;
  static char *format_flags = "+-# 0'";
  static char badflag;
  char *two = "0x1p+1";
  char *valid="ABCDEFGHIJKLMNOPQRSTUVWXYXabcdefghijklmnopqrstuvwxyx.01234567890";
  char *dotptr, *lptr, *nonum, *p;
  char testbuf[80];
  int errors, ndx;

  if (format_types == NULL){
    format_types = (char *) mymalloc(strlen(BASE_FORMATS)+4, "(parsenumformat)");
    strcpy(format_types,BASE_FORMATS);

    /* check for support of type 'F' (MS VS 2012 doesn't have it) */
    sprintf(testbuf, "%.1F", 1.2);
    if (strlen(testbuf) == 3 && testbuf[0] == '1' && testbuf[2] == '2')
      strcat(format_types,"F");

    /* check for support of hexadecimal floating point */
    sprintf(testbuf, "%.0a", 2.0);
    if (!strcmp(testbuf,two))
      strcat(format_types, "aA");

    /* check for support of digit-grouping (') flag */
    sprintf(testbuf, "%'.0f", 1234.0);
    if (strlen(testbuf) > 2 && testbuf[0] == '1' && testbuf[2] == '2')
      badflag = '\0'; /* supported */
    else
      badflag = '\''; /* not supported */
  }

  errors = 0;

  p = num_format.format;

  if (*p != '%') {
    fprintf(stderr, "%s: number format specification must start with '%%'\n",
            progname);
    errors++;
  }
  else if (strrchr(num_format.format, '%') != num_format.format) {
    fprintf(stderr, "%s: only one '%%' allowed in number format specification\n",
            progname);
    errors++;
    p++;
  }
  else
    p++;

  dotptr = strchr(num_format.format, '.');
  if (dotptr && strrchr(num_format.format, '.') != dotptr) {
    fprintf(stderr, "%s: only one '.' allowed in number format specification\n",
            progname);
    errors++;
  }

  /* skip over flags */
  while (*p && strchr(format_flags, *p)) {
    if (*p == badflag) {   /* only if digit-grouping flag (') not supported */
      fprintf(stderr, "%s: digit-grouping flag (') not supported\n", progname);
      errors++;
    }
    p++;
  }

  /* check for type length modifiers, which aren't allowed */
  if ((lptr = strstr(num_format.format, "hh"))
     || (lptr = strstr(num_format.format, "ll"))) {
    fprintf(stderr, "%s: type length modifier (%.2s) not supported\n", progname, lptr);
    errors++;
  }
  else if ((lptr = strpbrk(num_format.format, "hjLltz"))) {
    fprintf(stderr, "%s: type length modifier (%c) not supported\n", progname, lptr[0]);
    errors++;
  }

  /* check for other invalid characters */
  ndx = strspn(p, valid);
  if (ndx < strlen(p)) {
    fprintf(stderr, "%s: invalid character (%c) in width, precision, or type\n",
    progname, p[ndx]);
    errors++;
  }

  if (errors) { /* results of any other checks are likely to be misleading */
    fprintf(stderr, "%s: invalid number format specification (%s)\n", 
                    progname, num_format.format);
    fprintf(stderr, "%s: valid specification is %%[flags][width][.precision]type\n",
            progname);
    return -1;
  }

  /* get width and precision if specified; check precision */
  num_format.width = (int) strtol(p, &nonum, 10);

  if (*nonum == '.'){
    if (isdigit(nonum[1]))
      num_format.precision = (int) strtol(nonum+1, &nonum, 10);
    else {
      num_format.precision = 0;
      nonum++;
    }
  }
  else  /* precision not given--use printf() default */
    num_format.precision = 6;

  /* check for valid format type */
  if (emptystr(nonum)) {
    fprintf(stderr, "%s: missing format type\n", progname);
    errors++;
  }
  else {
    if (strchr(format_types, *nonum)) {
      if (nonum[1]) {
        fprintf(stderr, "%s: invalid character(s) (%s) after format type\n",
                progname, nonum + 1);
        errors++;
      }
      else 
        num_format.type = *nonum;
    }
    else {
      fprintf(stderr,
              "%s: invalid format type (%c)--valid types are [%s]\n",
              progname, *nonum, format_types);
      errors++;
    }
  }

  if (num_format.precision == 0 && 
      (num_format.type == 'G' || num_format.type == 'g'))
    num_format.precision = 1;

  if (errors) {
    fprintf(stderr, "%s: invalid number format specification (%s)\n", 
                    progname, num_format.format);
    fprintf(stderr, "%s: valid specification is %%[flags][width][.precision]type\n",
            progname);
    return -1;
  }
  else
    return 0;
}

/*
   round a number to the lesser of the displayed precision or the
   remaining significant digits; indicate in hasnondigits if a number
   will contain any character other than the digits 0-9 in the current
   display format.
*/

double
round_output(double value, int sigdigits, int *hasnondigits)
{
  int buflen;
  char *buf;
  double rounded;  
  double mult_factor, rdigits;
  int fmt_digits;       /* decimal significant digits in format */

  if (!isfinite(value)){
    if (hasnondigits)
      *hasnondigits = 1;
    return value;
  }

  fmt_digits = num_format.precision;
  switch (num_format.type) {
    case 'A':
    case 'a':
      sigdigits = round(sigdigits * log2(10) / 4);
      fmt_digits++;
      break;
    case 'E':
    case 'e':
      fmt_digits++;
      break;
    case 'F':
    case 'f':
      if (fabs(value) > 0)
        fmt_digits += ceil(log10(fabs(value)));
      break;
  }

  if (sigdigits < fmt_digits)
    rdigits = sigdigits;
  else
    rdigits = fmt_digits;

  /* place all significant digits to the right of the radix */
  if (value != 0)
    rdigits -= ceil(log10(fabs(value)));
  /* then handle like rounding to specified decimal places */
  mult_factor = pow(10.0, rdigits);
  rounded = round(value * mult_factor) / mult_factor;

  /* allow for sign (1), radix (1), exponent (5), E or E formats (1), NUL */
  buflen = num_format.precision + 9;

  if (num_format.width > buflen)
    buflen = num_format.width;

  if (strchr("Ff", num_format.type)) {
    int len=num_format.precision+2;
    if (fabs(value) > 1.0)
      len += (int) floor(log10(fabs(value))) + 1;
    if (len > buflen)
      buflen = len;
  }

  /* allocate space for thousands separators with digit-grouping (') flag */
  /* assume worst case--that all groups are two digits */
  if (strchr(num_format.format, '\'') && strchr("FfGg", num_format.type)) 
    buflen = buflen*3/2;

  buf = (char *) mymalloc(buflen, "(round_output)");
  sprintf(buf, num_format.format, value);

  if (hasnondigits){
    if (strspn(buf, "1234567890") != strlen(buf))
      *hasnondigits = 1;
    else
      *hasnondigits = 0;
  }

  free(buf);
  return rounded;
}

/* 
   Determine significant digits in remainder relative to an original
   value which is assumed to have full double precision.  Returns
   the number of binary or decimal digits depending on the value
   of base, which must be 2 or 10. 
*/

int
getsigdigits(double original, double remainder, int base)
{
  int sigdigits;
  double maxdigits;
  double (*logfunc)(double);

  if (base == 2) {
    maxdigits = DBL_MANT_DIG;
    logfunc = log2;
  }
  else {
    maxdigits = DBL_MANT_DIG * log10(2.0);
    logfunc = log10;
  }

  if (original == 0)
    return floor(maxdigits);
  else if (remainder == 0)
    return 0;

  sigdigits = floor(maxdigits - logfunc(fabs(original/remainder)));

  if (sigdigits < 0)
    sigdigits = 0;

  return sigdigits;
}

/* Rounds a double to the specified number of binary or decimal 
   digits.  The base argument must be 2 or 10.  */

double
round_digits(double value, int digits, int base)
{
  double mult_factor;
  double (*logfunc)(double);

  if (digits == 0)
    return 0.0;

  if (base == 2)
    logfunc = log2;
  else
    logfunc = log10;

  if (value != 0)
    digits -= ceil(logfunc(fabs(value)));

  mult_factor = pow((double) base, digits);

  return round(value*mult_factor)/mult_factor;
}


/* Returns true if the value will display as equal to the reference 
   and if hasnondigits is non-null then return true if the displayed
   output contains any character in "0123456789".  */

int 
displays_as(double reference, double value, int *hasnondigits)
{
  int buflen;
  char *buf;
  double rounded;  

  if (!isfinite(value)){
    if (hasnondigits)
      *hasnondigits = 1;
    return 0;
  }
  
  /* allow for sign (1), radix (1), exponent (5), E or E formats (1), NUL */
  buflen = num_format.precision + 9;

  if (num_format.width > buflen)
    buflen = num_format.width;

  if (strchr("Ff", num_format.type)) {
    int len=num_format.precision+2;
    if (fabs(value) > 1.0)
      len += (int) floor(log10(fabs(value))) + 1;
    if (len > buflen)
      buflen = len;
  }

  /* allocate space for thousands separators with digit-grouping (') flag */
  /* assume worst case--that all groups are two digits */
  if (strchr(num_format.format, '\'') && strchr("FfGg", num_format.type)) 
    buflen = buflen*3/2;

  buf = (char *) mymalloc(buflen, "(round_to_displayed)");
  sprintf(buf, num_format.format, value);

  if (hasnondigits){
    if (strspn(buf, "1234567890") != strlen(buf))
      *hasnondigits = 1;
    else
      *hasnondigits = 0;
  }
  rounded = strtod(buf, NULL);
  free(buf);
  
  return rounded==reference;
}



/* Print the unit in 'unitstr' along with any necessary punctuation.
   The 'value' is the multiplier for the unit.  If printnum is set
   to PRINTNUM then this value is printed, or set it to NOPRINTNUM
   to prevent the value from being printed. 
*/

#define PRINTNUM 1
#define NOPRINTNUM 0

void
showunitname(double value, char *unitstr, int printnum)
{
  int hasnondigits;     /* flag to indicate nondigits in displayed value */
  int is_one;           /* Does the value display as 1? */

  is_one = displays_as(1, value, &hasnondigits);

  if (printnum && !(is_one && isdecimal(*unitstr)))
    logprintf(num_format.format, value);

  if (strpbrk(unitstr, "+-"))   /* show sums and differences of units */
    logprintf(" (%s)", unitstr);   /* in parens */
     /* fractional unit 1|x and multiplier is all digits and not one--   */
     /* no space or asterisk or numerator (3|8 in instead of 3 * 1|8 in) */
  else if (printnum && !flags.showfactor
           && startswith(unitstr,"1|") && isfract(unitstr)
           && !is_one && !hasnondigits)
    logputs(unitstr+1);
     /* multiplier is unity and unit begins with a number--no space or       */
     /* asterisk (multiplier was not shown, and the space was already output)*/
  else if (is_one && isdecimal(*unitstr))
    logputs(unitstr);
     /* unitstr begins with a non-fraction number and multiplier was */
     /* shown--prefix a spaced asterisk */
  else if (isdecimal(unitstr[0]))
    logprintf(" * %s", unitstr);
  else
    logprintf(" %s", unitstr);
} 


/* Show the conversion factors or print the conformability error message */

int
showanswer(char *havestr,struct unittype *have,
           char *wantstr,struct unittype *want)
{
   struct unittype invhave;
   int doingrec;  /* reciprocal conversion? */
   char *right = NULL, *left = NULL;

   doingrec=0;
   if (compareunits(have, want, ignore_dimless)) {
        char **src,**dest;

        invhave.factor=1/have->factor;
        for(src=have->numerator,dest=invhave.denominator;*src;src++,dest++)
           *dest=*src;
        *dest=0;
        for(src=have->denominator,dest=invhave.numerator;*src;src++,dest++)
           *dest=*src;
        *dest=0;
        if (flags.strictconvert || compareunits(&invhave, want, ignore_dimless)){
          showconformabilityerr(havestr, have, wantstr, want);
          return -1;
        }
        if (flags.verbose>0)
          logputchar('\t');
        logputs("reciprocal conversion\n");
        have=&invhave;
        doingrec=1;
   } 
   if (flags.verbose==2) {
     if (!doingrec) 
       left=right="";
     else if (strchr(havestr,'/')) {
       left="1 / (";
       right=")";
     } else {
       left="1 / ";
       right="";
     }
   }   

   /* Print the first line of output. */

   if (flags.verbose==2) 
     logprintf("\t%s%s%s = ",left,havestr,right);
   else if (flags.verbose==1)
     logputs("\t* ");
   if (flags.verbose==2)
     showunitname(have->factor / want->factor, wantstr, PRINTNUM);
   else
     logprintf(num_format.format, have->factor / want->factor);

   /* Print the second line of output. */

   if (!flags.oneline){
     if (flags.verbose==2) 
       logprintf("\n\t%s%s%s = (1 / ",left,havestr,right);
     else if (flags.verbose==1)
       logputs("\n\t/ ");
     else 
       logputchar('\n');
     logprintf(num_format.format, want->factor / have->factor);
     if (flags.verbose==2) {
       logputchar(')');
       showunitname(0,wantstr, NOPRINTNUM); 
     }
   }
   logputchar('\n');
   return 0;
}


/* Checks that the function definition has a valid inverse 
   Prints a message to stdout if function has bad definition or
   invalid inverse. 
*/

#define SIGN(x) ( (x) > 0.0 ?   1 :   \
                ( (x) < 0.0 ? (-1) :  \
                                0 ))

void
checkfunc(struct func *infunc, int verbose)
{
  struct unittype theunit, saveunit;
  struct prefixlist *prefix;
  int err, i;
  double direction;

  if (infunc->skip_error_check){
    if (verbose)
      printf("skipped function '%s'\n", infunc->name);
    return;
  }
  if (verbose)
    printf("doing function '%s'\n", infunc->name);
  if ((prefix=plookup(infunc->name)) 
      && strlen(prefix->name)==strlen(infunc->name))
    printf("Warning: '%s' defined as prefix and function\n",infunc->name);
  if (infunc->table){
    /* Check for valid unit for the table */
    if (parseunit(&theunit, infunc->tableunit, 0, 0) ||
        completereduce(&theunit))
      printf("Table '%s' has invalid units '%s'\n",
             infunc->name, infunc->tableunit);
    freeunit(&theunit);

    /* Check for monotonicity which is needed for unique inverses */
    if (infunc->tablelen<=1){ 
      printf("Table '%s' has only one data point\n", infunc->name);
      return;
    }
    direction = SIGN(infunc->table[1].value -  infunc->table[0].value);
    for(i=2;i<infunc->tablelen;i++)
      if (SIGN(infunc->table[i].value-infunc->table[i-1].value) != direction){
        printf("Table '%s' lacks unique inverse around entry %.8g\n",
               infunc->name, infunc->table[i].location);
        return;
      }
    return;
  }
  if (infunc->forward.dimen){
    if (parseunit(&theunit, infunc->forward.dimen, 0, 0) ||
        completereduce(&theunit)){
      printf("Function '%s' has invalid units '%s'\n", 
             infunc->name, infunc->forward.dimen);
      freeunit(&theunit);
      return;
    }
  } else initializeunit(&theunit); 
  if (infunc->forward.domain_max && infunc->forward.domain_min)
    theunit.factor *= 
             (*infunc->forward.domain_max+*infunc->forward.domain_min)/2;
  else if (infunc->forward.domain_max) 
    theunit.factor = theunit.factor * *infunc->forward.domain_max - 1;
  else if (infunc->forward.domain_min)
    theunit.factor = theunit.factor * *infunc->forward.domain_min + 1;
  else 
    theunit.factor *= 7;   /* Arbitrary choice where we evaluate inverse */
  if (infunc->forward.dimen){
    unitcopy(&saveunit, &theunit);
    err = evalfunc(&theunit, infunc, FUNCTION, ALLERR);
    if (err) {
      printf("Error in definition %s(%s) as '%s':\n",
             infunc->name, infunc->forward.param, infunc->forward.def);
      printf("      %s\n",errormsg[err]);
      freeunit(&theunit);
      freeunit(&saveunit);
      return;
    }
  } else {
#   define MAXPOWERTOCHECK 4
    struct unittype resultunit, arbunit;
    char unittext[9]; 
    double factor;
    int errors[MAXPOWERTOCHECK], errcount=0;
    char *indent;

    strcpy(unittext,"(kg K)^ ");
    factor = theunit.factor;
    initializeunit(&saveunit);
    initializeunit(&resultunit);
    for(i=0;i<MAXPOWERTOCHECK;i++){
      lastchar(unittext) = '0'+i;
      err = parseunit(&arbunit, unittext, 0, 0);
      if (err) initializeunit(&arbunit);
      arbunit.factor = factor;
      unitcopy(&resultunit, &arbunit);
      errors[i] = evalfunc(&resultunit, infunc, FUNCTION, ALLERR); 
      if (errors[i]) errcount++;
      else {
        freeunit(&saveunit);
        freeunit(&theunit);
        unitcopy(&saveunit, &arbunit);
        unitcopy(&theunit, &resultunit);
      }
      freeunit(&resultunit);
      freeunit(&arbunit);
    }
    if (!errors[0] && errcount==3) {
      printf("Warning: function '%s(%s)' defined as '%s'\n",
             infunc->name, infunc->forward.param, infunc->forward.def);
      printf("         appears to require a dimensionless argument, 'units' keyword not given\n");
      indent = "         ";
    }
    else if (errcount==MAXPOWERTOCHECK) {
      printf("Error or missing 'units' keyword in definion %s(%s) as '%s'\n",
             infunc->name, infunc->forward.param, infunc->forward.def);
      indent="      ";
    }
    else if (errcount){
      printf("Warning: function '%s(%s)' defined as '%s'\n",
             infunc->name, infunc->forward.param, infunc->forward.def);
      printf("         failed for some test inputs:\n");
      indent = "         ";
    }
    for(i=0;i<MAXPOWERTOCHECK;i++) 
      if (errors[i]) {
        lastchar(unittext) = '0'+i;
        printf("%s%s(",indent,infunc->name);
        printf(num_format.format, factor);
        printf("%s): %s\n", unittext, errormsg[errors[i]]);
      }
  }
  if (completereduce(&theunit)){
    printf("Definition %s(%s) as '%s' is irreducible\n",
           infunc->name, infunc->forward.param, infunc->forward.def);
    freeunit(&theunit);
    freeunit(&saveunit);
    return;
  }    
  if (!(infunc->inverse.def)){
    printf("Warning: no inverse for function '%s'\n", infunc->name);
    freeunit(&theunit);
    freeunit(&saveunit);
    return;
  }
  err = evalfunc(&theunit, infunc, INVERSE, ALLERR);
  if (err){
    printf("Error in inverse ~%s(%s) as '%s':\n",
           infunc->name,infunc->inverse.param, infunc->inverse.def);
    printf("      %s\n",errormsg[err]);
    freeunit(&theunit);
    freeunit(&saveunit);
    return;
  }
  divunit(&theunit, &saveunit);
  if (unit2num(&theunit) || fabs(theunit.factor-1)>1e-12)
    printf("Inverse is not the inverse for function '%s'\n", infunc->name);
  freeunit(&theunit);
}


struct namedef { 
  char *name;
  char *def;
};
    
#define CONFORMABLE 1
#define TEXTMATCH 2

void 
addtolist(struct unittype *have, char *searchstring, char *rname, char *name, 
               char *def, struct namedef **list, int *listsize, 
               int *maxnamelen, int *count, int searchtype)
{
  struct unittype want;
  int len = 0;
  int keepit = 0;

  if (!name) 
    return;
  if (searchtype==CONFORMABLE){
    initializeunit(&want);
    if (!parseunit(&want, name,0,0) && !completereduce(&want))
      keepit = !compareunits(have,&want,ignore_dimless);
  } else if (searchtype == TEXTMATCH) {
    keepit = (strstr(rname,searchstring) != NULL);
  }
  if (keepit){
    if (*count==*listsize){
      *listsize += 100;
      *list = (struct namedef *) 
        realloc(*list,*listsize*sizeof(struct namedef));
      if (!*list){
        fprintf(stderr, "%s: memory allocation error (addtolist)\n",
                progname);  
        exit(EXIT_FAILURE);
      }
    }
    (*list)[*count].name = rname;
    if (strchr(def, PRIMITIVECHAR))
      (*list)[*count].def = "<primitive unit>";
    else
      (*list)[*count].def = def;
    (*count)++;
    len = strwidth(name);
    if (len>*maxnamelen)
          *maxnamelen = len;
  }
  if (searchtype == CONFORMABLE)
    freeunit(&want);
}


int 
compnd(const void *a, const void *b)
{
  return strcmp(((struct namedef *)a)->name, ((struct namedef *)b)->name);
}


int 
screensize()
{
   int lines = 20;      /* arbitrary but probably safe value */

#if defined (_WIN32) && defined (_MSC_VER)
   CONSOLE_SCREEN_BUFFER_INFO csbi;

   if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
       lines = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#endif

#ifdef HAVE_IOCTL
   struct winsize ws;
   int fd;
   fd = open("/dev/tty", O_RDWR);
   if (fd>=0 && ioctl(fd, TIOCGWINSZ, &ws)==0)
     lines = ws.ws_row;
#endif

   return lines;
}

/*
  determines whether the output will fit on the screen, and opens a
  pager if it won't
*/
FILE *
get_output_fp(int lines)
{
  FILE *fp = NULL;

  if (isatty(fileno(stdout)) && screensize() < lines) {
    if ((fp = popen(pager, "w")) == NULL) {
      fprintf(stderr, "%s: can't run pager '%s--'", progname, pager);
      perror((char*) NULL);
    }
  }
  if (!fp)
    fp = stdout;

  return fp;
}

int
countlines(char *msg)
{
  int nlines = 0;
  char *p;

  for (p = msg; *p; p++)
    if (*p == '\n')
      nlines++;

  return nlines;
}

/* 
   If have is non-NULL then search through all units and print the ones
   which are conformable with have.  Otherwise search through all the 
   units for ones whose names contain the second argument as a substring.
*/

void 
tryallunits(struct unittype *have, char *searchstring)
{
  struct unitlist *uptr;
  struct namedef *list;
  int listsize, maxnamelen, count;
  struct func *funcptr;
  struct wantalias *aliasptr;
  int i, j, searchtype;
  FILE *outfile;
  char *seploc, *firstunit;

  list = (struct namedef *) mymalloc( 100 * sizeof(struct namedef), 
                                       "(tryallunits)");
  listsize = 100;
  maxnamelen = 0;
  count = 0;

  if (have)
    searchtype = CONFORMABLE;
  else {
    if (!searchstring)
      searchstring="";
    searchtype = TEXTMATCH;
  }

  for(i=0;i<HASHSIZE;i++)
    for (uptr = utab[i]; uptr; uptr = uptr->next)
      addtolist(have, searchstring, uptr->name, uptr->name, uptr->value, 
                &list, &listsize, &maxnamelen, &count, searchtype);
  for(i=0;i<SIMPLEHASHSIZE;i++)
    for(funcptr=ftab[i];funcptr;funcptr=funcptr->next){
      if (funcptr->table) 
        addtolist(have, searchstring, funcptr->name, funcptr->tableunit, 
                "<piecewise linear>", &list, &listsize, &maxnamelen, &count, 
                searchtype);
      else
        addtolist(have, searchstring, funcptr->name, funcptr->inverse.dimen, 
                "<nonlinear>", &list, &listsize, &maxnamelen, &count, 
                searchtype);
  }
  for(aliasptr=firstalias;aliasptr;aliasptr=aliasptr->next){
    firstunit = dupstr(aliasptr->definition);/* coverity[var_assigned] */
    seploc = strchr(firstunit,UNITSEPCHAR);  /* Alias definitions allowed in */
    *seploc = 0;                             /* database contain UNITSEPCHAR */
    addtolist(have, searchstring, aliasptr->name, firstunit, 
              aliasptr->definition, &list, &listsize, &maxnamelen, &count,
              searchtype);
    free(firstunit);
  }
    
  qsort(list, count, sizeof(struct namedef), compnd);

  if (count==0)
    puts("No matching units found.");
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
  /* see if we need a pager */
  outfile = get_output_fp(count);
  for(i=0;i<count;i++){
    fputs(list[i].name,outfile);
    if (flags.verbose > 0 || flags.interactive) {
        for(j=strwidth(list[i].name);j<=maxnamelen;j++)
          putc(' ',outfile);
        tightprint(outfile,list[i].def);
    }
    fputc('\n',outfile);
  }
  if (outfile != stdout)
    pclose(outfile);
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_DFL);
#endif
}


/* If quiet is false then prompt user with the query.  

   Fetch one line of input and return it in *buffer.

   The bufsize argument is a dummy argument if we are using readline. 
   The readline version frees buffer if it is non-null.  The other
   version keeps the same buffer and grows it as needed.

   If no data is read, then this function exits the program. 
*/


void
getuser_noreadline(char **buffer, int *bufsize, const char *query)
{
#ifdef SUPPORT_UTF8
  int valid = 0;
  while(!valid){
    fputs(query, stdout);
    if (!fgetslong(buffer, bufsize, stdin,0)){
      if (!flags.quiet)
        putchar('\n');
      exit(EXIT_SUCCESS);
    }
    replacectrlchars(*buffer);
    valid = strwidth(*buffer)>=0;
    if (!valid)
      printf("Error: %s\n",invalid_utf8);
  }
#else
  fputs(query, stdout);
  if (!fgetslong(buffer, bufsize, stdin,0)){
    if (!flags.quiet)
      putchar('\n');
    exit(EXIT_SUCCESS);
  }
  replacectrlchars(*buffer);
#endif
}  


#ifndef READLINE
#  define getuser getuser_noreadline
#else 
           /* we DO have readline */
void
getuser_readline(char **buffer, int *bufsize, const char *query)
{
#ifdef SUPPORT_UTF8
  int valid = 0;
  while (!valid){
    if (*buffer) free(*buffer);
    *buffer = readline(query);
    if (*buffer)
      replacectrlchars(*buffer);
    if (!*buffer || strwidth(*buffer)>=0)
        valid=1;
    else
      printf("Error: %s\n",invalid_utf8);
  }
#else
    if (*buffer) free(*buffer);
    *buffer = readline(query);
    if (*buffer)
      replacectrlchars(*buffer);
#endif
  if (nonempty(*buffer)) add_history(*buffer);
  if (!*buffer){
    if (!flags.quiet)
       putchar('\n');
    exit(EXIT_SUCCESS);
  }
}


void
getuser(char **buffer, int *bufsize, const char *query)
{
  if (flags.readline)
    getuser_readline(buffer,bufsize,query);
  else
    getuser_noreadline(buffer,bufsize,query);
}


/* Unit name completion for readline.  

   Complete function names or alias names or builtin functions.  

   Complete to the end of a prefix or complete to the end of a unit.
   If the text includes a full prefix plus part of a unit and if the
   prefix is longer than one character then complete that compound.
   Don't complete a prefix fragment into prefix plus anything.
*/
   
#define CU_ALIAS 0
#define CU_BUILTIN 1
#define CU_FUNC 2
#define CU_PREFIX 3
#define CU_UNITS 4
#define CU_DONE 5

char *
completeunits(char *text, int state)
{
  static int uhash, fhash, phash, checktype;
  static struct prefixlist *curprefix, *unitprefix;
  static struct unitlist *curunit;
  static struct func *curfunc;
  static struct wantalias *curalias;
  static char **curbuiltin;
  char *output = 0;

#ifndef NO_SUPPRESS_APPEND
  rl_completion_suppress_append = 1;
#endif
  
  if (!state){     /* state == 0 means this is the first call, so initialize */
    checktype = 0; /* start at first type */
    fhash = uhash = phash = 0;
    unitprefix=0; /* search for unit continuations starting with this prefix */
    curfunc=ftab[fhash];
    curunit=utab[uhash];
    curprefix=ptab[phash];
    curbuiltin = builtins;
    curalias = firstalias;
  }
  while (checktype != CU_DONE){
    if (checktype == CU_ALIAS){
      while(curalias){
        if (startswith(curalias->name,text))
          output = dupstr(curalias->name);
        curalias = curalias->next;
        if (output) return output;
      }
      checktype++;
    }
    if (checktype == CU_BUILTIN){
      while(*curbuiltin){
        if (startswith(*curbuiltin,text))
          output = dupstr(*curbuiltin);
        curbuiltin++;
        if (output) return output;
      }
      checktype++;
    }
    while (checktype == CU_FUNC){
      while (!curfunc && fhash<SIMPLEHASHSIZE-1){
        fhash++;
        curfunc = ftab[fhash];
      }
      if (!curfunc) 
        checktype++;
      else {
        if (startswith(curfunc->name,text))
          output = dupstr(curfunc->name);
        curfunc = curfunc->next;
        if (output) return output;  
      }
    }
    while (checktype == CU_PREFIX){
      while (!curprefix && phash<SIMPLEHASHSIZE-1){
        phash++;
        curprefix = ptab[phash];
      }
      if (!curprefix)
        checktype++;
      else {
        if (startswith(curprefix->name,text))
          output = dupstr(curprefix->name);
        curprefix = curprefix->next;
        if (output) return output;
      }
    }
    while (checktype == CU_UNITS){    
      while (!curunit && uhash<HASHSIZE-1){
        uhash++;
        curunit = utab[uhash];
      }
      /* If we're done with the units go through them again with */
      /* the largest possible prefix stripped off */
      if (!curunit && !unitprefix 
            && (unitprefix = plookup(text)) && strlen(unitprefix->name)>1){
        uhash = 0;
        curunit = utab[uhash];
      }
      if (!curunit) {
        checktype++;
        break;
      }
      if (unitprefix){
        if (startswith(curunit->name, text+unitprefix->len)){
          output = (char *)mymalloc(1+strlen(curunit->name)+unitprefix->len,
                                    "(completeunits)");
          strcpy(output, unitprefix->name);
          strcat(output, curunit->name);
        }       
      } 
      else if (startswith(curunit->name,text)) 
        output = dupstr(curunit->name);
      curunit=curunit->next;
      if (output)
        return output;
    }
  } 
  return 0;
}

#endif /* READLINE */



/* see if current directory contains an executable file */
int
checkcwd (char *file)
{
  FILE *fp;
  char *p;

  fp = openfile(file, "r");
  if (fp){
    fclose(fp);
    return 1;
  }
#ifdef _WIN32
  else if (!((p = strrchr(file, '.')) && isexe(p))) {
    char *pathname;

    pathname = mymalloc(strlen(file) + strlen(EXE_EXT) + 1,
                        "(checkcwd)");
    strcpy(pathname, file);
    strcat(pathname, EXE_EXT);
    fp = openfile(pathname, "r");
    free(pathname);
    if (fp) {
      fclose(fp);
      return 1;
    }
  }
#endif

  return 0;
}

/* 
   return the last component of a pathname, and remove a .exe extension
   if one exists.
*/

char *
getprogramname(char *path)
{
  size_t proglen;
  char *p;

  path = pathend(path);

  /* get rid of filename extensions in Windows */
  proglen = strlen(path);

  if ((p = strrchr(path, '.')) && isexe(p))
    proglen -= 4;
  return dupnstr(path, proglen);
}

/*
   Find the directory that contains the invoked executable.
*/

char *
getprogdir(char *progname, char **fullprogname)
{
  char *progdir = NULL;
  char *p;

#if defined (_WIN32) && defined (_MSC_VER)
  char buf[FILENAME_MAX + 1];

  /* get the full pathname of the current executable and be done with it */
  /* TODO: is there way to do this with gcc? */

  if (GetModuleFileName(NULL, buf, FILENAME_MAX + 1))
    progdir = dupstr(buf);
#endif

  /* If path name is absolute or includes more than one component use it */

  if (!progdir && (isfullpath(progname) || hasdirsep(progname)))
    progdir = dupstr(progname);


  /*
  command.com and cmd.exe under Windows always run a program that's in the
  current directory whether or not the current directory is in PATH, so we need
  to check the current directory.

  This could return a false positive if units is run from a Unix-like command
  interpreter under Windows if the current directory is not in PATH but
  contains 'units' or 'units.exe'
  */
#if defined (_WIN32) && !defined (_MSC_VER)
  if (!progdir && checkcwd(progname)) 
      progdir = dupstr(progname);
#endif

  /* search PATH to find the executable */
  if (!progdir) {
    char *env;
    env = getenv("PATH");
    if (env) {
      /* search PATH */
      char *direc, *direc_end, *pathname;
      int len;
      FILE *fp;

      pathname = mymalloc(strlen(env)+strlen(progname)+strlen(EXE_EXT)+2,
                                                         "(getprogdir)");
      direc = env;
      while (direc) {
        direc_end = strchr(direc,PATHSEP);
        if (!direc_end)
          len = strlen(direc);
        else 
          len = direc_end-direc;
        strncpy(pathname, direc, len);
        if (len>0)
          pathname[len++]=DIRSEP;
        strcpy(pathname+len, progname);
        fp = openfile(pathname, "r");
        if (fp){
          progdir = dupstr(pathname);
          break;
        }
#ifdef _WIN32
        /*
          executable may or may not have '.exe' suffix, so we need to
          look for both
        */
        if (!((p = strrchr(pathname, '.')) && isexe(p))) {
          strcat(pathname, EXE_EXT);
          fp = openfile(pathname, "r");
          if (fp){
            progdir = dupstr(pathname);
            break;
          }
        }
#endif
        direc = direc_end;
        if (direc) direc++;
      }
      free(pathname);
      if (fp)
        fclose(fp);
    }
  }

  if (!progdir) {
    fprintf(stderr, "%s: cannot find program directory\n", progname);
    exit(EXIT_FAILURE);
  }

  *fullprogname = dupstr(progdir);      /* used by printversion() */
  p = pathend(progdir);
  *p = '\0';  
 
  return progdir; 
}

/*
  find a possible data directory relative to a 'bin' directory that
  contains the executable
*/

char *
getdatadir()
{
    int progdirlen;
    char *p;

    if (isfullpath(DATADIR))
      return DATADIR;
    if (!progdir || emptystr(DATADIR))
      return NULL;
    progdirlen = strlen(progdir);
    datadir = (char *) mymalloc(progdirlen + strlen(DATADIR) + 2,
				"(getdatadir)");
    strcpy(datadir, progdir);
    if (isdirsep(progdir[progdirlen - 1]))
      datadir[progdirlen - 1] = '\0';   /* so pathend() can work */
    p = pathend(datadir);
    if ((strlen(p) == 3) && (tolower(p[0]) == 'b') \
       && (tolower(p[1]) == 'i') && (tolower(p[2]) == 'n')) {
      p = DATADIR;
      while (*p == '.')         /* ignore "./", "../" */
	p++;
      if (isdirsep(*p))
	p++;
      strcpy(pathend(datadir), p);
      
      return datadir;
    }
    else
      return NULL;
}

void
showfilecheck(int errnum, char *filename)
{
  if (errnum==ENOENT)
    printf("  Checking %s\n", filename);
  else
    printf("  Checking %s: %s\n", filename, strerror(errnum)); 
}

/*
  On Windows, find the locale map
  
  If print is set to ERRMSG then display error message if the file is
  not valid.  If print is set to SHOWFILES then display files that are
  checked (when the filename is not a fully specified path).  If print
  is set to NOERRMSG then do not display anything.

  Returns filename of valid local map file or NULL if no file was found.
*/
#ifdef _WIN32
char *
findlocalemap(int print)
{
  FILE *map = NULL;
  char *filename = NULL;
  char *file;

  /*
      Try the environment variable UNITSLOCALEMAP, then the #defined
      value LOCALEMAP, then the directory containing the units
      executable, then the directory given by datadir, and finally
      the directory containing the units data file.
  */

  /* check the environment variable UNITSLOCALEMAP */
  file = getenv("UNITSLOCALEMAP");
  if (nonempty(file)) {
    map = openfile(file,"rt");
    if (!map) {
      if (print == ERRMSG) {
        fprintf(stderr,
                "%s: cannot open locale map '%s'\n  specified in UNITSLOCALEMAP environment variable. ",
                progname, file);
        perror((char *) NULL);
      }
      return NULL;
    }
    else
      filename = dupstr(file);
  }

  /* check the #defined value LOCALEMAP */
  if (!map) {
    file = LOCALEMAP;
    map = openfile(file,"rt");
    if (map)
      filename = dupstr(file);
  }

  if (!map && !progdir) {
    if (print == ERRMSG) {
      fprintf(stderr,
              "%s: cannot find locale map--program directory not set\n",
              progname);
      exit(EXIT_FAILURE);
    }
    else
      return NULL;
  }

  /* check the directory with the units executable */
  if (!map) {
    filename = (char *) mymalloc(strlen(progdir) + strlen(file) + 2,
               "(findlocalemap)");
    strcpy(filename, progdir);
    strcat(filename, file);
    map = openfile(filename,"rt");
    if (print==SHOWFILES && !map)
      showfilecheck(errno, filename);
  }

  /* check data directory */
  if (!map && datadir) {     
    if (filename)
      free(filename);
    filename = (char *) mymalloc(strlen(datadir) + strlen(file) + 3,
                                 "(findlocalemap)");
    strcpy(filename, datadir);
    strcat(filename, DIRSEPSTR);
    strcat(filename, file);
    map = openfile(filename, "rt");
    if (print==SHOWFILES && !map)
      showfilecheck(errno, filename);
  }

  /* check the directory with the units data file  */
  if (!map && unitsfiles[0]) {
    char *lastfilename = NULL;

    if (filename) {
      if (datadir)
	lastfilename = dupstr(filename);
      free(filename);
    }
    filename = (char *) mymalloc(strlen(unitsfiles[0]) + strlen(file) + 2,
               "(findlocalemap)");
    strcpy(filename, unitsfiles[0]);
    strcpy(pathend(filename), file);

    /* don't bother if we've just checked for it */
    if (lastfilename && strcmp(filename, lastfilename)) {
      map = openfile(filename,"rt");
      if (print==SHOWFILES && !map)
	showfilecheck(errno, filename);
    }
  }

  if (map) {
    fclose(map);
    return filename;
  }
  else {
    if (filename)
      free(filename);
    return NULL;
  }
}
#endif

/*
  Find the units database file. 
  
  If print is set to ERRMSG then display error message if the file is
  not valid.  If print is set to SHOWFILES then display files that are
  checked (when the filename is not a fully specified path).  If print
  is set to NOERRMSG then do not display anything.

  Returns filename of valid database file or NULL if no file
  was found.  
*/

char *
findunitsfile(int print)
{
  FILE *testfile=0;
  char *file;

  file = getenv("UNITSFILE");
  if (nonempty(file)) {
    testfile = openfile(file, "rt");
    if (!testfile) {
      if (print==ERRMSG) {
        fprintf(stderr,
                "%s: cannot open units file '%s' in environment variable UNITSFILE.  ",
                progname, file);
        perror((char *) NULL);
      }
      return NULL;
    }
  }

  if (!testfile && isfullpath(UNITSFILE)){
    file = UNITSFILE;
    testfile = openfile(file, "rt");
    if (!testfile) {
      if (print==ERRMSG) {
        fprintf(stderr,
                "%s: cannot open units data file '%s'.  ", progname, UNITSFILE);
        perror((char *) NULL);
      }
      return NULL;
    }
  }      

  if (!testfile && !progdir) {
    if (print==ERRMSG) {
      fprintf(stderr,
              "%s: cannot open units file '%s' and cannot find program directory.\n", progname, UNITSFILE);
      perror((char *) NULL);
    }
    return NULL;
  }

  if (!testfile) {
    /* check the directory containing the units executable */
    file = (char *) mymalloc(strlen(progdir)+strlen(UNITSFILE)+1,
                             "(findunitsfile)");
    strcpy(file, progdir);
    strcat(file, UNITSFILE);
    testfile = openfile(file, "rt");
    if (print==SHOWFILES && !testfile)
      showfilecheck(errno, file);
    if (!testfile)
      free(file);
  }

  /* check data directory */
  if (!testfile && datadir) {
    file = (char *) mymalloc(strlen(datadir) + strlen(UNITSFILE) + 2,
                             "(findunitsfile)");
    strcpy(file, datadir);
    strcat(file, DIRSEPSTR);
    strcat(file, UNITSFILE);
    testfile = openfile(file, "rt");
    if (print==SHOWFILES && !testfile)
      showfilecheck(errno, file);
    if (!testfile)
      free(file);
  }

  if (!testfile) {
    if (print==ERRMSG)
      fprintf(stderr,"%s: cannot find units file '%s'\n", progname, UNITSFILE);
     return NULL;
  }
  else {
    fclose(testfile);
    return file;
  }
}

/*
  Find the user's home directory.  Unlike *nix, Windows doesn't usually
  set HOME, so we check several other places as well.
*/
char *
findhome(char **errmsg)
{
  struct stat statbuf;
  int allocated = 0;
  char *homedir;
  char notfound[] = "Specified home directory '%s' does not exist";
  char notadir[] = "Specified home directory '%s' is not a directory";

  /*
    Under UNIX just check HOME.  Under Windows, if HOME is not set then
    check HOMEDRIVE:HOMEPATH and finally USERPROFILE
  */
  homedir = getenv("HOME");

#ifdef _WIN32
  if (!nonempty(homedir)) {
    /* try a few other places */
    /* try HOMEDRIVE and HOMEPATH */
    char *homedrive, *homepath;
    if ((homedrive = getenv("HOMEDRIVE")) && *homedrive && (homepath = getenv("HOMEPATH")) && *homepath) {
      homedir = mymalloc(strlen(homedrive)+strlen(homepath)+1,"(personalfile)");
      allocated = 1;
      strcpy(homedir, homedrive);
      strcat(homedir, homepath);
    }
    else
      /* finally, try USERPROFILE */
      homedir = getenv("USERPROFILE");
  }
#endif
  
  /*
    If a home directory is specified, see if it exists and is a
    directory.  If not set error message text. 
  */
  
  if (nonempty(homedir)) {
    if (stat(homedir, &statbuf) != 0) {
      *errmsg = malloc(strlen(notfound)+strlen(homedir));
      sprintf(*errmsg, notfound, homedir);
    }
    else if (!(statbuf.st_mode & S_IFDIR)) {
      *errmsg = malloc(strlen(notadir)+strlen(homedir));
      sprintf(*errmsg, notadir, homedir);
    }
    if (!allocated)
      homedir = dupstr(homedir);
    return homedir;
  }
  else {
    *errmsg = "no home directory";
    return NULL;
  }
}

/*
  Find a personal file.  First checks the specified environment variable 
  (envname) for the filename to  use.  If this is unset then search user's 
  home directory for basename.  If there is no home directory, returns 
  NULL.  Otherwise if the file exists then returns its name in newly allocated
  space and sets *exists to 1. If the file does not exist then sets *exist to 
  zero and:
      With checkonly == 0, prints error message and returns NULL
      With checkonly != 0, returns filename (does not print error message)
*/
char *
personalfile(const char *envname, const char *basename,
             int checkonly, int *exists)
{
  FILE *testfile;
  char *filename=NULL;

  *exists = 0; 

  /* First check the specified environment variable for a file name */ 
  if (envname) 
    filename = getenv(envname);
  if (nonempty(filename)){       
    testfile = openfile(filename, "rt");
    if (testfile){
      fclose(testfile);
      *exists = 1;
      return filename;
    }
    if (checkonly)
      return filename;
    else {
      fprintf(stderr, "%s: cannot open file '%s' specified in %s environment variable: ",
              progname, filename, envname);
      perror((char *) NULL);
      return NULL;
    }
  }
  /* Environment variable not set: look in home directory */
  else if (nonempty(homedir)) {
    filename = mymalloc(strlen(homedir)+strlen(basename)+2,
                        "(personalfile)");
    strcpy(filename,homedir);

    if (strcmp(homedir, "/") && strcmp(homedir, "\\"))
      strcat(filename,DIRSEPSTR);
    strcat(filename,basename);

    testfile = openfile(filename, "rt");
    if (testfile){
      fclose(testfile);
      *exists = 1;
      return filename;
    }
    if (checkonly)
      return filename;
    else {
      if (errno==EACCES || errno==EISDIR) {
        fprintf(stderr,"%s: cannot open file '%s': ",progname,filename);
        perror(NULL);
      }
      free(filename);
      return NULL;
    }
  }
  else
    return NULL;
}


/* print usage message */

void 
usage()
{
  int nlines;
  char *unitsfile;
  char *msg = "\nUsage: %s [options] ['from-unit' 'to-unit']\n\n\
Options:\n\
    -h, --help           show this help and exit\n\
    -c, --check          check that all units reduce to primitive units\n\
        --check-verbose  like --check, but lists units as they are checked\n\
        --verbose-check    so you can find units that cause endless loops\n\
    -d, --digits         show output to specified number of digits (default: %d)\n\
    -e, --exponential    exponential format output\n\
    -f, --file           specify a units data file (-f '' loads default file)\n"
#ifdef READLINE
"\
    -H, --history        specify readline history file (-H '' disables history)\n"
#endif
"\
    -L, --log            specify a file to log conversions\n\
    -l, --locale         specify a desired locale\n\
    -m, --minus          make - into a subtraction operator (default)\n\
        --oldstar        use old '*' precedence, higher than '/'\n\
        --newstar        use new '*' precedence, equal to '/'\n\
    -n, --nolists        disable conversion to unit lists\n\
    -S, --show-factor    show non-unity factor before 1|x in multi-unit output\n\
        --conformable    in non-interactive mode, show all conformable units\n\
    -o, --output-format  specify printf numeric output format (default: %%.%d%c)\n\
    -p, --product        make '-' into a product operator\n\
    -q, --quiet          suppress prompting\n\
        --silent         same as --quiet\n\
    -s, --strict         suppress reciprocal unit conversion (e.g. Hz<->s)\n\
    -v, --verbose        show slightly more verbose output\n\
        --compact        suppress printing of tab, '*', and '/' character\n\
    -1, --one-line       suppress the second line of output\n\
    -t, --terse          terse output (--strict --compact --quiet --one-line)\n\
    -r, --round          round last element of unit list output to an integer\n\
    -U, --unitsfile      show units data filename and exit\n\
    -u, --units          specify a CGS unit system (gauss[ian]|esu|emu)\n\
    -V, --version        show version, data filenames (with -t: version only)\n\
    -I, --info           show version, files, and program properties\n";
  FILE *fp = NULL;

  unitsfile = findunitsfile(NOERRMSG);

  nlines = countlines(msg);
  /* see if we need a pager */
  fp = get_output_fp(nlines + 4);

  fprintf(fp, msg, progname, DEFAULTPRECISION, DEFAULTPRECISION, DEFAULTTYPE);
  if (!unitsfile)
    fprintf(fp, "Units data file '%s' not found.\n\n", UNITSFILE);
  else
    fprintf(fp, "\nTo learn about the available units look in '%s'\n\n", unitsfile);
  fputs("Report bugs to adrianm@gnu.org.\n\n", fp);

  if (fp != stdout)
    pclose(fp);
}

/* Print message about how to get help */

void 
helpmsg()
{
  fprintf(stderr,"\nTry '%s --help' for more information.\n",progname);
  exit(EXIT_FAILURE);
}

/* show units version, and optionally, additional information */
void
printversion()
{
  int exists;
  char *u_unitsfile = NULL;  /* units data file specified in UNITSFILE */
  char *m_unitsfile;         /* personal units data file from HOME_UNITS_ENV */
  char *p_unitsfile;         /* personal units data file */
  FILE *fp, *histfile;
#ifdef _WIN32
  char *localemap;
#endif

  if (flags.verbose == 0) {
    printf("GNU Units version %s\n", VERSION);
    return;
  }

  printf("GNU Units version %s\n%s, %s, locale %s\n",
         VERSION, RVERSTR,UTF8VERSTR,mylocale);
#if defined (_WIN32) && defined (HAVE_MKS_TOOLKIT)
  puts("With MKS Toolkit");
#endif

  if (flags.verbose == 2) {
    if (!fullprogname)
      getprogdir(progname, &fullprogname);
    if (fullprogname)
      printf("\n%s program is %s\n", progname, fullprogname);
  }

  /* units data file */

  putchar('\n');
  if (isfullpath(UNITSFILE))
    printf("Default units data file is '%s'\n", UNITSFILE);
  else
    printf("Default units data file is '%s';\n  %s will search for this file\n",
           UNITSFILE, progname);
  if (flags.verbose < 2)
    printf("Default personal units file: %s\n", homeunitsfile);

  if (flags.verbose == 2){
    u_unitsfile = getenv("UNITSFILE");
    if (u_unitsfile)
      printf("Environment variable UNITSFILE set to '%s'\n", u_unitsfile);
    else
      puts("Environment variable UNITSFILE not set");

    unitsfiles[0] = findunitsfile(SHOWFILES);
    
    if (unitsfiles[0]) {
      /* We searched for the file in program and data dirs */
      if (!isfullpath(UNITSFILE) && !nonempty(u_unitsfile))
        printf("Found data file '%s'\n", unitsfiles[0]);
      else
        printf("Units data file is '%s'\n", unitsfiles[0]);
    } 
    else {
      if (errno && (nonempty(u_unitsfile) || isfullpath(UNITSFILE)))
        printf("*** Units data file invalid: %s ***\n",strerror(errno));
      else
        puts("*** Units data file not found ***");
    }
    if (homedir_error)
      printf("\n%s\n", homedir_error);
    else
      printf("\nHome directory is '%s'\n", homedir);
  }

  /* personal units data file: environment */
  if (flags.verbose == 2){
    m_unitsfile = getenv(HOME_UNITS_ENV);
    putchar('\n');
    if (m_unitsfile) {
      printf("Environment variable %s set to '%s'\n",
             HOME_UNITS_ENV,m_unitsfile);
    }
    else
      printf("Environment variable %s not set\n", HOME_UNITS_ENV);

    p_unitsfile = personalfile(HOME_UNITS_ENV, homeunitsfile, 1, &exists);
    if (p_unitsfile) {
      printf("Personal units data file is '%s'\n", p_unitsfile);
      if (!exists){
        if (homedir_error && !nonempty(m_unitsfile))
          printf("  (File invalid: %s)\n", homedir_error);
        else if (errno==ENOENT && !nonempty(m_unitsfile))
          puts("  (File does not exist)");
        else 
          printf("  (File invalid: %s)\n",strerror(errno));
      }  
    }
    else
      puts("Personal units data file not found: no home directory");
  }
#ifdef READLINE
  if (flags.verbose == 2) {
    historyfile = personalfile(NULL,HISTORY_FILE,1,&exists);
    if (historyfile){
      printf("\nDefault readline history file is '%s'\n", historyfile);
      histfile = openfile(historyfile,"r+");
      if (!histfile)
        printf("  (File invalid: %s)\n",
               homedir_error ? homedir_error : strerror(errno));
      else
        fclose(histfile);
    }  
    else 
      puts("\nReadline history file unusable: no home directory");
  }
#endif

#ifdef _WIN32
  /* locale map */
  if (flags.verbose == 2) {
    putchar('\n');
    localemap = getenv("UNITSLOCALEMAP");
    if (localemap)
      printf("Environment variable UNITSLOCALEMAP set to '%s'\n", localemap);
    else
      puts("Environment variable UNITSLOCALEMAP not set");

    if (isfullpath(LOCALEMAP))
      printf("Default locale map is '%s'\n", LOCALEMAP);
    else
      printf("Default locale map is '%s';\n  %s will search for this file\n",
             LOCALEMAP, progname);

    localemap = findlocalemap(SHOWFILES);
    if (localemap && !isfullpath(LOCALEMAP))
      printf("Found locale map '%s'\n", localemap);
    else if (localemap)
      printf("Locale map is '%s'\n", localemap);
    else
      puts("*** Locale map not found ***");
  }
#endif

  printf("\n%s\n\n", LICENSE);
}

void
showunitsfile()
{
  char *unitsfile;
  unitsfile = findunitsfile(NOERRMSG);
  if (unitsfile)
    printf("%s\n", unitsfile);
  else
    puts("Units data file not found");
}


char *shortoptions = "VIUu:vqechSstf:o:d:mnpr1l:L:"
#ifdef READLINE
    "H:"
#endif
  ;

struct option longoptions[] = {
  {"check", no_argument, &flags.unitcheck, 1},
  {"check-verbose", no_argument, &flags.unitcheck, 2},
  {"compact", no_argument, &flags.verbose, 0},
  {"digits", required_argument, 0, 'd'},
  {"exponential", no_argument, 0, 'e'},
  {"file", required_argument, 0, 'f'},
  {"help", no_argument, 0, 'h'},
#ifdef READLINE  
  {"history", required_argument, 0, 'H'},
#endif  
  {"info", no_argument, 0, 'I'},
  {"locale", required_argument, 0, 'l'}, 
  {"log", required_argument, 0, 'L'}, 
  {"minus", no_argument, &parserflags.minusminus, 1},
  {"newstar", no_argument, &parserflags.oldstar, 0},
  {"nolists", no_argument, 0, 'n'},
  {"oldstar", no_argument, &parserflags.oldstar, 1},
  {"one-line", no_argument, &flags.oneline, 1},
  {"output-format", required_argument, 0, 'o'},
  {"product", no_argument, &parserflags.minusminus, 0},
  {"quiet", no_argument, &flags.quiet, 1},
  {"round",no_argument, 0, 'r'},
  {"show-factor", no_argument, 0, 'S'},
  {"conformable", no_argument, &flags.showconformable, 1 },
  {"silent", no_argument, &flags.quiet, 1},
  {"strict",no_argument,&flags.strictconvert, 1},
  {"terse",no_argument, 0, 't'},
  {"unitsfile", no_argument, 0, 'U'},
  {"units", required_argument, 0, 'u'},
  {"verbose", no_argument, &flags.verbose, 2},
  {"verbose-check", no_argument, &flags.unitcheck, 2},
  {"version", no_argument, 0, 'V'},
  {0,0,0,0} };

/* Process the args.  Returns 1 if interactive mode is desired, and 0
   for command line operation.  If units appear on the command line
   they are returned in the from and to parameters. */

int
processargs(int argc, char **argv, char **from, char **to)
{
   extern char *optarg;
   extern int optind;
   int optchar, optindex;
   int ind;
   int doprintversion=0;
   char *unitsys=0;
   
   // Reset getopt
   optind = 1;
   optarg = NULL;

   while ( -1 != 
      (optchar = 
         getopt_long(argc, argv,shortoptions,longoptions, &optindex ))) {
      switch (optchar) {
         case 'm':
            parserflags.minusminus = 1;
            break;
         case 'p':
            parserflags.minusminus = 0;
            break;
         case 't':
            flags.oneline = 1;
            flags.quiet = 1;
            flags.strictconvert = 1;
            flags.verbose = 0;
            break;

         /* numeric output format */
         case 'd':
            if (checksigdigits(optarg) < 0)
              exit(EXIT_FAILURE);
            else    /* ignore anything given with 'o' option */
              num_format.format = NULL; 
            break;
         case 'e':  /* ignore anything given with 'o' option */
            num_format.format = NULL;   
            num_format.type = 'e';
            break;
         case 'o':
            num_format.format = optarg;
            break;

         case 'c':
            flags.unitcheck = 1;
            break;
         case 'f':
            for(ind=0;unitsfiles[ind];ind++); 
            if (ind==MAXFILES){
              fprintf(stderr, "At most %d -f specifications are allowed\n",
                      MAXFILES);
              exit(EXIT_FAILURE);
            }
            if (optarg && *optarg)
              unitsfiles[ind] = optarg;
            else {
              unitsfiles[ind] = findunitsfile(ERRMSG);
              if (!unitsfiles[ind]) 
                exit(EXIT_FAILURE);
            }
            unitsfiles[ind+1] = 0;
            break;
         case 'L':
            logfilename = optarg;
            break;
         case 'l':
            mylocale = optarg;
            break;
         case 'n':
            flags.unitlists = 0;
            break;
         case 'q':
            flags.quiet = 1;
            break; 
         case 'r':
            flags.round = 1;
            break;
         case 'S':
            flags.showfactor = 1;
            break;
         case 's':
            flags.strictconvert = 1;
            break;
         case 'v':
            flags.verbose = 2;
            break;
         case '1':
            flags.oneline = 1;
            break;
         case 'I':
            flags.verbose = 2;  /* fall through */
         case 'V':
            doprintversion = 1;
            break;
         case 'U':
            showunitsfile();
            exit(EXIT_SUCCESS);
            break;
         case 'u':
            unitsys = optarg;
            for(ind=0;unitsys[ind];ind++)
              unitsys[ind] = tolower(unitsys[ind]);
            break;
         case 'h':
            usage();
            exit(EXIT_SUCCESS);
#ifdef READLINE     
         case 'H':
           if (emptystr(optarg))
              historyfile=NULL;
            else
              historyfile = optarg;
            break;
#endif      
         case 0: break;  /* This is reached if a long option is 
                            processed with no return value set. */
         case '?':        /* Invalid option or missing argument returns '?' */
         default:
            helpmsg();    /* helpmsg() exits with error */
      } 
   }

   if (doprintversion){
     printversion();
     exit(EXIT_SUCCESS);
   }

   /* command-line option overwrites environment */
   if (unitsys)
     setenv("UNITS_SYSTEM", unitsys, 1);

   if (flags.unitcheck) {
     if (optind != argc){
       fprintf(stderr, 
               "Too many arguments (arguments are not allowed with -c).\n");
       helpmsg();         /* helpmsg() exits with error */
     }
   } else {
     if (optind == argc - 2) {
        if (flags.showconformable) {
          fprintf(stderr,"Too many arguments (only one unit expression allowed with '--conformable').\n");
          helpmsg();             /* helpmsg() exits with error */
        }
        flags.quiet=1;
        *from = argv[optind];
        *to = dupstr(argv[optind+1]); /* This string may get rewritten later */
        return 0;                     /* and we might call free() on it      */
     }

     if (optind == argc - 1) {
        flags.quiet=1;
        *from = argv[optind];
        *to=0;
        return 0;
     }
     if (optind < argc - 2) {
        fprintf(stderr,"Too many arguments (maybe you need quotes).\n");
        helpmsg();             /* helpmsg() exits with error */
     }
   }

   return 1;
}

/* 
   Show a pointer under the input to indicate a problem.
   Prints 'position' spaces and then the pointer.
   If 'position' is negative, nothing is printed.
 */

void
showpointer(int position)
{
  if (position >= 0){
    while (position--) putchar(' ');
    puts("^");
  }
} /* end showpointer */


/*
   Process the string 'unitstr' as a unit, placing the processed data
   in the unit structure 'theunit'.  Returns 0 on success and 1 on
   failure.  If an error occurs an error message is printed to stdout.
   A pointer ('^') will be printed if an error is detected, and  promptlen 
   should be set to the printing width of the prompt string, or set 
   it to NOPOINT to supress printing of the pointer.  
 */

 
int 
processunit(struct unittype *theunit, char *unitstr, int promptlen)
{
  char *errmsg;
  int errloc,err;
  char savechar;

  if (flags.unitlists && strchr(unitstr, UNITSEPCHAR)){
    puts("Unit list not allowed");
    return 1;
  }
  if ((err=parseunit(theunit, unitstr, &errmsg, &errloc))){
    if (promptlen >= 0){
      if (err!=E_UNKNOWNUNIT || !irreducible){
        if (errloc>0) {
          savechar = unitstr[errloc];
          unitstr[errloc] = 0;
          showpointer(promptlen+strwidth(unitstr)-1);
          unitstr[errloc] = savechar;
        } 
        else showpointer(promptlen);
      }
    }
    else
      printf("Error in '%s': ", unitstr);
    fputs(errmsg,stdout);
    if (err==E_UNKNOWNUNIT && irreducible)
      printf(" '%s'", irreducible);
    putchar('\n');
    return 1;
  }
  if ((err=completereduce(theunit))){
    fputs(errormsg[err],stdout);
    if (err==E_UNKNOWNUNIT)
      printf(" '%s'", irreducible);
    putchar('\n');
    return 1;
  }
  return 0;
}



/*
   Checks the input parameter unitstr (a list of units separated by
   UNITSEPCHAR) for errors.  All units must be parseable and
   conformable to each other.  Returns 0 on success and 1 on failure.

   If an error is found then print an error message on stdout.  A
   pointer ('^') will be printed to mark the error.  The promptlen
   parameter should be set to the printing width of the prompt string
   so that the pointer is correctly aligned.

   To suppress the printing of the pointer set promptlen to NOPOINT.
   To suppress printing of error messages entirely set promptlen to 
   NOERRMSG.
*/


int
checkunitlist(char *unitstr, int promptlen)
{
  struct unittype unit[2], one;
  char *firstunitstr,*nextunitstr;
  int unitidx = 0;

  int printerror = promptlen != NOERRMSG;

  initializeunit(&one);

  firstunitstr = unitstr;

  initializeunit(unit);
  initializeunit(unit+1);

  while (unitstr) {
    if ((nextunitstr = strchr(unitstr, UNITSEPCHAR)) != 0)
      *nextunitstr = '\0';

    if (!unitstr[strspn(unitstr, " ")]) {  /* unitstr is blank */
      if (!nextunitstr) {  /* terminal UNITSEPCHAR indicates repetition */
        freeunit(unit);    /* of last unit and is permitted */
        return 0;
      }
      else {               /* internal blank units are not allowed */
        if (printerror){
          showpointer(promptlen);
          puts("Error: blank unit not allowed");
        }
        freeunit(unit);
        return 1;
      }
    }

    /* processunit() prints error messages; avoid it to supress them */

    if ((printerror && processunit(unit+unitidx,unitstr,promptlen)) ||
        (!printerror && 
           (parseunit(unit+unitidx, unitstr,0,0) 
             || completereduce(unit+unitidx) 
             || compareunits(unit+unitidx,&one, ignore_primitive)))){
      if (printerror)
        printf("Error in unit list entry: %s\n",unitstr);
      freeunit(unit);
      freeunit(unit+1);
      return 1;
    }


    if (unitidx == 0)
      unitidx = 1;
    else {
      if (compareunits(unit, unit+1, ignore_dimless)){
        if (printerror){
          int wasverbose = flags.verbose;
          FILE *savelog = logfile;
          logfile=0;
          flags.verbose = 2;   /* always use verbose form to be unambiguous */
                               /* coverity[returned_null] */
          *(strchr(firstunitstr, UNITSEPCHAR)) = '\0';
          removespaces(firstunitstr);
          removespaces(unitstr);
          showpointer(promptlen);
          showconformabilityerr(firstunitstr, unit, unitstr, unit+1);
          flags.verbose = wasverbose;
          logfile = savelog;
        }
        freeunit(unit);
        freeunit(unit+1);
        return 1;
      }
      freeunit(unit+1);
    }

    if (nextunitstr) {
      if (promptlen >= 0) promptlen += strwidth(unitstr)+1;
      *(nextunitstr++) = UNITSEPCHAR;
    }
    unitstr = nextunitstr;
  }

  freeunit(unit);

  return 0;
} /* end checkunitlist */


/*
   Call either processunit or checkunitlist, depending on whether the
   string 'unitstr' contains a separator character.  Returns 0 on
   success and 1 on failure.  If an error occurs an error message is
   printed to stdout.

   A pointer will be printed if an error is detected, and  promptlen 
   should be set to the printing width of the prompt string, or set 
   it to NOPOINT to supress printing of the pointer.  
*/

int
processwant(struct unittype *theunit, char *unitstr, int promptlen)
{
  if (flags.unitlists && strchr(unitstr, UNITSEPCHAR))
    return checkunitlist(unitstr, promptlen);
  else
    return processunit(theunit, unitstr, promptlen);
}


void
checkallaliases(int verbose)
{
  struct wantalias *aliasptr;

  for(aliasptr = firstalias; aliasptr; aliasptr=aliasptr->next){
    if (verbose)
      printf("doing unit list '%s'\n", aliasptr->name);
    if (checkunitlist(aliasptr->definition,NOERRMSG))
      printf("Unit list '%s' contains errors\n", aliasptr->name);
    if (ulookup(aliasptr->name))
      printf("Unit list '%s' hides a unit definition.\n", aliasptr->name);
    if (fnlookup(aliasptr->name))
      printf("Unit list '%s' hides a function definition.\n", aliasptr->name);
  }
}



/* 
   Check that all units and prefixes are reducible to primitive units and that
   function definitions are valid and have correct inverses.  A message is
   printed for every unit that does not reduce to primitive units.

*/


void 
checkunits(int verbosecheck)
{
  struct unittype have,second,one;
  struct unitlist *uptr;
  struct prefixlist *pptr;
  struct func *funcptr;
  char *prefixbuf, *testunit;
  int i;

  initializeunit(&one);

  /* Check all functions for valid definition and correct inverse */
  
  for(i=0;i<SIMPLEHASHSIZE;i++)
    for(funcptr=ftab[i];funcptr;funcptr=funcptr->next)
      checkfunc(funcptr, verbosecheck);

  checkallaliases(verbosecheck);

  /* Now check all units for validity */

  for(i=0;i<HASHSIZE;i++)
    for (uptr = utab[i]; uptr; uptr = uptr->next){
      if (verbosecheck)
        printf("doing '%s'\n",uptr->name);
      if (parseunit(&have, uptr->name,0,0) 
          || completereduce(&have) 
          || compareunits(&have,&one, ignore_primitive)){
        if (fnlookup(uptr->name)) 
          printf("Unit '%s' hidden by function '%s'\n", uptr->name, uptr->name);
        else
          printf("'%s' defined as '%s' irreducible\n",uptr->name, uptr->value);
      } else {
        parserflags.minusminus = !parserflags.minusminus; 
                                                 /* coverity[check_return] */
        parseunit(&second, uptr->name, 0, 0);    /* coverity[check_return] */
        completereduce(&second);     /* Can't fail because it worked above */
        if (compareunits(&have, &second, ignore_nothing)){
          printf("'%s': replace '-' with '+-' for subtraction or '*' to multiply\n", uptr->name);
        }
        freeunit(&second);
        parserflags.minusminus=!parserflags.minusminus;
      }

      freeunit(&have);
    }

  /* Check prefixes */ 

  testunit="meter";
  for(i=0;i<SIMPLEHASHSIZE;i++)
    for(pptr = ptab[i]; pptr; pptr = pptr->next){
      if (verbosecheck)
        printf("doing '%s-'\n",pptr->name);
      prefixbuf = mymalloc(strlen(pptr->name) + strlen(testunit) + 1,
                           "(checkunits)");
      strcpy(prefixbuf,pptr->name);
      strcat(prefixbuf,testunit);
      if (parseunit(&have, prefixbuf,0,0) || completereduce(&have) || 
          compareunits(&have,&one,ignore_primitive))
        printf("'%s-' defined as '%s' irreducible\n",pptr->name, pptr->value);
      else { 
        int plevel;    /* check for bad '/' character in prefix */
        char *ch;
        plevel = 0;
        for(ch=pptr->value;*ch;ch++){
          if (*ch==')') plevel--;
          else if (*ch=='(') plevel++;
          else if (plevel==0 && *ch=='/'){
            printf(
              "'%s-' defined as '%s' contains a bad '/'. (Add parentheses.)\n",
              pptr->name, pptr->value);
            break;
          }
        }           
      }  
      freeunit(&have);
      free(prefixbuf);
    }
}


/*
   Converts the input value 'havestr' (which is already parsed into
   the unit structure 'have') into a sum of the UNITSEPCHAR-separated
   units listed in 'wantstr'.  You must call checkunitlist first to
   ensure 'wantstr' is error-free.  Prints the results (or an error message)
   on stdout.  Returns 0 on success and 1 on failure.
*/

int
showunitlist(char *havestr, struct unittype *have, char *wantstr)
{
  struct unittype want, lastwant;
  char *lastunitstr, *nextunitstr, *lastwantstr=0;
  double remainder;     /* portion of have->factor remaining */
  double round_dir;     /* direction of rounding */
  double value;         /* value (rounded to integer with 'r' option) */
  int firstunit = 1;    /* first unit in a multi-unit string */
  int value_shown = 0;  /* has a value been shown? */
  int sigdigits;
  char val_sign;  

  initializeunit(&want);
  remainder = fabs(have->factor);
  val_sign = have->factor < 0 ? '-' : '+';
  lastunitstr = 0;
  nextunitstr = 0;
  round_dir = 0;

  if (flags.round) {
    /* disable unit repetition with terminal UNITSEPCHAR when rounding */
    if (lastchar(wantstr) == UNITSEPCHAR)
      lastchar(wantstr) = 0;
    if ((lastwantstr = strrchr(wantstr, UNITSEPCHAR)))
      lastwantstr++;
  }

  while (wantstr) {
    if ((nextunitstr = strchr(wantstr, UNITSEPCHAR)))
      *(nextunitstr++) = '\0';
    removespaces(wantstr);

    /*
      if wantstr ends in UNITSEPCHAR, repeat last unit--to give integer
      and fractional parts (3 oz + 0.371241 oz rather than 3.371241 oz)
    */
    if (emptystr(wantstr))              /* coverity[alias_transfer] */
      wantstr = lastunitstr;

    if (processunit(&want, wantstr, NOPOINT)) {
      freeunit(&want);
      return 1;
    }

    if (firstunit){
      /* checkunitlist() ensures conformability within 'wantstr',
         so we just need to check the first unit to see if it conforms
         to 'have' */
      if (compareunits(have, &want, ignore_dimless)) {
        showconformabilityerr(havestr, have, wantstr, &want);
        freeunit(&want);
        return 1;
      }

      /* round to nearest integral multiple of last unit */
      if (flags.round) {
        value = remainder;
        if (nonempty(lastwantstr)) {      /* more than one unit */
          removespaces(lastwantstr);
          initializeunit(&lastwant);
          if (processunit(&lastwant, lastwantstr, NOPOINT)) {
            freeunit(&lastwant);
            return 1;
          }
          remainder = round(remainder / lastwant.factor) * lastwant.factor;
        }
        else    /* first unit is last unit */
          remainder = round(remainder / want.factor) * want.factor;

        round_dir = remainder - value;
      }
      if (flags.verbose == 2) {
        removespaces(havestr);
        logprintf("\t%s = ", havestr);
      } else if (flags.verbose == 1)
        logputchar('\t');
    } /* end if first unit */

    if (0==(sigdigits = getsigdigits(have->factor, remainder, 10)))
      break;    /* nothing left */

    /* Remove sub-precision junk accumulating in the remainder.  Rounding
       is base 2 to ensure that we keep all valid bits. */
    remainder = round_digits(remainder,
                             getsigdigits(have->factor,remainder,2),2);
    if (nextunitstr)
      remainder = want.factor * modf(remainder / want.factor, &value);
    else
      value = remainder / want.factor;

    /* The remainder represents less than one of the current want unit.
       But with display rounding it may round up to 1, leading to an output
       like "4 feet + 12 inch".  Check for this case and if the remainder 
       indeed rounds up to 1 then add that remainder into the current unit 
       and set the remainder to zero. */
    if (nextunitstr){
      double rounded_next =
          round_digits(remainder/want.factor,
                       getsigdigits(have->factor, remainder / want.factor, 10),
                       10);
      if (displays_as(1,rounded_next, NULL)){
        value++;               
        remainder = 0;         /* Remainder is zero */
      }
    }

    /* Round the value to significant digits to prevent display
       of bogus sub-precision decimal digits */
    value = round_digits(value,sigdigits,10);

    /* This ensures that testing value against zero will work below
       at the last unit, which is the only case where value is not integer */
    if (!nextunitstr && displays_as(0, value, NULL))
      value=0;

    if (!flags.verbose){
      if (!firstunit) 
        logputchar(UNITSEPCHAR);
      logprintf(num_format.format,value);
      value_shown=1;
    } else { /* verbose case */
      if (value != 0) {
        if (value_shown) /* have already displayed a number so need a '+' */
          logprintf(" %c ",val_sign);
        else if (val_sign=='-')
          logputs("-");
        showunitname(value, wantstr, PRINTNUM);
        if (sigdigits <= floor(log10(value))+1)   /* Used all sig digits */
          logprintf(" (at %d-digit precision limit)", DBL_DIG);
        value_shown=1;
      }
    }
    
    freeunit(&want);
    lastunitstr = wantstr;
    wantstr = nextunitstr;
    firstunit = 0;
  }

  /* if the final unit value was rounded print indication */
  if (!value_shown) {  /* provide output if every value rounded to zero */
    logputs("0 ");
    if (isdecimal(*lastunitstr))
      logputs("* ");
    logputs(lastunitstr);
  }

  if (round_dir != 0) {
    if (flags.verbose){
      if (round_dir > 0)
        logprintf(" (rounded up to nearest %s) ", lastunitstr);
      else
        logprintf(" (rounded down to nearest %s) ", lastunitstr);
    } else 
      logprintf("%c%c", UNITSEPCHAR, round_dir > 0 ?'-':'+');   
  }
  logputchar('\n');
  return 0;
} /* end showunitlist */


#if defined (_WIN32) && defined (HAVE_MKS_TOOLKIT)
int
ismksmore(char *pager)
{
    static int mksmore = -1;

    if (mksmore >= 0)
      return mksmore;

    /*
      Tries to determine whether the MKS Toolkit version of more(1) or
      less(1) will run.  Neither accepts '+<lineno>', so if either will
      run, we need to give the option as '+<lineno>g'.
    */
    if (strstr(pager, "more") || strstr(pager, "less")) {
      char *mypager, *mkspager, *mksroot, *p;
      char pathbuf[FILENAME_MAX + 1];
      struct _stat mybuf, mksbuf;

      mypager = NULL;
      mkspager = NULL;
      mksmore = 0;
      if (strlen(pager) > FILENAME_MAX) {
        fprintf(stderr, "%s: cannot invoke pager--value '%s' in PAGER too long\n",
                progname, pager);
        return 0;       /* TODO: this really isn't the right value */
      }
      else if (!isfullpath(pager)) {
        mypager = (char *) mymalloc(strlen(pager) + strlen(EXE_EXT) + 1, "(ishelpquery)");
        strcpy(mypager, pager);
        if (!((p = strrchr(mypager, '.')) && isexe(p)))
          strcat(mypager, EXE_EXT);

        _searchenv(mypager, "PATH", pathbuf);
      }
      else
        strcpy(pathbuf, pager);

      mksroot = getenv("ROOTDIR");
      if (mksroot) {
        char * mksprog;

        if (strstr(pager, "more"))
          mksprog = "more.exe";
        else
          mksprog = "less.exe";
        mkspager = (char *) mymalloc(strlen(mksroot) + strlen("/mksnt/") + strlen(mksprog) + 1,
                                     "(ishelpquery)");
        strcpy(mkspager, mksroot);
        strcat(mkspager, "\\mksnt\\");
        strcat(mkspager, mksprog);
      }

      if (*pathbuf && mksroot) {
        if (_stat(mkspager, &mksbuf)) {
          fprintf(stderr, "%s: cannot stat file '%s'. ", progname, mkspager);
          perror((char *) NULL);
          return 0;
        }
        if (_stat(pathbuf, &mybuf)) {
          fprintf(stderr, "%s: cannot stat file '%s'. ", progname, pathbuf);
          perror((char *) NULL);
          return 0;
        }
        /*
          if we had inodes, this would be simple ... but if it walks
          like a duck and swims like a duck and quacks like a duck ...
        */
        if (mybuf.st_size   == mksbuf.st_size
          && mybuf.st_ctime == mksbuf.st_ctime
          && mybuf.st_mtime == mksbuf.st_mtime
          && mybuf.st_atime == mksbuf.st_atime
          && mybuf.st_mode  == mksbuf.st_mode)
            mksmore = 1;
      }
      if (mypager)
        free(mypager);
      if (mkspager)
        free(mkspager);
    }

    return mksmore;
}
#endif

/* 
   Checks to see if the input string contains HELPCOMMAND possibly
   followed by a unit name on which help is sought.  If not, then
   return 0.  Otherwise invoke the pager on units file at the line
   where the specified unit is defined.  Then return 1.  */

int
ishelpquery(char *str, struct unittype *have)
{
  struct unitlist *unit;
  struct func *function;
  struct wantalias *alias;
  struct prefixlist *prefix;
  char commandbuf[1000];  /* Hopefully this is enough overkill as no bounds */
  int unitline;           /* checking is performed. */
  char *file;
  char **exitptr;
  
  if (have && !strcmp(str, UNITMATCH)){
    tryallunits(have,0);
    return 1;
  }
  for(exitptr=exit_commands;*exitptr;exitptr++)
    if (!strcmp(str, *exitptr))
      exit(EXIT_SUCCESS);
  if (startswith(str, SEARCHCOMMAND)){
    str+=strlen(SEARCHCOMMAND);
    if (!emptystr(str) && *str != ' ')
      return 0;
    removespaces(str);
    if (emptystr(str)){
      printf("\n\
Type 'search text' to see a list of all unit names \n\
containing 'text' as a substring\n\n");
      return 1;
    }
    tryallunits(0,str);
    return 1;
  }
  if (startswith(str, HELPCOMMAND)){
    str+=strlen(HELPCOMMAND);
    if (!emptystr(str) && *str != ' ')
      return 0;
    removespaces(str);

    if (emptystr(str)){
      int nlines;
      char *unitsfile;
      char *msg = "\n\
%s converts between different measuring systems and    %s6 inches\n\
acts as a units-aware calculator.  At the '%s'    %scm\n\
prompt, type in the units you want to convert from or             * 15.24\n\
an expression to evaluate.  At the '%s' prompt,           / 0.065\n\
enter the units to convert to or press return to see\n\
the reduced form or definition.                           %stempF(75)\n\
                                                          %stempC\n\
The first example shows that 6 inches is about 15 cm              23.889\n\
or (1/0.065) cm.  The second example shows how to\n\
convert 75 degrees Fahrenheit to Celsius.  The third      %sbu^(1/3)\n\
example converts the cube root of a bushel to a list      %sft;in\n\
of semicolon-separated units.                                     1 ft + 0.9 in\n\
\n\
To quit from %s type 'quit' or 'exit'.       %s2 btu + 450 ft lbf\n\
                                                %s(kg^2/s)/(day lb/m^2)\n\
At the '%s' prompt type '%s' to get a            * 1.0660684e+08\n\
list of conformable units.  At either prompt you        / 9.3802611e-09\n\
type 'help myunit' to browse the units database\n\
and read the comments relating to myunit or see         %s6 tbsp sugar\n\
other units related to myunit.  Typing 'search          %sg\n\
text' will show units whose names contain 'text'.               * 75\n\
                                                                / 0.013333333\n\n";
    char *fmsg = "To learn about the available units look in '%s'\n\n";
    FILE *fp;

    /* presumably, this cannot fail because it was already checked at startup */
    unitsfile = findunitsfile(NOERRMSG);

    nlines = countlines(msg);
    /* but check again anyway ... */
    if (unitsfile)
      nlines += countlines(fmsg);

    /* see if we need a pager */
    fp = get_output_fp(nlines);

    fprintf(fp, msg, 
      progname, QUERYHAVE,
      QUERYHAVE, QUERYWANT,
      QUERYWANT,
      QUERYHAVE,QUERYWANT,QUERYHAVE,QUERYWANT,
      progname, QUERYHAVE,QUERYWANT,
      QUERYWANT,
      UNITMATCH,
      QUERYHAVE,QUERYWANT);

      if (unitsfile)
        fprintf(fp, fmsg, unitsfile);

      if (fp != stdout)
        pclose(fp);

      return 1;
    }
    if ((function = fnlookup(str))){
      file = function->file;
      unitline = function->linenumber;
    }
    else if ((unit = ulookup(str))){
      unitline = unit->linenumber;
      file = unit->file;
    }
    else if ((prefix = plookup(str)) && strlen(str)==prefix->len){
      unitline = prefix->linenumber;
      file = prefix->file;
    }
    else if ((alias = aliaslookup(str))){
      unitline = alias->linenumber;
      file = alias->file;
    }
    else {
      printf("Unknown unit '%s'\n",str);
      return 1;
    }

#if defined (_WIN32) && defined (HAVE_MKS_TOOLKIT)
    if (ismksmore(pager))
      /*
        inner escaped quotes are necessary for filenames with spaces;
        outer escaped quotes are necessary for cmd.exe to see the
        command as a single string containing one or more quoted strings
        (e.g., cmd /c ""command" "arg1" "arg2" ... ")
      */
      sprintf(commandbuf,"\"\"%s\" +%dg \"%s\"\"", pager, unitline, file);
    else
      /* more.com seems to have positioning problems, and it can't back up */
      sprintf(commandbuf,"\"\"%s\"  +%d \"%s\"\"", pager,
              unitline > 2 ? unitline - 3 : unitline, file);
#elif defined (_WIN32)
    sprintf(commandbuf,"\"\"%s\" +%d \"%s\"\"", pager,
            unitline > 2 ? unitline - 3 : unitline, file);
#else
    sprintf(commandbuf,"%s +%d %s", pager, unitline, file);
#endif
    if (system(commandbuf))
      fprintf(stderr,"%s: cannot invoke pager '%s' to display help\n", 
              progname, pager);
    return 1;
  }
  return 0;
}

#ifdef SUPPORT_UTF8
void
checklocale()
{
  char *temp;
  temp = setlocale(LC_CTYPE,"");
  utf8mode = (strcmp(nl_langinfo(CODESET),"UTF-8")==0);
  if (temp){
    mylocale = dupstr(temp);
    temp = strchr(mylocale,'.');
    if (temp)
      *temp = 0;
  } else
    mylocale = DEFAULTLOCALE;
}

#else

void
checklocale()
{
  char *temp=0;
#ifndef NO_SETLOCALE  
  temp = setlocale(LC_CTYPE,"");
#endif
  if (!temp)
    temp = getenv("LC_CTYPE");
  if (!temp)
    temp = getenv("LANG");
  if (!temp)
    mylocale = DEFAULTLOCALE;
  else {
    mylocale = dupstr(temp);  
    temp = strchr(mylocale,'.');
    if (temp)
      *temp = 0;
  }
}

#endif

/* 
   Replaces an alias in the specified string input.  Returns 1 if the
   alias that is found contains errors. 
*/

int
replacealias(char **string, int *buflen)
{
  int usefree = 1;
  struct wantalias *aliasptr;
  char *input;

  if (!flags.readline && buflen)
    usefree = 0;

  if (nonempty(*string)) {  /* check that string is defined and nonempty */
    input = *string;
    removespaces(input);
    if ((aliasptr=aliaslookup(input))){
      if (checkunitlist(aliasptr->definition,NOERRMSG)){
        puts("Unit list definition contains errors.");
        return 1;
      }
      if (usefree){
        free(*string);
        *string = dupstr(aliasptr->definition);
      } else {                           /* coverity[dead_error_line] */
        while (strlen(aliasptr->definition)>*buflen)
          growbuffer(string, buflen);
        strcpy(*string, aliasptr->definition);
      }
    }
  }
  return 0;
}


/* 
   Remaps the locale name returned on Windows systems to the value
   returned on Unix-like systems
*/

void
remaplocale(char *filename)
{
  FILE *map;
  char *value;
  char name[80];

  map = openfile(filename,"rt");
  if (!map) {
    fprintf(stderr,"%s: cannot open locale map '%s'. ",progname,filename);
    perror((char *) NULL);
  }
  else {
    while(!feof(map)){
      if (!fgets(name,80,map))
        break;
      lastchar(name) = 0;
      value=strchr(name,'#');
      if (value) *value=0;
      value=strchr(name,'\t');
      if (!value) continue;
      *value++=0;
      removespaces(value);
      removespaces(name);
      if (!strcmp(name, mylocale))
        mylocale = dupstr(value);
    }
    fclose(map);
  }
}


void
close_logfile(void)
{
  if (logfile){
    fputc('\n',logfile);
    fclose(logfile);
  }
}


void
open_logfile(void)
{  
  time_t logtime;
  char * timestr;

  logfile = openfile(logfilename, "at");
  if (!logfile){
    fprintf(stderr, "%s: cannot write to log file '%s'.  ", 
            progname, logfilename);
    perror(0);
    exit(EXIT_FAILURE);
  }
  time(&logtime);
  timestr = ctime(&logtime);
  fprintf(logfile, "### Log started %s \n", timestr);
  atexit(close_logfile);
}


void
write_files_sig(int sig)
{
#ifdef READLINE
  if (historyfile)
    save_history();
#endif
  close_logfile();
  signal(sig, SIG_DFL);
  raise(sig);
}


int
unitsHandler(int argc, char **argv)
{
   static struct unittype have, want;
   char *havestr=0, *wantstr=0;
   struct func *funcval;
   struct wantalias *alias;
   int havestrsize=0;   /* Only used if READLINE is undefined */
   int wantstrsize=0;   /* Only used if READLINE is undefined */
   int readerr;
   char **unitfileptr;
   int unitcount=0, prefixcount=0, funccount=0;   /* for counting units */
   char *queryhave, *querywant, *comment;
   int queryhavewidth, querywantwidth;
#ifdef _WIN32
   char *localemap;
#endif

   /* Set program parameter defaults */
   num_format.format = NULL;
   num_format.precision = DEFAULTPRECISION;
   num_format.type = DEFAULTTYPE;

   flags.quiet = 0;       /* Do not supress prompting */
   flags.unitcheck = 0;   /* Unit checking is off */
   flags.verbose = 1;     /* Medium verbosity */
   flags.round = 0;       /* Rounding off */
   flags.strictconvert=0; /* Strict conversion disabled (reciprocals active) */
   flags.unitlists = 1;   /* Multi-unit conversion active */
   flags.oneline = 0;     /* One line output is disabled */
   flags.showconformable=0;  /* show unit conversion rather than all conformable units */
   flags.showfactor = 0;  /* Don't show a multiplier for a 1|x fraction */
                          /*       in unit list output */
   parserflags.minusminus = 1;  /* '-' character gives subtraction */
   parserflags.oldstar = 0;     /* '*' has same precedence as '/' */

   progname = getprogramname(argv[0]);

   /*
     unless UNITSFILE and LOCALEMAP have absolute pathnames, we may need
     progdir to search for supporting files
   */
   if (!(isfullpath(UNITSFILE) && isfullpath(LOCALEMAP)))
     progdir = getprogdir(argv[0], &fullprogname);
   else {
     progdir = NULL;
     fullprogname = NULL;
   }
   datadir = getdatadir();      /* directory to search as last resort */

   checklocale();

   homedir = findhome(&homedir_error);

#ifdef READLINE
#  if RL_READLINE_VERSION > 0x0402 
      rl_completion_entry_function = (rl_compentry_func_t *)completeunits;
#  else
      rl_completion_entry_function = (Function *)completeunits;
#  endif
   rl_basic_word_break_characters = " \t+-*/()|^;";
   flags.readline = isatty(0);
   if (flags.readline){
     int file_exists;
     historyfile = personalfile(NULL,HISTORY_FILE,1,&file_exists);
   }
#else
   flags.readline = 0;
#endif

   unitsfiles[0] = 0;

#ifdef _WIN32
   if (!strcmp(homeunitsfile,".units"))
     homeunitsfile = "unitdef.units";
#endif

   pager = getenv("PAGER");
   if (!pager)
     pager = DEFAULTPAGER;

   flags.interactive = processargs(argc, argv, &havestr, &wantstr);

#ifdef READLINE   
   if (flags.interactive && flags.readline && historyfile){
     rl_initialize();
     read_history(historyfile);
     init_history_length = history_length;
     init_history_base = history_base;
     atexit(save_history);
   }
#endif

   signal(SIGINT, write_files_sig);
   signal(SIGTERM, write_files_sig);

   if (logfilename) {
     if (!flags.interactive)
       fprintf(stderr,
               "Log file '%s' ignored in non-interactive mode.\n",logfilename);
     else open_logfile();
   }
  
   /* user has specified the complete format--use it */
   if (num_format.format != NULL) {
       if (parsenumformat())
         return EXIT_FAILURE;
   }
   else
       setnumformat();

   if (flags.verbose==0)
     deftext = "";

   if (!unitsfiles[0]){
     char *unitsfile;
     unitsfile = findunitsfile(ERRMSG);
     if (!unitsfile)
       return EXIT_FAILURE;
     else {
       int file_exists;

       unitsfiles[0] = unitsfile;
       unitsfiles[1] = personalfile(HOME_UNITS_ENV,homeunitsfile, 
                                         0, &file_exists);
       unitsfiles[2] = 0;
     }
   }

#ifdef _WIN32
   localemap = findlocalemap(ERRMSG);
   if (localemap)
     remaplocale(localemap);
#endif
 
   if (!hasLoadedUnits) {
		for(unitfileptr=unitsfiles;*unitfileptr;unitfileptr++){
		 readerr = readunits(*unitfileptr, stderr, &unitcount, &prefixcount, 
							 &funccount, 0);
		 if (readerr==E_MEMORY || readerr==E_FILE) 
		   return EXIT_FAILURE;
	   }   
   }

   if (flags.quiet)
     queryhave = querywant = "";   /* No prompts are being printed */
   else {
     if (!promptprefix){
       queryhave = QUERYHAVE;
       querywant = QUERYWANT;
     } else {
       queryhave = (char *)mymalloc(strlen(promptprefix)+strlen(QUERYHAVE)+1,
                                    "(main)");
       querywant = (char *)mymalloc(strlen(promptprefix)+strlen(QUERYWANT)+1,
                                    "(main)");
       strcpy(queryhave, promptprefix);
       strcat(queryhave, QUERYHAVE);
       memset(querywant, ' ', strlen(promptprefix));
       strcpy(querywant+strlen(promptprefix), QUERYWANT);
     }
     printf("%d units, %d prefixes, %d nonlinear units\n\n", 
            unitcount, prefixcount,funccount);
   }
   queryhavewidth = strwidth(queryhave);
   querywantwidth = strwidth(querywant);

   if (flags.unitcheck) {
     checkunits(flags.unitcheck==2 || flags.verbose==2);
     return EXIT_SUCCESS;
   }

   if (!flags.interactive) {
     replacectrlchars(havestr);
     if (wantstr)
       replacectrlchars(wantstr);
#ifdef SUPPORT_UTF8
     if (strwidth(havestr)<0){
       printf("Error: %s on input\n",invalid_utf8);
       return EXIT_FAILURE;
     }
     if (wantstr && strwidth(wantstr)<0){
       printf("Error: %s on input\n",invalid_utf8);
       return EXIT_FAILURE;
     }
#endif
     replace_minus(havestr);
     removespaces(havestr);
     if (wantstr) {
       replace_minus(wantstr);
       removespaces(wantstr);
     }
     if ((funcval = fnlookup(havestr))){
       showfuncdefinition(funcval, FUNCTION);
       return EXIT_SUCCESS;
     }
     if ((funcval = invfnlookup(havestr))){
       showfuncdefinition(funcval, INVERSE);
       return EXIT_SUCCESS;
     }
     if ((alias = aliaslookup(havestr))){
       showunitlistdef(alias);
       return EXIT_SUCCESS;
     }
     if (processunit(&have, havestr, NOPOINT))
       return EXIT_FAILURE;
     if (flags.showconformable == 1) {
       tryallunits(&have,0);
       return EXIT_SUCCESS;
     }
     if (!wantstr){
       showdefinition(havestr,&have);
       return EXIT_SUCCESS;
     }
     if (replacealias(&wantstr, 0)) /* the 0 says that we can free wantstr */
       return EXIT_FAILURE;
     if ((funcval = fnlookup(wantstr))){
       if (showfunc(havestr, &have, funcval))  /* Clobbers have */
         return EXIT_FAILURE;
       else
         return EXIT_SUCCESS;
     }
     if (processwant(&want, wantstr, NOPOINT))
       return EXIT_FAILURE;
     if (strchr(wantstr, UNITSEPCHAR)){
       if (showunitlist(havestr, &have, wantstr))
         return EXIT_FAILURE;
       else
         return EXIT_SUCCESS;
     }
     if (showanswer(havestr,&have,wantstr,&want))
       return EXIT_FAILURE;
     else
       return EXIT_SUCCESS;
   } else {       /* interactive */
     for (;;) {
       do {
         fflush(stdout);
         getuser(&havestr,&havestrsize,queryhave);
         replace_minus(havestr);
         comment = strip_comment(havestr);
         removespaces(havestr);
         if (logfile && comment && emptystr(havestr))
           fprintf(logfile, "#%s\n", comment);
       } while (emptystr(havestr) || ishelpquery(havestr,0) ||
                (!fnlookup(havestr) && !invfnlookup(havestr)
                 && !aliaslookup(havestr) 
                 && processunit(&have, havestr, queryhavewidth)));
       if (logfile) {
         if (comment)
           fprintf(logfile, "%s%s\t#%s\n", LOGFROM, havestr,comment);
         else
           fprintf(logfile, "%s%s\n", LOGFROM, havestr);
       }
       if ((alias = aliaslookup(havestr))){
         showunitlistdef(alias);
         continue;
       }
       if ((funcval = fnlookup(havestr))){
         showfuncdefinition(funcval, FUNCTION);
         continue;
       }
       if ((funcval = invfnlookup(havestr))){
         showfuncdefinition(funcval, INVERSE);
         continue;
       }
       do { 
         int repeat; 
         do {
           repeat = 0;
           fflush(stdout);
           getuser(&wantstr,&wantstrsize,querywant);
           replace_minus(wantstr);
           comment = strip_comment(wantstr);
           removespaces(wantstr);
           if (logfile && comment && emptystr(wantstr)){
             fprintf(logfile, "#%s\n", comment);
             repeat = 1;
           }
           if (ishelpquery(wantstr, &have)){
             repeat = 1;
             printf("%s%s\n",queryhave, havestr);
           }
         } while (repeat);
       } while (replacealias(&wantstr, &wantstrsize)
                || (!fnlookup(wantstr)
                    && processwant(&want, wantstr, querywantwidth)));
       if (logfile) {
         fprintf(logfile, "%s", LOGTO);
         tightprint(logfile, wantstr);
         if (comment)
           fprintf(logfile, "\t#%s", comment);
         putc('\n', logfile);
       }
       if (emptystr(wantstr))
         showdefinition(havestr,&have);
       else if (strchr(wantstr, UNITSEPCHAR))
         showunitlist(havestr, &have, wantstr);
       else if ((funcval = fnlookup(wantstr)))
         showfunc(havestr, &have, funcval); /* Clobbers have */
       else {
         showanswer(havestr,&have,wantstr, &want);
         freeunit(&want);
       }
       unitcopy(&lastunit, &have);
       lastunitset=1;
       freeunit(&have);
     }
   }
   return (0);
}

/* NOTES:

mymalloc, growbuffer and tryallunits are the only places with print
statements that should (?) be removed to make a library.  How can
error reporting in these functions (memory allocation errors) be
handled cleanly for a library implementation?  

Way to report the reduced form of the two operands of a sum when
they are not conformable.

Way to report the required domain when getting an domain error.

*/