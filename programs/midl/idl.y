%code provides
{

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int yylex(YYSTYPE *yylval, YYLTYPE *yylloc);
void yyerror(YYLTYPE *yylloc, const char *error);

void yylloc_BOF(YYLTYPE *yylloc);
void yylloc_EOL(YYLTYPE *yylloc);
void yylloc_update(YYLTYPE *yylloc, const char *yytext);

extern char *lineptr;

#define YYERROR_VERBOSE 1
}

%define api.pure full
%locations

%token NUM
%left '-' '+'
%left '(' ')'

%%
LINE:                   { $$ = 0; }
       | LINE EXPR '\n' { printf("%s=%d\n", lineptr, $2); }
       | LINE '\n'
       ;


EXPR:    NUM            { $$ = $1; }
     |   EXPR '-' EXPR  { $$ = $1 - $3; }
     |   EXPR '+' EXPR  { $$ = $1 + $3; }
     |   '(' EXPR ')'   { $$ = $2; }
     |   error ';'      { yyerrok; }
     ;


%%

static size_t yylloc_size;
static char *yylloc_line;

void yylloc_BOF(YYLTYPE *yylloc)
{
    yylloc->first_line = 1;
    yylloc->last_line = 1;
    yylloc->first_column = 1;
    yylloc->last_column = 1;
}

void yylloc_EOL(YYLTYPE *yylloc)
{
    yylloc->first_line++;
    yylloc->last_line++;
    yylloc->first_column = 1;
    yylloc->last_column = 1;
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

void yylloc_update(YYLTYPE *yylloc, const char *yytext)
{
    int len = strlen(yytext);

    yylloc->first_column = yylloc->last_column;
    yylloc->last_column += len;

    if (yylloc->last_column >= yylloc_size)
    {
        yylloc_size = max(yylloc_size * 3 / 2, yylloc->last_column);
        yylloc_line = realloc(yylloc_line, yylloc_size);
    }

    strcpy(yylloc_line + yylloc->first_column - 1, yytext);
}

void yyerror(YYLTYPE *yylloc, const char *error)
{
    int i;

    fprintf(stderr, "error: %s in line %d, column %d\n", error, yylloc->first_line, yylloc->first_column);
    fprintf(stderr, "%s\n", yylloc_line);

    for (i = 0; i < yylloc->first_column - 1; i++) fprintf(stderr," ");
    fprintf(stderr,"^");
    for (i = yylloc->first_column + 1; i < yylloc->last_column; i++) fprintf(stderr,"~");
    fprintf(stderr,"\n");
}
