#include "2cc.h"

static Vector *node_vec;
static Vector *top_levels;

static Node *make_ident_node(Token *ident_token);
static Node *make_assign_node(Token *ident_token);
static Node *make_literal_node();
static Node *make_mul_sub_node();
static Node *make_add_sub_node();
static Node *make_binary_node(int kind, Node *left, Node *right);
static Node *make_if_node();
static Node *make_cond_node();
static Node *make_newline_node();
static Node *make_eof_node();
static Node *read_stmt();
static Node *make_component_node();

void parse_init() {
    top_levels = vec_init();
    node_vec = vec_init();
}

void parse_toplevel() {
    Token *token;
    while(1) {
        token = lex();
        switch(get_token_kind(token)) {
        case T_IDENT:
            vec_push(node_vec, make_ident_node(token));
            continue;
        case T_IF:
            vec_push(node_vec, make_if_node());
            continue;
        case T_NEWLINE:
            vec_push(node_vec, make_newline_node());
            continue;
        case T_EOF:
            vec_push(node_vec, make_eof_node());
            return;
        default:
            printf("parrser error\n");
            exit(1);
        }
    }
}

static Node *make_ident_node(Token *ident_token) {
    Node *node;
    if(next_token(T_ASSIGN)) {
        return make_assign_node(ident_token);
    }
    return NULL;
}

static Node *make_binary_node(int kind, Node *left, Node *right) {
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = kind;
    node->left = left;
    node->right = right;
    return node;
}

static Node *make_literal_node() {
    Token *token = lex();

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_LITERAL;
    node->val = get_token_val(token);
    return node;
}

static Node *make_mul_sub_node() {
    Node *node = make_literal_node();

    if(next_token(T_MUL))
        return make_binary_node(AST_MUL, node, make_literal_node());
    if(next_token(T_DIV))
        return make_binary_node(AST_DIV, node, make_literal_node());

    return node;
}

static Node *make_add_sub_node() {
    Node *node = make_mul_sub_node();

    if(next_token(T_ADD))
        return make_binary_node(AST_ADD, node, make_mul_sub_node());
    if(next_token(T_SUB))
        return make_binary_node(AST_SUB, node, make_mul_sub_node());

    return node;
}

static Node *make_assign_node(Token *ident_token) {
    Node *value = make_add_sub_node();

    Node *self = (Node *)malloc(sizeof(Node));
    self->kind = AST_GLOBAL_DECL;
    self->varname = get_token_num(ident_token);
    self->value = value;

    return self;
}

static Node *make_cond_node() { return make_add_sub_node(); }

static Node *make_if_node() {
    ensure_token(T_LPAREN);
    Node *cond = make_cond_node();
    ensure_token(T_RPAREN);

    Node *then;
    if(expect_token(T_LBRACE)) {
        then = make_component_node();
    } else {
        then = read_stmt();
    }

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_IF;
    node->cond = cond;
    node->then = then;
    return node;
}

static Node *read_stmt() {
    Token *token = lex();
    switch(get_token_kind(token)) {
    case T_LBRACE:
        return make_component_node();
    case T_IF:
        return make_if_node();
    case T_NEWLINE:
        return make_newline_node();
    case T_EOF:
        return make_eof_node();
    default:
        return make_ident_node(token);
    }
}

static Node *make_component_node() {
    Vector *component = vec_init();

    while(!expect_token(T_RBRACE)) {
        vec_push(component, read_stmt());
    }

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_COMPONENT;
    node->stmt = component;
    return node;
}

static Node *make_newline_node() {
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_NEWLINE;
    return node;
}

static Node *make_eof_node() {
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_EOF;
    return node;
}

Vector *get_top_levels() { return node_vec; }