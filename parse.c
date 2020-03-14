#include "2cc.h"

static Vector *node_vec;
static Vector *top_levels;

static Map *global_var;

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
static Node *read_decl();
static Node *read_decl_or_stmt();
static Node *make_component_node();

static Type *make_decl_type(Token *token);
static Type *make_primitive_type(int kind, int size);

static Node *make_decl_and_init_node(Type *type, char *ident, Node *value);
static Node *make_decl_node(Type *type, char *ident);

void parse_init() {
    top_levels = vec_init();
    node_vec = vec_init();
    global_var = map_init();
}

void parse_toplevel() {
    Token *token;
    while(1) {
        token = peek_token();
        if(get_token_kind(token) == T_EOF)
            return;

        vec_push(node_vec, read_decl_or_stmt());
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
    char *val = get_token_val(token);
    node->int_val = atoi(val);
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

static Node *make_relational_node() {
    Node *node = make_add_sub_node();

    if(next_token(T_LESS_EQ))
        return make_binary_node(AST_LESS_EQ, node, make_add_sub_node());
    else if(next_token(T_LESS))
        return make_binary_node(AST_LESS, node, make_add_sub_node());
    else if(next_token(T_GRE_EQ))
        return make_binary_node(AST_GRE_EQ, node, make_add_sub_node());
    else if(next_token(T_GRE))
        return make_binary_node(AST_GRE, node, make_add_sub_node());

    return node;
}

static Node *make_equ_neq_node() {
    Node *node = make_relational_node();

    if(next_token(T_EQUAL))
        return make_binary_node(AST_EQUAL, node, make_relational_node());

    return node;
}

static Node *make_logand_node() {
    Node *node = make_equ_neq_node();

    if(next_token(T_BIN_AND))
        return make_binary_node(AST_LOG_AND, node, make_equ_neq_node());

    return node;
}

static Node *make_logor_node() {
    Node *node = make_logand_node();

    if(next_token(T_BIN_OR))
        return make_binary_node(AST_LOG_OR, node, make_logand_node());

    return node;
}

static Node *make_assign_node(Token *ident_token) {
    Node *value = make_logor_node();

    Node *self = (Node *)malloc(sizeof(Node));
    self->kind = AST_GLOBAL_DECL;
    self->varname = get_token_val(ident_token);
    self->value = value;

    return self;
}

static Node *make_cond_node() { return make_logor_node(); }

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
    case T_EOF:
        return make_eof_node();
    default:
        return make_ident_node(token);
    }
}

static Node *read_decl() {
    Token *type = lex();
    Type *basetype = make_decl_type(type);
    Token *ident_token = lex();
    char *ident = get_token_val(ident_token);
    if(get_token_kind(ident_token) != T_IDENT) {
        printf("unvalid token : should be ident token\n");
        exit(1);
    }
    Node *node;
    if(next_token(T_ASSIGN)) {
        Node *value = make_logor_node();
        node = make_decl_and_init_node(basetype, ident, value);
    } else {
        node = make_decl_and_init_node(basetype, ident, NULL);
    }
    ensure_token(T_SEMICOLON);
    map_set(global_var, ident, node);
    return node;
}

static Node *read_decl_or_stmt() {
    Token *tok = peek_token();
    if(is_type(tok)) {
        return read_decl();
    } else
        return read_stmt();
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

static Type *make_decl_type(Token *token) {
    switch(get_token_kind(token)) {
    case T_INT:
        return make_primitive_type(TYPE_INT, 4);
    case T_SHORT:
        return make_primitive_type(TYPE_SHORT, 2);
    case T_DOUBLE:
        return make_primitive_type(TYPE_DOUBLE, 4);
    case T_FLOAT:
        return make_primitive_type(TYPE_FLOAT, 4);
    case T_LONG:
        return make_primitive_type(TYPE_LONG, 4);
    case T_CHAR:
        return make_primitive_type(TYPE_CHAR, 4);
    default:
        printf("unvalid type token\n");
        exit(1);
    }
}

static Type *make_primitive_type(int kind, int size) {
    Type *type = (Type *)malloc(sizeof(Type));
    type->kind = kind;
    type->size = size;
    return type;
}

static Node *make_decl_and_init_node(Type *type, char *ident, Node *value) {
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_GLOBAL_DECL;
    node->type = type;
    node->ident = ident;
    node->val = value;

    return node;
}

static Node *make_decl_node(Type *type, char *ident) {
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_GLOBAL_DECL;
    node->type = type;
    node->ident = ident;
    node->val = NULL;

    return node;
}