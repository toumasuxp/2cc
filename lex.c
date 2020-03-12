#include "2cc.h"

struct _Token {
    int kind;
    char *val;
};

static Vector *token_buf = NULL;

static Token *read_number(int c);
static Token *read_ident(int c);
static Token *make_num_token(char *num);
static Token *make_newline_token();
static Token *make_eof_token();
static Token *read_plus();
static Token *make_ident_token(char *buf);

static Token *read_equal(int c);
static Token *make_equal_token();

int get_token_kind(Token *token) { return token->kind; }

char *get_token_val(Token *token) { return token->val; }

void lex_init() {
    token_buf = vec_init();
    Vector *vec = vec_init();
    vec_push(token_buf, vec);
}

Token *lex() {
    Vector *vec = vec_tail(token_buf);
    if(vec_len(vec) > 0)
        return vec_pop(vec);
    skip_space();

    int c = readc();

    switch(c) {
    case '0' ... '9':
        return read_number(c);
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
        return read_ident(c);
    case '=':
        return read_equal(c);
    case '+':
        return read_plus();
    case '\n':
        return make_newline_token();
    case EOF:
        return make_eof_token();
    }
}

bool next_token(int kind) {
    Token *token = lex();
    if(get_token_kind(token) == kind)
        return true;

    unread_token(token);
    return false;
}

Token *peek_token() {
    Token *token = lex();
    unread_token(token);
    return token;
}

bool expect_token(int kind) {
    Token *token = lex();
    unread_token(token);
    return get_token_kind(token) == kind;
}

void unread_token(Token *token) {
    if(get_token_kind(token) == T_EOF)
        return;
    Vector *vec = vec_tail(token_buf);
    vec_push(vec, token);
}

// テスト用
char *get_token_num(Token *token) { return token->val; }

static Token *read_number(int c) {
    Buffer *buf = make_buffer();
    buf_write(buf, c);

    for(;;) {
        c = readc();

        if(!isdigit(c) && c != '.') {
            unreadc(c);
            buf_write(buf, '\0');
            return make_num_token(buf_get_body(buf));
        }
        buf_write(buf, c);
    }
}

static Token *read_ident(int c) {
    Buffer *buf = make_buffer();
    buf_write(buf, c);

    for(;;) {
        c = readc();
        if(!isalnum(c) && c != '_') {
            unreadc(c);
            buf_write(buf, '\0');
            return make_ident_token(buf_get_body(buf));
        }
        buf_write(buf, c);
    }
}

static Token *read_equal(int c) { return make_equal_token(c); }

static Token *make_equal_token() {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_ASSIGN;
    tok->val = NULL;
    return tok;
}

static Token *make_num_token(char *num) {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_NUM;
    tok->val = num;
    return tok;
}

static Token *make_ident_token(char *buf) {
    printf("T_IDENT token\n");
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_IDENT;
    tok->val = buf;
    return tok;
}

static Token *make_newline_token() {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_NEWLINE;
    return tok;
}

static Token *read_plus() {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_ADD;
    return tok;
}

static Token *make_eof_token() {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_EOF;
    return tok;
}