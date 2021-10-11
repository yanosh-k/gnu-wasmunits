/*
 *  parse.y: the parser for GNU units, a program for units conversion
 *  Copyright (C) 1999-2002, 2007, 2009, 2014, 2017-2018, 2020 Free Software 
 *  Foundation, Inc
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


%{
#include<stdio.h>
#include "units.h"

struct commtype {
   int location;
   const char *data;
   struct unittype *result;
   int errorcode;
};

static int err;  /* value used by parser to store return values */

/* 
   The CHECK macro aborts parse if an error has occurred.  It optionally
   destroys a variable.  Call with CHECK(0) if no variables need destruction 
   on error. 
*/
 
#define CHECK(var) if (err) { comm->errorcode=err; \
                              if (var) destroyunit(var); \
                              YYABORT; }
 
int yylex();
void yyerror(struct commtype *comm, char *);

#define MAXMEM 100
int unitcount=0;    /* Counts the number of units allocated by the parser */

struct function { 
   char *name; 
   double (*func)(double); 
   int type;
}; 

#define DIMENSIONLESS 0
#define ANGLEIN 1
#define ANGLEOUT 2
#define NATURAL 3

struct unittype *
getnewunit()
{
  struct unittype *unit;

  if (unitcount>=MAXMEM)
    return 0;
  unit = (struct unittype *) 
    mymalloc(sizeof(struct unittype),"(getnewunit)");
  if (!unit)
    return 0;
  initializeunit(unit);
  unitcount++;
  return unit;
}


void
destroyunit(struct unittype *unit)
{
  freeunit(unit);
  free(unit);
  unitcount--;
}  
 

struct unittype *
makenumunit(double num,int *myerr)
{
  struct unittype *ret;
  ret=getnewunit();
  if (!ret){
    *myerr = E_PARSEMEM;
    return 0;  
  }
  ret->factor = num;
  *myerr = 0;
  return ret;
}

int
logunit(struct unittype *theunit, int base)
{  
  if ((err=unit2num(theunit)))
    return err;
  if (base==2)
    theunit->factor = log2(theunit->factor);
  else if (base==10)
    theunit->factor = log10(theunit->factor);
  else
    theunit->factor = log(theunit->factor)/log((double)base);
  if (errno)
    return E_FUNC;
  return 0;
}
 
int
funcunit(struct unittype *theunit, struct function const *fun)
{
  struct unittype angleunit;
  if (fun->type==ANGLEIN){
    err=unit2num(theunit);
    if (err==E_NOTANUMBER){
      initializeunit(&angleunit);
      angleunit.denominator[0] = dupstr("radian");
      angleunit.denominator[1] = 0;
      err = multunit(theunit, &angleunit);
      freeunit(&angleunit);
      if (!err)
        err = unit2num(theunit);
    }
    if (err)
      return err;
  } else if (fun->type==ANGLEOUT || fun->type == DIMENSIONLESS || fun->type == NATURAL) {
    if ((err=unit2num(theunit)))
      return err;
    if (fun->type==NATURAL && (theunit->factor<0 || trunc(theunit->factor)!=theunit->factor))
      return E_NOTINDOMAIN;
  } else 
     return E_BADFUNCTYPE;
  errno = 0;
  theunit->factor = (*(fun->func))(theunit->factor);
  if (errno)
    return E_FUNC;
  if (fun->type==ANGLEOUT) {
    theunit->numerator[0] = dupstr("radian");
    theunit->numerator[1] = 0;
  }
  return 0;
}


%}

%parse-param {struct commtype *comm}
%lex-param {struct commtype *comm}
%define api.pure full
%define api.prefix {units}

%union {
  double number;
  int integer;
  struct unittype *unit;
  struct function *realfunc;
  struct func *unitfunc;
}

%token <number> REAL
%token <unit> UNIT
%token <realfunc> REALFUNC
%token <integer> LOG
%token <unitfunc> UNITFUNC
%token <integer> EXPONENT
%token <integer> MULTIPLY
%token <integer> MULTSTAR
%token <integer> DIVIDE
%token <integer> NUMDIV
%token <integer> SQRT
%token <integer> CUBEROOT
%token <integer> MULTMINUS
%token <integer> EOL
%token <integer> FUNCINV
%token <integer> MEMERROR
%token <integer> BADNUMBER
%token <integer> UNITEND
%token <integer> LASTUNSET

%type <number> numexpr
%type <unit> expr
%type <unit> list
%type <unit> pexpr
%type <unit> unitexpr

%destructor { destroyunit($$);} <unit>

%left ADD MINUS
%left UNARY
%left DIVIDE MULTSTAR
%left MULTIPLY MULTMINUS
%nonassoc '(' SQRT CUBEROOT REALFUNC LOG UNIT REAL UNITFUNC FUNCINV MEMERROR BADNUMBER UNITEND LASTUNSET
%right EXPONENT
%left NUMDIV


%%
 input: EOL           { comm->result = makenumunit(1,&err); CHECK(0);
                       comm->errorcode = 0; YYACCEPT; }
      | unitexpr EOL { comm->result = $1; comm->errorcode = 0; YYACCEPT; }
      | error        { YYABORT; }
      ;

 unitexpr:  expr                    { $$ = $1;}
         |  DIVIDE list             { invertunit($2); $$=$2;}
         ;

 expr: list                         { $$ = $1; }
     | MULTMINUS list %prec UNARY   { $$ = $2; $$->factor *= -1; }
     | MINUS list %prec UNARY       { $$ = $2; $$->factor *= -1; }
     | expr ADD expr                { err = addunit($1,$3); destroyunit($3);
                                      CHECK($1);$$=$1;}
     | expr MINUS expr              { $3->factor *= -1;
                                      err = addunit($1,$3); destroyunit($3);
                                      CHECK($1);$$=$1;}
     | expr DIVIDE expr             { err = divunit($1, $3); destroyunit($3);
                                      CHECK($1);$$=$1;}
     | expr MULTIPLY expr           { err = multunit($1,$3); destroyunit($3);
                                      CHECK($1);$$=$1;}
     | expr MULTSTAR expr           { err = multunit($1,$3); destroyunit($3);
                                      CHECK($1);$$=$1;}
     ; 

 numexpr:  REAL                     { $$ = $1;         }
         | numexpr NUMDIV numexpr   { $$ = $1 / $3;    }
     ;

 pexpr: '(' expr ')'                { $$ = $2;  }
       ;

 /* list is a list of units, possibly raised to powers, to be multiplied
    together. */

list:  numexpr                     { $$ = makenumunit($1,&err); CHECK(0);}
      | UNIT                       { $$ = $1; }
      | list EXPONENT list         { err = unitpower($1,$3);destroyunit($3);
                                     CHECK($1);$$=$1;}
      | list MULTMINUS list        { err = multunit($1,$3); destroyunit($3);
                                     CHECK($1);$$=$1;}
      | list list %prec MULTIPLY   { err = multunit($1,$2); destroyunit($2);
                                     CHECK($1);$$=$1;}
      | pexpr                      { $$=$1; }
      | SQRT pexpr                 { err = rootunit($2,2); CHECK($2); $$=$2;}
      | CUBEROOT pexpr             { err = rootunit($2,3); CHECK($2); $$=$2;}
      | REALFUNC pexpr             { err = funcunit($2,$1);CHECK($2); $$=$2;}
      | LOG pexpr                  { err = logunit($2,$1); CHECK($2); $$=$2;}
      | UNITFUNC pexpr             { err = evalfunc($2,$1,0,0); CHECK($2);$$=$2;}
      | FUNCINV UNITFUNC pexpr     { err = evalfunc($3,$2,1,0); CHECK($3);$$=$3;}
      | list EXPONENT MULTMINUS list %prec EXPONENT  
                                   { $4->factor *= -1; err = unitpower($1,$4);
                                     destroyunit($4);CHECK($1);$$=$1;}
      | list EXPONENT MINUS list %prec EXPONENT  
                                   { $4->factor *= -1; err = unitpower($1,$4);
                                     destroyunit($4);CHECK($1);$$=$1;}
      | BADNUMBER                  { err = E_BADNUM;   CHECK(0); }
      | MEMERROR                   { err = E_PARSEMEM; CHECK(0); }        
      | UNITEND                    { err = E_UNITEND;  CHECK(0); }
      | LASTUNSET                  { err = E_LASTUNSET;CHECK(0); }
      | FUNCINV UNIT               { err = E_NOTAFUNC; CHECK($2);}
   ;

%%

double
factorial(double x)
{
  return tgamma(x+1);
}  

struct function 
  realfunctions[] = { {"sin", sin,    ANGLEIN},
                      {"cos", cos,    ANGLEIN},
                      {"tan", tan,    ANGLEIN},
                      {"ln", log,     DIMENSIONLESS},
                      {"log", log10,  DIMENSIONLESS},
                      {"exp", exp,    DIMENSIONLESS},
                      {"acos", acos,  ANGLEOUT},
                      {"atan", atan,  ANGLEOUT},
                      {"asin", asin,  ANGLEOUT},
		      {"sinh", sinh, DIMENSIONLESS},
		      {"cosh", cosh, DIMENSIONLESS},		      
		      {"tanh", tanh, DIMENSIONLESS},
		      {"asinh", asinh, DIMENSIONLESS},
		      {"acosh", acosh, DIMENSIONLESS},		      
		      {"atanh", atanh, DIMENSIONLESS},
                      {"round", round, DIMENSIONLESS},
                      {"floor", floor, DIMENSIONLESS},
                      {"ceil", ceil, DIMENSIONLESS},
                      {"abs", fabs, DIMENSIONLESS},
                      {"erf", erf, DIMENSIONLESS},
                      {"erfc", erfc, DIMENSIONLESS},
                      {"Gamma", tgamma, DIMENSIONLESS},
                      {"lnGamma", lgamma, DIMENSIONLESS},
                      {"factorial", factorial, NATURAL},
                      {0, 0, 0}};

struct {
  char op;
  int value;
} optable[] = { {'*', MULTIPLY},
                {'/', DIVIDE},
                {'|', NUMDIV},
                {'+', ADD},
                {'(', '('},
                {')', ')'},
                {'^', EXPONENT},
                {'~', FUNCINV},
                {0, 0}};

struct {
  char *name;
  int value;
} strtable[] = { {"sqrt", SQRT},
                 {"cuberoot", CUBEROOT},
                 {"per" , DIVIDE},
                 {0, 0}};

#define LASTUNIT '_'     /* Last unit symbol */


int yylex(YYSTYPE *lvalp, struct commtype *comm)
{
  int length, count;
  struct unittype *output;
  const char *inptr;
  char *name;

  char *nonunitchars = "~;+-*/|\t\n^ ()"; /* Chars not allowed in unit name --- also defined in units.c */
  char *nonunitends = ".,_";              /* Can't start or end a unit */
  char *number_start = ".,0123456789";    /* Can be first char of a number */
  
  if (comm->location==-1) return 0;
  inptr = comm->data + comm->location;   /* Point to start of data */

  /* Skip spaces */
  while(*inptr==' ') inptr++, comm->location++;

  if (*inptr==0) {
    comm->location = -1;
    return EOL;  /* Return failure if string has ended */
  }  

  /* Check for **, an exponent operator.  */

  if (0==strncmp("**",inptr,2)){
    comm->location += 2;
    return EXPONENT;
  }

  /* Check for '-' and '*' which get special handling */

  if (*inptr=='-'){
    comm->location++;
    if (parserflags.minusminus)
      return MINUS;
    return MULTMINUS;
  }      

  if (*inptr=='*'){
    comm->location++;
    if (parserflags.oldstar)
      return MULTIPLY;
    return MULTSTAR;
  }      

  /* Check for the "last unit" symbol */ 

  if (*inptr == LASTUNIT) {
    comm->location++;
    if (!lastunitset) 
      return LASTUNSET;
    output = getnewunit();
    if (!output)
      return MEMERROR;
    unitcopy(output, &lastunit);
    lvalp->unit = output;
    return UNIT;
  } 

  /* Look for single character ops */

  for(count=0; optable[count].op; count++){
    if (*inptr==optable[count].op) {
       comm->location++;
       return optable[count].value;
    }
  }

  /* Look for numbers */

  if (strchr(number_start,*inptr)){  /* prevent "nan" from being recognized */
    char *endloc;
    lvalp->number = strtod(inptr, &endloc);
    if (inptr != endloc) { 
      comm->location += (endloc-inptr);
      if (*endloc && strchr(number_start,*endloc))
        return BADNUMBER;
      else
        return REAL;
    }
  }

  /* Look for a word (function name or unit name) */

  length = strcspn(inptr,nonunitchars);   

  if (!length){  /* Next char is not a valid unit char */
     comm->location++;
     return 0;
  }

  /* Check that unit name doesn't start or end with forbidden chars */
  if (strchr(nonunitends,*inptr)){
    comm->location++;
    return 0;
  }
  if (strchr(nonunitends, inptr[length-1])){
    comm->location+=length;
    return 0;
  }

  name = dupnstr(inptr, length);

  /* Look for string operators */

  for(count=0;strtable[count].name;count++){
    if (!strcmp(name,strtable[count].name)){
      free(name);
      comm->location += length;
      return strtable[count].value;
    }
  }
  
  /* Look for real function names */

  for(count=0;realfunctions[count].name;count++){
    if (!strcmp(name,realfunctions[count].name)){
      lvalp->realfunc = realfunctions+count;
      comm->location += length;
      free(name);
      return REALFUNC;
    }
  }

  /* Check for arbitrary base log */
  
  if (!strncmp(name, "log",3)){
    count = strspn(name+3,"1234567890");
    if (count+3 == strlen(name)){
      lvalp->integer=atoi(name+3);
      if (lvalp->integer>1){      /* Log base must be larger than 1 */
	comm->location += length;
	free(name);
	return LOG;
      }
    }
  }
      
  /* Look for function parameter */

  if (function_parameter && !strcmp(name,function_parameter)){
    free(name);
    output = getnewunit();
    if (!output)
      return MEMERROR;
    unitcopy(output, parameter_value);
    lvalp->unit = output;
    comm->location += length;
    return UNIT;
  } 

  /* Look for user defined function */

  lvalp->unitfunc = fnlookup(name);
  if (lvalp->unitfunc){
    comm->location += length;
    free(name);
    return UNITFUNC;
  }

  /* Didn't find a special string, so treat it as unit name */

  comm->location+=length;
  if (strchr("23456789",inptr[length-1]) && !hassubscript(name)) {
    /* ends with digit but not a subscript, so do exponent handling like m3 */
    count = name[length-1] - '0';
    length--;
    if (strchr(number_start, name[length-1])){
      free(name);
      return UNITEND;
    }
  } else count=1;

  free(name);
    
  output = getnewunit();
  if (!output)
    return MEMERROR;
  output->numerator[count--]=0;
  for(;count>=0;count--)
    output->numerator[count] = dupnstr(inptr, length);
  lvalp->unit=output;
  return UNIT;
}


void yyerror(struct commtype *comm, char *s){}


int
parseunit(struct unittype *output, char const *input,char **errstr,int *errloc)
{
  struct commtype comm;
  int saveunitcount;

  saveunitcount = unitcount;
  initializeunit(output);
  comm.result = 0;
  comm.location = 0;
  comm.data = input;
  comm.errorcode = E_PARSE;    /* Assume parse error */
  errno=0;
  if (yyparse(&comm) || errno){
    if (comm.location==-1) 
      comm.location = strlen(input);
    if (errstr){
      if (comm.errorcode==E_FUNC || errno)
        *errstr = strerror(errno);
      else
        *errstr=errormsg[comm.errorcode];
    }
    if (errloc)
      *errloc = comm.location;
    if (unitcount!=saveunitcount)
      fprintf(stderr,"units: Parser leaked memory with error: %d in %d out\n",
             saveunitcount, unitcount);
    return comm.errorcode;
  } else {
    if (errstr)
      *errstr = 0;
    multunit(output,comm.result);
    destroyunit(comm.result);
    if (unitcount!=saveunitcount)
      fprintf(stderr,"units: Parser leaked memory without error: %d in %d out\n",
	      saveunitcount, unitcount);
    return 0;
  }
}


