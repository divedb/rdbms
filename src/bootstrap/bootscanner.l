%{

#include <iostream>
#include "rdbms/"

#define YY_NO_UNPUT

#if defined(yywrap)
#undef yywrap
#endif

static YYSTYPE yylval;
static int yyline; // Keep track of the line number for error reporting

%}

%option case-insensitive

digit       [0-9]
oct         \\{digit}{digit}{digit}
exp         [Ee][-+]?{digit}+
id          ([A-Za-z0-9_] | {oct} | \-)+
sid         \"(^\")*\"
array_id    [A-Za-z0-9_]+\[{digit}*\]

%%

OPEN        { return OPEN; }
CLOSE       { return XCLOSE; }
CREATE      { return XCREATE; }
OID         { return OBJ_ID; }
BOOTSTRAP   { return XBOOTSTRAP; }
_NULL_      { return NULLVAL; }
INSERT      { return INSERT_TUPLE; }
DECLARE     { return XDECLARE; }
BUILD       { return XBUILD; }
INDICES     { return INDICES; }
INDEX       { return INDEX; }
ON          { return ON; }
USING       { return USING; }

","         { return COMMA; }
"="         { return EQUALS; }
")"         { return LPAREN; }
"("         { return RPAREN; }

(-)?{digit}+"."{digit}*({exp})? |
(-)?{digit}*"."{digit}+({exp})? |
(-)?{digit}+{exp} {}

{array_id} { }
{id} {}
{sid} {}

[\n] { yyline++; }
[\t] ;
" "  ;
^\#[^\n]* ;

. {}

%%

int yywrap() { return 1; }

void yyerror(const char* str) {
    std::cerr << "\tsyntax error " << yyline << " : " << str;
}
