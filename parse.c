#include "2cc.h"

static Vector *node_vec;
static Vector *top_levels;

/*
    本当はglobal_varsもVectorにしたいけど、
    変数と関数を両方格納するためにMapを採用した
*/
static Map *global_vars;
static Vector *local_vars;

static char *lcontinue = NULL;
static char *lbreak = NULL;

static bool is_global = true;

#define SET_JUMP_LABELS(cont, brk)                                             \
    char *ocontinue = lcontinue;                                               \
    char *obreak = lbreak;                                                     \
    lcontinue = cont;                                                          \
    lbreak = brk

#define RESTORE_JUMP_LABELS()                                                  \
    lcontinue = ocontinue;                                                     \
    lbreak = obreak

static Node *read_func();
static bool is_func();
static void skip_parentheses(Vector *vector);
static Param *make_decl_param();
static void make_func_params(Vector *params);
static Node *make_func_body(Type *functype, char *name, Vector *params);

static Type *make_func_type(char **name, Type *basetype, Vector *params);

static Node *make_ident_node();
static Node *make_assign_node();
static Node *make_literal_node();
static Node *make_mul_sub_node();
static Node *make_add_sub_node();
static Node *make_binary_node(int kind, Node *left, Node *right);
static Node *make_actual_assign_node(Node *node, int ast_kind);
static void *ensure_assign_node(Node *node);
static Node *make_if_node();
static Node *make_cond_node();
static Node *make_while_node();
static Node *make_continue_node();
static Node *make_break_node();
static Node *make_return_node();
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

static Node *make_func_call_node(Token *ident);
static Node *search_var(char *name);

void parse_init() {
    top_levels = vec_init();
    node_vec = vec_init();
    global_vars = map_init();
}

void parse_toplevel() {
    Token *token;
    while(1) {
        token = peek_token();
        if(get_token_kind(token) == T_EOF) {
            printf("call t eof\n");
            return;
        }

        if(is_func()) {
            printf("set parse func\n");
            vec_push(node_vec, read_func());
        } else {
            vec_push(node_vec, read_decl_or_stmt());
        }
    }
}

static Node *read_func() {
    Token *token = lex();
    Type *basetype = make_decl_type(token);
    char *name;
    Vector *params = vec_init();
    Type *functype = make_func_type(&name, basetype, params);

    ensure_token(T_LBRACE);
    Node *body = make_func_body(functype, name, params);

    map_set(global_vars, name, body);
    return body;
}

static Node *make_func_body(Type *functype, char *name, Vector *params) {
    local_vars = vec_init();
    is_global = false;
    Node *body = make_component_node();

    Node *func = (Node *)malloc(sizeof(Node));
    func->kind = AST_FUNCDEF;
    func->func_type = functype;
    func->func_name = name;
    func->params = params;
    func->body = body;
    func->local_vars = local_vars;

    is_global = true;
    local_vars = NULL;
    return func;
}

static Type *make_func_type(char **name, Type *basetype, Vector *params) {
    Token *token = lex();

    if(!is_token_kind(token, T_IDENT)) {
        printf("unvalid function decl\n");
        exit(1);
    }
    *name = get_token_val(token);

    ensure_token(T_LPAREN);

    make_func_params(params);

    Type *functype = (Type *)malloc(sizeof(Type));
    functype->kind = basetype->kind;
    functype->params = params;
    functype->size = basetype->size;
    return functype;
}

static void make_func_params(Vector *params) {
    Token *token;

    for(;;) {
        if(next_token(T_RPAREN))
            break;

        token = peek_token();
        if(is_type(token)) {
            vec_push(params, make_decl_param());

            if(next_token(T_COMMA))
                continue;
        }
        ensure_token(T_RPAREN);
        break;
    }
}

static Param *make_decl_param() {
    Token *type_token = lex();
    Type *type = make_decl_type(type_token);

    Token *ident = lex();
    if(is_token_kind(ident, T_IDENT)) {
        Param *param = (Param *)malloc(sizeof(Param));
        param->type = type;
        param->name = get_token_val(ident);
        return param;
    } else {
        printf("ERROR!!\n");
        exit(1);
    }
}

static bool is_func() {
    Vector *vector = vec_init();
    bool result = false;
    Token *token;
    for(;;) {
        token = lex();
        vec_push(vector, token);

        if(is_token_kind(token, T_SEMICOLON))
            break;

        if(is_type(token) || is_token_kind(token, T_IDENT))
            continue;

        if(is_token_kind(token, T_LPAREN)) {
            skip_parentheses(vector);
            continue;
        }

        result = is_token_kind(token, T_LBRACE);
        break;
    }

    while(vec_len(vector) > 0)
        unread_token(vec_pop(vector));

    return result;
}

static void skip_parentheses(Vector *vector) {
    Token *token;
    for(;;) {
        token = lex();
        if(is_token_kind(token, T_EOF)) {
            printf("premature end of input");
            exit(1);
        }

        vec_push(vector, token);

        if(is_token_kind(token, T_RPAREN))
            break;
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
    char *val = get_token_val(token);
    node->int_val = atoi(val);
    return node;
}

static Node *make_ident_node() {
    Token *token = lex();

    if(next_token(T_LPAREN))
        return make_func_call_node(token);

    Node *node = (Node *)malloc(sizeof(Node));
    char *name = get_token_val(token);

    if(is_global) {
        Node *var = search_var(name);
        if(var == NULL) {
            printf("undefined variable: %s\n", name);
            exit(1);
        }

        node->kind = AST_GVAR;
        node->type = var->type;
        node->varname = var->ident;
    } else {
        Node *var = search_var(name);
        if(var == NULL) {
            printf("undefined variable: %s\n", name);
            exit(1);
        }

        node->kind = AST_LVAR;
        node->type = var->type;
        node->varname = var->ident;
        node->loff = var->loff;
    }

    return node;
}

static Node *make_func_call_node(Token *ident) {
    Vector *args = vec_init();

    char *id = get_token_val(ident);
    Node *func = map_get(global_vars, id);
    if(func == NULL || func->kind != AST_FUNCDEF) {
        printf("undefined function: %s\n", get_token_val(ident));
        exit(1);
    }

    // とりあえず今回は引数なしのみ関数呼び出しをできるようにする
    ensure_token(T_RPAREN);
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_FUNC_CALL;
    node->call_func_name = func->func_name;
    node->call_func_type = func->func_type; // ここはいらないかも
    return node;
}

static Node *search_var(char *name) {
    Node *var = map_get(global_vars, name);
    if(var)
        return var;

    for(int i = 0; i < vec_len(local_vars); i++) {
        var = vec_get(local_vars, i);
        if(!strcmp(name, var->varname)) {
            return var;
        }
    }

    return NULL;
}

static Node *make_primary_node() {
    if(expect_token(T_IDENT))
        return make_ident_node();
    else if(expect_token(T_LITERAL))
        return make_literal_node();

    // 最後にreturnをしているのは特に意味はない
    return make_literal_node();
}

static Node *make_mul_sub_node() {
    Node *node = make_primary_node();

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

static Node *make_assign_node() {
    Node *node = make_logor_node();

    if(next_token(T_ASSIGN))
        return make_actual_assign_node(node, AST_SIMPLE_ASSIGN);

    if(next_token(T_ADD_ASSIGN))
        return make_actual_assign_node(node, AST_ADD_ASSIGN);

    if(next_token(T_SUB_ASSIGN))
        return make_actual_assign_node(node, AST_SUB_ASSIGN);

    if(next_token(T_MUL_ASSIGN))
        return make_actual_assign_node(node, AST_MUL_ASSIGN);

    if(next_token(T_DIV_ASSIGN))
        return make_actual_assign_node(node, AST_DIV_ASSIGN);

    return node;
}

static Node *make_actual_assign_node(Node *node, int ast_kind) {
    ensure_assign_node(node);
    return make_binary_node(ast_kind, node, make_logor_node());
}

static void *ensure_assign_node(Node *node) {
    if(node->kind != AST_GVAR && node->kind != AST_LVAR) {
        printf("unvalid token error: should AST_GVAR or AST_LVAR\n");
        exit(1);
    }
}

static Node *make_cond_node() { return make_assign_node(); }

static Node *make_if_node() {
    ensure_token(T_LPAREN);
    Node *cond = make_cond_node();
    ensure_token(T_RPAREN);

    Node *then;
    if(next_token(T_LBRACE)) {
        then = make_component_node();
    } else {
        then = read_decl_or_stmt();
    }

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_IF;
    node->cond = cond;
    node->then = then;
    return node;
}

static Node *make_while_node() {

    ensure_token(T_LPAREN);
    Node *cond = make_cond_node();
    ensure_token(T_RPAREN);
    char *begin = make_label();
    char *end = make_label();

    SET_JUMP_LABELS(begin, end);
    Node *then;
    printf("while then\n");

    if(next_token(T_LBRACE)) {
        then = make_component_node();
    } else {
        then = read_decl_or_stmt();
    }

    RESTORE_JUMP_LABELS();
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_WHILE;
    node->cond = cond;
    node->then = then;
    node->lbegin = begin;
    node->lend = end;
    return node;
}

static Node *read_stmt() {
    Token *token = lex();
    switch(get_token_kind(token)) {
    case T_LBRACE:
        return make_component_node();
    case T_IF:
        return make_if_node();
    case T_WHILE:
        return make_while_node();
    case T_BREAK:
        return make_break_node();
    case T_CONTINUE:
        return make_continue_node();
    case T_RETURN:
        return make_return_node();
    case T_EOF:
        return make_eof_node();
    default:
        unread_token(token);
        return make_assign_node();
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

    if(is_global)
        map_set(global_vars, ident, node);
    else
        map_set(local_vars, ident, node);

    return node;
}

static Node *read_decl_or_stmt() {
    Token *tok = peek_token();
    if(is_type(tok)) {
        return read_decl();
    } else
        return read_stmt();
}

static Node *make_return_node() {
    if(next_token(T_SEMICOLON)) {
        Node *node = (Node *)malloc(sizeof(Node));
        node->kind = AST_RETURN;
        return node;
    }

    Node *retval = make_assign_node();
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_RETURN;
    node->retval = retval;

    ensure_token(T_SEMICOLON);
    return node;
}

static Node *make_break_node() {
    if(lbreak == NULL) {
        printf("ERROR: unvalid break parser\n");
        exit(1);
    }
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_BREAK;
    node->jmp_label = lbreak;
    return node;
}

static Node *make_continue_node() {
    if(lbreak == NULL) {
        printf("ERROR: unvalid continue parser\n");
        exit(1);
    }

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_CONTINUE;
    node->jmp_label = lcontinue;
    return node;
}

static Node *make_component_node() {
    Vector *component = vec_init();

    Node *self;
    while(!expect_token(T_RBRACE)) {
        self = read_decl_or_stmt();
        printf("node kind is %d\n", self->kind);
        vec_push(component, self);
    }

    ensure_token(T_RBRACE);

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
    node->kind = is_global ? AST_GLOBAL_DECL : AST_LOCAL_DECL;
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