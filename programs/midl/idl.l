%{
#include "idl.tab.h"

#define YY_USER_INIT    yylloc_BOF( yylloc );
#define YY_USER_ACTION  yylloc_update( yylloc, yytext );

char *lineptr = NULL;
size_t n = 0;
size_t consumed = 0;
size_t available = 0;

size_t domin(size_t a, size_t b);

#if 0
#define YY_INPUT(buf,result,max_size) {\
    if(available <= 0) {\
        consumed = 0;\
        available = getline(&lineptr, &n, yyin);\
        if (available < 0) {\
            if (ferror(yyin)) { perror("read error:"); }\
                available = 0;\
            }\
    }\
    result = domin(available, max_size);\
    strncpy(buf, lineptr + consumed, result);\
    consumed += result;\
    available -= result;\
}
#endif

%}

%option bison-bridge
%option bison-locations
%option noyywrap
%option noinput
%option nounput

%%

[\t ]+   { }
[0-9]+   { *yylval = atoi(yytext);  return NUM; }
\n       { yylloc_EOL(yylloc); return '\n'; }
[^a]     { return yytext[0]; }
"a"+     { return YYUNDEF; }

%%

size_t domin(size_t a, size_t b) {
    return b < a ? b : a;
}
