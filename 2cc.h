#pragma once

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// types

typedef struct _Token Token;

typedef struct _File File;

enum {
    T_NUM,
    T_ADD,
    T_SUB,
    T_MUL,
    T_DIV,
    T_NEWLINE,
    T_IF,
    T_IDENT,
    T_ASSIGN,
    T_LPAREN,
    T_RPAREN,
    T_LBRACE,
    T_RBRACE,
    T_SEMICOLON,
    T_EOF
};

enum {
    AST_GLOBAL_DECL,
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_LITERAL,
    AST_COMPONENT,
    AST_IF,
    AST_NEWLINE,
    AST_EOF,
    AST_END,
};

typedef struct _Buffer Buffer;

typedef struct _Vector Vector;

typedef struct _Node Node;

struct _Node {
    int kind;
    union {

        // assign expression
        struct {
            char *varname;
            struct _Node *value;
        };

        // literal
        char *val;

        // binary calc ex) + - %
        struct {
            struct _Node *left;
            struct _Node *right;
        };

        // if statement
        struct {
            struct _Node *cond;
            struct _Node *then;
        };

        // statement
        Vector *stmt;
    };
};

// main.c

// file.c

void file_init(char *filename);
int readc();
void unreadc(int c);
void skip_space();

// lex.c

void lex_init();
Token *lex();
int get_token_kind(Token *token);
char *get_token_num(Token *token);
bool next_token(int kind);
void unread_token(Token *token);
Token *peek_token();
bool expect_token(int kind);
void ensure_token(int kind);
char *get_token_val(Token *token);

// parse.c

void parse_toplevel();
void parse_init();
Vector *get_top_levels();

// vector.c

Vector *vec_init();
void vec_push(Vector *vec, void *elem);
void *vec_pop(Vector *vec);
void *vec_head(Vector *vec);
void *vec_tail(Vector *vec);
void *vec_body(Vector *vec);
int vec_len(Vector *vec);
void *vec_get(Vector *vec, int index);

// buffer.c

Buffer *make_buffer();
void buf_write(Buffer *buf, int c);
char *buf_get_body(Buffer *buf);
int buf_get_size(Buffer *buf);
int buf_get_len(Buffer *buf);
char *format(char *fmt, ...);
char *vformat(char *fmt, va_list ap);

// gen.c

void gen_init();
void gen_toplevel(Vector *toplevel);