#include "2cc.h"

static Vector *node_vec;
static Vector *top_levels;

static void make_ident_node(Token *ident_token);
static void make_eof_node();
static Node *make_assign_node(Token *ident_token);
static Node *make_literal_node();
static Node *make_mul_sub_node();
static Node *make_add_sub_node();
static Node *make_binary_node(int kind, Node *left, Node *right);

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
            make_ident_node(token);
            continue;
        case T_NEWLINE:
            continue;
        case T_EOF:
            make_eof_node();
            return;
        default:
            printf("parrser error\n");
            exit(1);
        }
    }
}

static void make_ident_node(Token *ident_token) {
    Node *node;
    if(next_token(T_ASSIGN)) {
        node = make_assign_node(ident_token);
        vec_push(node_vec, node);
    }
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

static void make_eof_node() {
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_END;
    node->varname = NULL;
    node->val = NULL;
    vec_push(node_vec, node);
}

Vector *get_top_levels() { return node_vec; }