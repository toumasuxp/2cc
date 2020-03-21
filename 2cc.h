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
    T_LITERAL,
    T_ADD,
    T_SUB,
    T_MUL,
    T_DIV,
    T_NEWLINE,
    T_IF,
    T_WHILE,
    T_BREAK,
    T_RETURN,
    T_CONTINUE,
    T_IDENT,
    T_ASSIGN,
    T_ADD_ASSIGN,
    T_SUB_ASSIGN,
    T_MUL_ASSIGN,
    T_DIV_ASSIGN,
    T_EQUAL,
    T_LESS_EQ,
    T_LESS,
    T_GRE_EQ,
    T_GRE,
    T_BIN_AND,
    T_BIN_OR,

    T_INT,
    T_CHAR,
    T_SHORT,
    T_LONG,
    T_FLOAT,
    T_DOUBLE,

    T_LPAREN,
    T_RPAREN,
    T_LBRACE,
    T_RBRACE,
    T_SEMICOLON,
    T_COMMA,
    T_EOF,
    T_DUMMY
};

enum { TYPE_INT, TYPE_SHORT, TYPE_LONG, TYPE_CHAR, TYPE_FLOAT, TYPE_DOUBLE };

enum {
    AST_GLOBAL_DECL,
    AST_LOCAL_DECL,
    AST_GVAR,
    AST_LVAR,
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_LOG_OR,
    AST_LOG_AND,
    AST_EQUAL,
    AST_LESS_EQ,
    AST_LESS,
    AST_GRE_EQ,
    AST_GRE,
    AST_LITERAL,
    AST_COMPONENT,
    AST_IF,
    AST_WHILE,
    AST_BREAK,
    AST_RETURN,
    AST_CONTINUE,
    AST_NEWLINE,

    AST_SIMPLE_ASSIGN,
    AST_ADD_ASSIGN,
    AST_SUB_ASSIGN,
    AST_MUL_ASSIGN,
    AST_DIV_ASSIGN,

    AST_FUNCDEF,
    AST_FUNC_CALL,

    AST_EOF,
    AST_END,
};

typedef struct _Buffer Buffer;

typedef struct _Vector Vector;

typedef struct _Node Node;

typedef struct _Type Type;

typedef struct _Map Map;

typedef struct _Param Param;

struct _Node {
    int kind;
    union {

        // ident node
        struct {
            char *varname;
            Type *ident_type;
        };

        // variable declare
        struct {
            Type *type;
            char *ident;
            struct _Node *val;
            int loff;
        };

        // literal
        int int_val;

        // binary calc ex) + - %
        struct {
            struct _Node *left;
            struct _Node *right;
        };

        // if or while statement
        struct {
            struct _Node *cond;
            struct _Node *then;
            char *lbegin;
            char *lend;
        };

        // function
        struct {
            Type *func_type;
            char *func_name;
            Vector *params;
            struct _Node *body;
            Vector *local_vars;
        };

        // func call
        struct {
            Type *call_func_type;
            char *call_func_name;
            Vector *args;
        };

        // return value
        struct _Node *retval;

        // break or continue
        char *jmp_label;

        // statement
        Vector *stmt;
    };
};

struct _Type {
    int kind;
    int size;
    Vector *params;

    char *ident_name; // 主に関数名や変数名を入れるために使う
};

struct _Param {
    Type *type;
    char *name;
    int loff;
};

// main.c

// file.c

void file_init(char *filename);
int readc();
bool expectc(int c);
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
bool is_type(Token *token);
bool is_token_kind(Token *token, int kind);

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
char *make_label();

// map.c

Map *map_init();
void *map_get(Map *map, char *key);
void map_set(Map *map, char *key, void *val);