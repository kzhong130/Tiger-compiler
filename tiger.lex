%{
/* Lab2 Attention: You are only allowed to add code in this file and start at Line 26.*/
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "errormsg.h"
#include "absyn.h"
#include "y.tab.h"

int charPos=1;

int yywrap(void)
{
 charPos=1;
 return 1;
}

void adjust(void)
{
 EM_tokPos=charPos;
 charPos+=yyleng;
}

/*
* Please don't modify the lines above.
* You can add C declarations of your own below.
*/

/* @function: getstr
 * @input: a string literal
 * @output: the string value for the input which has all the escape sequences 
 * translated into their meaning.
 */
char *getstr(const char *str)
{
	//optional: implement this function if you need it
	return NULL;
}


char buf[1024];
int pos;
int num = 0;

%}
  /* You can add lex definitions here. */

letter [a-zA-Z]
digit [0-9]

%Start INIT COMMENT STR

%%

<INIT>while {adjust(); return WHILE;}
<INIT>for {adjust(); return FOR;}
<INIT>to {adjust(); return TO;}
<INIT>break {adjust(); return BREAK;}
<INIT>let {adjust(); return LET;}
<INIT>in {adjust(); return IN;}
<INIT>end {adjust(); return END;}
<INIT>function {adjust(); return FUNCTION;}
<INIT>var {adjust(); return VAR;}
<INIT>type {adjust(); return TYPE;}
<INIT>array {adjust(); return ARRAY;}
<INIT>if {adjust(); return IF;}
<INIT>then {adjust(); return THEN;}
<INIT>else {adjust(); return ELSE;}
<INIT>do {adjust(); return DO;}
<INIT>of {adjust(); return OF;}
<INIT>nil {adjust(); return NIL;}


<INIT>:= {adjust(); return ASSIGN;}
<INIT><> {adjust(); return NEQ;}
<INIT><= {adjust(); return LE;}
<INIT>>= {adjust(); return GE;}
<INIT>< {adjust(); return LT;}
<INIT>> {adjust(); return GT;}
<INIT>= {adjust(); return EQ;}
<INIT>, {adjust(); return COMMA;}
<INIT>: {adjust(); return COLON;}
<INIT>; {adjust(); return SEMICOLON;}
<INIT>"(" {adjust(); return LPAREN;}
<INIT>")" {adjust(); return RPAREN;}
<INIT>"[" {adjust(); return LBRACK;}
<INIT>"]" {adjust(); return RBRACK;}
<INIT>"{" {adjust(); return LBRACE;}
<INIT>"}" {adjust(); return RBRACE;}
<INIT>"." {adjust(); return DOT;}
<INIT>"+" {adjust(); return PLUS;}
<INIT>"-" {adjust(); return MINUS;}
<INIT>"*" {adjust(); return TIMES;}
<INIT>"/" {adjust(); return DIVIDE;}
<INIT>"&" {adjust(); return AND;}
<INIT>"|" {adjust(); return OR;}



<INIT>"/*" {adjust(); BEGIN COMMENT;}

<INIT>\" {adjust(); pos = 0; BEGIN STR;}

<INIT>" "|\t {adjust(); continue;} 
<INIT>\n {adjust(); EM_newline(); continue;}

<INIT>[a-zA-Z]([0-9]|[a-zA-Z]|_)* {adjust(); yylval.sval=String(yytext); return ID;}
<INIT>[0-9]+ {adjust(); yylval.ival=atoi(yytext); return INT;}

<INIT>. {adjust(); EM_error(EM_tokPos, "encountered illegal token");}

<STR>"\\n" {charPos += yyleng; buf[pos] = '\n'; pos++;}
<STR>"\\t" {charPos += yyleng; buf[pos] = '\t'; pos++;}
<STR>\\\" {charPos += yyleng; buf[pos] = '"'; pos++;}
<STR>\\\\ {charPos += yyleng; buf[pos]='\\'; pos++;}
<STR>\\[0-9][0-9][0-9] {charPos += yyleng; buf[pos]=atoi(yytext); pos++;}
<STR>\\[\n\t\r\f]+\\ {adjust();}
<STR>\" {charPos += yyleng; BEGIN INIT; buf[pos] = '\0'; yylval.sval = String(buf); return STRING;}
<STR>. {charPos += yyleng; strcpy(buf + pos, yytext); pos += yyleng;}

<COMMENT>"/*" {adjust(); num++;}
<COMMENT>"*/" {adjust(); if(num>0){num--;}else{BEGIN INIT;}}
<COMMENT>"\n" {adjust(); EM_newline();}
<COMMENT>. {adjust();}


. {BEGIN INIT; yyless(0);}

<<EOF>> {return 0;}
