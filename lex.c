#include "2cc.h"

struct _Token {
    int kind;
    char *val;
    bool is_type;
};

struct KeyWord {
    int kind;
    char *ident;
    bool is_type;
};

static struct KeyWord keywords[] = {
    {T_IF, "if", false},         {T_WHILE, "while", false},
    {T_BREAK, "break", false},   {T_CONTINUE, "continue", false},
    {T_RETURN, "return", false},

    {T_INT, "int", true},        {T_CHAR, "char", true},
    {T_FLOAT, "float", true},    {T_DOUBLE, "double", true},
    {T_LONG, "long", true},      {T_SHORT, "short", true},

    {T_DUMMY, "dummy", false}};

static Vector *token_buf = NULL;

static Token *read_number(int c);
static Token *read_ident(int c);
static Token *make_literal_token(char *num);
static Token *make_newline_token();
static Token *make_eof_token();
static Token *read_plus();
static Token *read_sub();
static Token *read_mult();
static Token *read_div();
static Token *make_ident_token(char *buf);

static Token *read_equal();
static Token *make_equal_token();
static Token *make_assign_token();
static Token *read_lbracket();
static Token *read_rbracket();
static Token *read_and();
static Token *read_or();
static Token *make_kind_token(int kind);
static Token *make_op_token(char c, int kind);
static Token *make_keyword_token(char *buf);

static bool is_keyword(char *buf);

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

    int c;
    while(1) {
        skip_space();
        c = readc();
        switch(c) {
        case '0' ... '9':
            return read_number(c);
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '_':
            return read_ident(c);
        case '=':
            return read_equal();
        case '+':
            return read_plus();
        case '*':
            return read_mult();
        case '/':
            return read_div();
        case '<':
            return read_lbracket();
        case '>':
            return read_rbracket();
        case '&':
            return read_and();
        case '|':
            return read_or();
#define op(c, t)                                                               \
    case c:                                                                    \
        return make_op_token(c, t);
#include "opcode.inc"
#undef op
        case '\n':
        case '\r':
            continue;
        case EOF:
            return make_eof_token();
        default:
            printf("error log\n");
            exit(1);
        }
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

void ensure_token(int kind) {
    Token *token = lex();
    if(get_token_kind(token) == kind) {
        return;
    } else {
        printf("ERROR: invalid token %d\n", get_token_kind(token));
        exit(1);
    }
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
            return make_literal_token(buf_get_body(buf));
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
            char *b = buf_get_body(buf);

            if(is_keyword(b))
                return make_keyword_token(b);

            return make_ident_token(b);
        }
        buf_write(buf, c);
    }
}

static Token *read_equal() {
    if(expectc('='))
        return make_equal_token();

    return make_assign_token();
}

static Token *make_equal_token() {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_EQUAL;
    tok->val = NULL;
    return tok;
}

static Token *make_assign_token() {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_ASSIGN;
    tok->val = NULL;
    return tok;
}

static Token *make_literal_token(char *num) {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_LITERAL;
    tok->val = num;
    tok->is_type = false;
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

static Token *read_sub() {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_SUB;
    return tok;
}

static Token *read_mult() {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_MUL;
    return tok;
}

static Token *read_div() {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_DIV;
    return tok;
}

static Token *read_lbracket() {
    if(expectc('='))
        return make_kind_token(T_LESS_EQ);

    return make_kind_token(T_LESS);
}

static Token *read_rbracket() {
    if(expectc('='))
        return make_kind_token(T_GRE_EQ);

    return make_kind_token(T_GRE);
}

static Token *read_and() {
    if(expectc('&'))
        return make_kind_token(T_BIN_AND);

    return make_kind_token(T_AND);
}

static Token *read_or() {
    if(expectc('|'))
        return make_kind_token(T_BIN_OR);
}

static Token *make_kind_token(int kind) {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = kind;
    return tok;
}

static Token *make_eof_token() {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = T_EOF;
    return tok;
}

static Token *make_op_token(char c, int kind) {
    Token *tok = (Token *)malloc(sizeof(Token));
    tok->kind = kind;
    return tok;
}

static bool is_keyword(char *buf) {
    int i;
    for(i = 0; keywords[i].kind != T_DUMMY; i++) {
        if(!strcmp(buf, keywords[i].ident)) {
            return true;
        }
    }
    return false;
}

bool is_type(Token *token) {
    int i;
    int kind = get_token_kind(token);
    for(i = 0; keywords[i].kind != T_DUMMY; i++) {
        if(kind == keywords[i].kind && keywords[i].is_type) {
            return true;
        }
    }
    return false;
}

bool is_token_kind(Token *token, int kind) {
    return get_token_kind(token) == kind;
}

static Token *make_keyword_token(char *buf) {
    int i;
    for(i = 0; keywords[i].kind != T_DUMMY; i++) {
        if(!strcmp(buf, keywords[i].ident)) {
            Token *token = (Token *)malloc(sizeof(Token));
            token->kind = keywords[i].kind;
            token->is_type = keywords[i].is_type;
            return token;
        }
    }
    return NULL;
}