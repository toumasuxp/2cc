#include "2cc.h"

static Vector *node_vec;
static Vector *top_levels;

static int loff = 0;

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
static Node *read_funcbody(Type *functype, char *name, Vector *params);

static Type *make_func_type(Type *rettype, Vector *params);

static Node *make_ident_node();
static Node *make_assign_node();
static Node *make_literal_node();
static Node *make_mul_sub_node();
static Node *make_add_sub_node();
static Node *ast_binop(Type *type, int kind, Node *left, Node *right);
static Node *binop(int kind, Node *left, Node *right);
static Node *make_actual_assign_node(Node *node, int ast_kind);
static void *ensure_assign_node(Node *node);
static Node *ast_if();
static Node *make_cond_node();
static Node *ast_while();
static Node *ast_continue();
static Node *ast_break();
static Node *make_return_node();
static Node *ast_newline();
static Node *ast_eof();
static Node *read_stmt();
static Node *read_decl();
static Node *read_decl_or_stmt();
static Node *ast_component();

static Node *ast_gvar(char *varname, Type *declator);
static Node *ast_lvar(char *varname, Type *declator);

static Type *make_decl_type();
static Type *make_primitive_type(int kind, int size);

static Node *make_decl_and_init_node(Type *type, Node *var, Node *value);

static Node *read_funcall(Token *ident);
static Node *search_var(char *name);

static Type *read_declator(char **name, Type *basetype, Vector *params);
static Type *make_ptr_type(Type *basetype);

static Node *ast_func(Type *functype, char *name, Vector *params, Node *body,
                      Vector *local_vars);

static Node *make_unary_node();
static Node *make_unary_addr_node();
static Node *make_unary_deref_node();

static Type *make_array_type(Type *base, int len);

static Type *read_declator_tail_array(char **name, Type *basetype,
                                      Vector *params);
static Type *read_declator_tail(char **name, Type *basetype, Vector *params);

static Node *ast_decl(Type *type);
static Node *make_decl_array_init(Type *type);
static Node *make_array_elm_node(Node *var);

static Type *read_declator_tail_func(char **name, Type *rettype,
                                     Vector *params);
static void read_declarator_params(Vector *types, Vector *vars);
static Type *read_func_param(char **name, bool typeonly);

static Node *conv(Node *node);
static Node *ast_uop(int kind, Type *ty, Node *operand);
static Vector *read_func_args(Vector *params);

// 8ccにあった書き方。かっこいい
Type *type_int = &(Type){TYPE_INT, 4};
Type *type_char = &(Type){TYPE_CHAR, 1};

Node *ast_semicolon = &(Node){AST_SEMICOLON};

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
            printf("set parse read_decl\n");
            vec_push(node_vec, read_decl());
        }
    }
}

static Node *read_func() {
    Type *basetype = make_decl_type();
    char *name;
    Vector *params = vec_init();
    local_vars = vec_init();
    is_global = false;

    Type *functype = read_declator(&name, basetype, params);
    ast_gvar(name, functype);
    ensure_token(T_LBRACE);
    Node *body = read_funcbody(functype, name, params);

    is_global = true;
    local_vars = NULL;
    return body;
}

static Node *read_funcbody(Type *functype, char *name, Vector *params) {
    Node *body = ast_component();
    Node *func = ast_func(functype, name, params, body, local_vars);

    return func;
}

static Node *ast_func(Type *functype, char *name, Vector *params, Node *body,
                      Vector *local_vars) {
    Node *func = (Node *)malloc(sizeof(Node));
    func->kind = AST_FUNCDEF;
    func->func_type = functype;
    func->func_name = name;
    func->params = params;
    func->body = body;
    func->local_vars = local_vars;
    return func;
}

static Type *make_func_type(Type *rettype, Vector *params) {
    Type *functype = (Type *)malloc(sizeof(Type));
    functype->kind = TYPE_FUNC;
    functype->rettype = rettype;
    functype->params = params;
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
    Type *type = make_decl_type();

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
    printf("check is func\n");
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

static Node *binop(int kind, Node *left, Node *right) {
    printf("check left node is TYPE POINTER\n");
    if(left->type->kind == TYPE_POINTER) {
        return ast_binop(left->type, kind, left, right);
    }
    printf("check right node is TYPE POINTER\n");
    if(right->type->kind == TYPE_POINTER) {
        printf("end check right node is TYPE POINTER\n");
        return ast_binop(right->type, kind, right, left);
    }
    return ast_binop(left->type, kind, left, right);
}

static Node *ast_binop(Type *type, int kind, Node *left, Node *right) {
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = kind;
    node->type = type;
    node->left = left;
    node->right = right;
    return node;
}

static Node *make_string_node() {
    Token *token = lex();
    Type *type = make_array_type(type_char, get_token_len(token));
    char *body = get_token_val(token);

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_LITERAL;
    node->type = type;
    node->literal_val = body;
    return node;
}

static Node *make_literal_node() {
    Token *token = lex();

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_LITERAL;
    node->type = type_int;
    char *literal_val = get_token_val(token);
    node->int_val = atoi(literal_val);
    return node;
}

static Node *make_ident_node() {
    Token *token = lex();

    if(next_token(T_LPAREN))
        return read_funcall(token);

    char *name = get_token_val(token);

    Node *var = search_var(name);
    if(var == NULL) {
        printf("undefined variable: %s\n", name);
        exit(1);
    }

    if(next_token(T_LBRACKET))
        return make_array_elm_node(var);
    return var;
}

static Node *make_array_elm_node(Node *var) {
    printf("make_array_elm_node\n");
    Node *len = make_mul_sub_node();
    ensure_token(T_RBRACKET);
    Node *t = binop(AST_ADD, conv(var), conv(len));

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_DEREF;
    node->type = make_ptr_type(t->type->pointer_type);
    node->operand = t;
    return node;
}

static Node *read_funcall(Token *ident) {

    char *id = get_token_val(ident);
    Node *func = map_get(global_vars, id);
    if(func == NULL || func->type->kind != TYPE_FUNC) {
        printf("undefined function: %s\n", get_token_val(ident));
        exit(1);
    }

    Vector *args = read_func_args(func->params);

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_FUNC_CALL;
    node->call_func_name = id;
    node->call_func_type = func->type->rettype; // ここはいらないかも
    node->args = args;
    return node;
}

static Vector *read_func_args(Vector *params) {
    Vector *args = vec_init();
    int i = 0;
    for(;;) {
        if(next_token(T_RPAREN))
            break;

        Node *arg = conv(make_assign_node());
        vec_push(args, arg);

        if(next_token(T_RPAREN))
            break;
        ensure_token(T_COMMA);
    }
    return args;
}

static Node *search_var(char *name) {
    printf("searching... %s\n", name);
    Node *var = map_get(global_vars, name);
    if(var) {
        printf("search GVAR %s\n", name);
        return var;
    }

    printf("searching local var.... %s\n", name);
    for(int i = 0; i < vec_len(local_vars); i++) {
        var = vec_get(local_vars, i);
        if(!strcmp(name, var->ident)) {
            printf("search LVAR %s\n", name);
            return var;
        }
    }

    return NULL;
}

static Node *make_primary_node() {
    printf("primary node\n");
    if(expect_token(T_IDENT))
        return make_ident_node();
    else if(expect_token(T_LITERAL))
        return make_literal_node();
    else if(expect_token(T_STRING))
        return make_string_node();

    // 最後にreturnをしているのは特に意味はない
    return make_literal_node();
}

static Node *make_unary_node() {
    printf("unary_addr node\n");
    if(next_token(T_MUL)) // *
        return make_unary_deref_node();

    if(next_token(T_AND)) { // &
        printf("unary and addr\n");
        return make_unary_addr_node();
    }

    return make_primary_node();
}

static Node *make_unary_deref_node() {
    Node *operand = make_primary_node();
    if(operand->type->kind != TYPE_POINTER) {
        printf("should be pointer variable. but, this is %d\n",
               operand->type->kind);
    }
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_DEREF;
    node->type = operand->type;
    node->operand = operand;
    return node;
}

static Node *make_unary_addr_node() {
    printf("make_unary_addr_node\n");
    Node *operand = make_primary_node();

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_ADDR;
    node->operand = operand;
    return node;
}

static Node *make_mul_sub_node() {
    Node *node = make_unary_node();

    if(next_token(T_MUL))
        return binop(AST_MUL, node, make_unary_node());
    if(next_token(T_DIV))
        return binop(AST_DIV, node, make_unary_node());

    return node;
}

static Node *make_add_sub_node() {
    Node *node = make_mul_sub_node();

    if(next_token(T_ADD))
        return binop(AST_ADD, node, make_mul_sub_node());
    if(next_token(T_SUB))
        return binop(AST_SUB, node, make_mul_sub_node());

    return node;
}

static Node *make_relational_node() {
    Node *node = make_add_sub_node();

    if(next_token(T_LESS_EQ))
        return binop(AST_LESS_EQ, node, make_add_sub_node());
    else if(next_token(T_LESS))
        return binop(AST_LESS, node, make_add_sub_node());
    else if(next_token(T_GRE_EQ))
        return binop(AST_GRE_EQ, node, make_add_sub_node());
    else if(next_token(T_GRE))
        return binop(AST_GRE, node, make_add_sub_node());

    return node;
}

static Node *make_equ_neq_node() {
    Node *node = make_relational_node();

    if(next_token(T_EQUAL))
        return binop(AST_EQUAL, node, make_relational_node());

    return node;
}

static Node *make_logand_node() {
    Node *node = make_equ_neq_node();

    if(next_token(T_BIN_AND))
        return binop(AST_LOG_AND, node, make_equ_neq_node());

    return node;
}

static Node *make_logor_node() {
    Node *node = make_logand_node();

    if(next_token(T_BIN_OR))
        return binop(AST_LOG_OR, node, make_logand_node());

    return node;
}

static Node *make_assign_node() {
    Node *node = make_logor_node();

    if(next_token(T_ASSIGN))
        node = make_actual_assign_node(node, AST_SIMPLE_ASSIGN);

    if(next_token(T_ADD_ASSIGN))
        node = make_actual_assign_node(node, AST_ADD_ASSIGN);

    if(next_token(T_SUB_ASSIGN))
        node = make_actual_assign_node(node, AST_SUB_ASSIGN);

    if(next_token(T_MUL_ASSIGN))
        node = make_actual_assign_node(node, AST_MUL_ASSIGN);

    if(next_token(T_DIV_ASSIGN))
        node = make_actual_assign_node(node, AST_DIV_ASSIGN);

    return node;
}

static Node *make_actual_assign_node(Node *node, int ast_kind) {
    ensure_assign_node(node);
    return ast_binop(node->left->type, ast_kind, node, make_logor_node());
}

static void *ensure_assign_node(Node *node) {
    if(node->kind != AST_GVAR && node->kind != AST_LVAR &&
       node->kind != AST_DEREF) {
        printf("unvalid token error: should AST_GVAR or AST_LVAR\n");
        exit(1);
    }
}

static Node *make_cond_node() { return make_assign_node(); }

static Node *ast_if() {
    ensure_token(T_LPAREN);
    Node *cond = make_cond_node();
    ensure_token(T_RPAREN);

    Node *then;
    if(next_token(T_LBRACE)) {
        then = ast_component();
    } else {
        then = read_decl_or_stmt();
    }

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_IF;
    node->cond = cond;
    node->then = then;
    return node;
}

static Node *ast_while() {

    ensure_token(T_LPAREN);
    Node *cond = make_cond_node();
    ensure_token(T_RPAREN);
    char *begin = make_label();
    char *end = make_label();

    SET_JUMP_LABELS(begin, end);
    Node *then;
    printf("while then\n");

    if(next_token(T_LBRACE)) {
        then = ast_component();
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
        return ast_component();
    case T_IF:
        return ast_if();
    case T_WHILE:
        return ast_while();
    case T_BREAK:
        return ast_break();
    case T_CONTINUE:
        return ast_continue();
    case T_RETURN:
        return make_return_node();
    case T_SEMICOLON:
        return ast_semicolon;
    case T_EOF:
        return ast_eof();
    default:
        unread_token(token);
    }
    Node *node = make_assign_node();
    ensure_token(T_SEMICOLON);
    return node;
}

static Node *read_decl() {
    char *varname;
    Type *basetype = make_decl_type();
    if(next_token(T_SEMICOLON))
        return ast_semicolon;

    Type *declator = read_declator(&varname, basetype, NULL);

    Node *var;
    if(is_global) {
        var = ast_gvar(varname, declator);
        printf("set %s in global vars\n", varname);
    } else {
        var = ast_lvar(varname, declator);
        printf("set %s in local vars\n", varname);
    }

    Node *node;
    if(next_token(T_ASSIGN)) {
        printf("make_decl_and_init_node\n");
        node = make_decl_and_init_node(declator, var, ast_decl(declator));
        printf("make_decl_and_init_node end\n");
    } else if(declator->kind != TYPE_FUNC) {
        node = make_decl_and_init_node(declator, var, NULL);
    }
    ensure_token(T_SEMICOLON);

    return node;
}

static Node *ast_gvar(char *varname, Type *declator) {
    Node *var = (Node *)malloc(sizeof(Node));
    var->kind = AST_GVAR;
    var->ident = varname;
    var->type = declator;

    map_set(global_vars, varname, var);
    return var;
}

static Node *ast_lvar(char *varname, Type *declator) {
    Node *var = (Node *)malloc(sizeof(Node));
    var->kind = AST_LVAR;
    var->ident = varname;
    var->type = declator;
    vec_push(local_vars, var);
    return var;
}

static Type *read_declator(char **name, Type *basetype, Vector *params) {
    /* 本当は8ccのようにnext_token('*')という実装にするのが綺麗だけど、
       今回はこれでいく
    */
    if(next_token(T_MUL)) { // if pointer declare
        printf("pointer declater\n");
        return read_declator(name, make_ptr_type(basetype), params);
    }

    Token *token = lex();
    if(!is_token_kind(token, T_IDENT)) {
        printf("unvalid declare. should be T_IDENT\n");
        exit(1);
    }

    *name = get_token_val(token);
    return read_declator_tail(name, basetype, params);
}

static Type *read_declator_tail(char **name, Type *basetype, Vector *params) {
    if(next_token(T_LBRACKET)) {
        return read_declator_tail_array(name, basetype, params);
    }
    if(next_token(T_LPAREN)) {
        printf("read_declator_tail_func\n");
        return read_declator_tail_func(name, basetype, params);
    }

    return basetype;
}

static Type *read_declator_tail_func(char **name, Type *rettype,
                                     Vector *params) {

    if(next_token(T_RPAREN))
        return make_func_type(rettype, vec_init());

    if(is_type(peek_token())) {
        Vector *paramtypes = vec_init();
        printf("read_declarator_params\n");
        read_declarator_params(paramtypes, params);
        return make_func_type(rettype, paramtypes);
    }
}

static void read_declarator_params(Vector *types, Vector *vars) {
    bool typeonly = !vars;

    for(;;) {
        char *name;
        printf("read_func_param\n");
        Type *ty = read_func_param(&name, typeonly);
        vec_push(types, ty);
        if(!typeonly) {
            vec_push(vars, ast_lvar(name, ty));
        }
        if(next_token(T_RPAREN))
            return;

        ensure_token(T_COMMA);
    }
}

static Type *read_func_param(char **name, bool typeonly) {
    Type *basety = type_int;
    if(is_type(peek_token())) {
        basety = make_decl_type();
    } else {
        printf("ERROR unvalid error\n");
        exit(1);
    }
    Type *ty = read_declator(name, basety, NULL);

    if(ty->kind == TYPE_ARRAY)
        return make_ptr_type(ty->pointer_type);

    return ty;
}

static Type *read_declator_tail_array(char **name, Type *basetype,
                                      Vector *params) {
    int len;

    if(next_token(T_RBRACKET))
        len = -1;
    else {
        len = eval_intval(make_mul_sub_node());
        ensure_token(T_RBRACKET);
    }
    return make_array_type(basetype, len);
}

static Type *make_array_type(Type *base, int len) {
    int size;
    if(len < 0)
        size = -1;
    else
        size = base->size * len;

    Type *type = (Type *)malloc(sizeof(Type));
    type->kind = TYPE_ARRAY;
    type->pointer_type = base;
    type->array_len = len;
    type->size = size;
    return type;
}

static Node *ast_decl(Type *type) {
    if(next_token(T_LBRACE)) {
        printf("make_decl_array_init\n");
        return make_decl_array_init(type);
    }

    return make_logor_node();
}

static Node *make_decl_array_init(Type *type) {
    int elemsize = type->pointer_type->size;
    bool flexible = (type->array_len <= 0);
    Vector *array_list = vec_init();
    int i;
    for(i = 0; flexible || i < type->array_len + 1; i++) {

        if(next_token(T_RBRACE))
            break;

        Node *expr = make_logor_node();
        vec_push(array_list, expr);

        next_token(T_COMMA);
    }

    if(flexible) {
        type->array_len = i;
        type->size = type->pointer_type->size * i;
    }

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_ARRAY_INIT;
    node->array_init_list = array_list;
    return node;
}

static Type *make_ptr_type(Type *basetype) {
    Type *type = (Type *)malloc(sizeof(Type));
    type->kind = TYPE_POINTER;
    type->pointer_type = basetype;
    type->size = 8;

    return type;
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

static Node *ast_break() {
    if(lbreak == NULL) {
        printf("ERROR: unvalid break parser\n");
        exit(1);
    }
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_BREAK;
    node->jmp_label = lbreak;
    return node;
}

static Node *ast_continue() {
    if(lbreak == NULL) {
        printf("ERROR: unvalid continue parser\n");
        exit(1);
    }

    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_CONTINUE;
    node->jmp_label = lcontinue;
    return node;
}

static Node *ast_component() {
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

static Node *ast_newline() {
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_NEWLINE;
    return node;
}

static Node *ast_eof() {
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = AST_EOF;
    return node;
}

Vector *get_top_levels() { return node_vec; }

static Type *make_decl_type() {
    Token *token = lex();
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

static Node *make_decl_and_init_node(Type *type, Node *var, Node *value) {
    Node *node = (Node *)malloc(sizeof(Node));
    node->kind = is_global ? AST_GLOBAL_DECL : AST_LOCAL_DECL;
    node->type = type;
    node->var = var;
    node->val = value;
    return node;
}

int eval_intval(Node *node) {
    switch(node->kind) {
    case AST_LITERAL:
        return node->int_val;
#define L eval_intval(node->left)
#define R eval_intval(node->right)
    case AST_ADD:
        return L + R;
    case AST_SUB:
        return L - R;
    case AST_MUL:
        return L * R;
    case AST_DIV:
        return L / R;
    case AST_LESS_EQ:
        return L <= R;
    case AST_LESS:
        return L < R;
    case AST_GRE:
        return L > R;
    case AST_GRE_EQ:
        return L >= R;
    case AST_LOG_AND:
        return L && R;
    case AST_LOG_OR:
        return L || R;
#undef L
#undef R
    default:
        printf("ERROR!! eval_intval\n");
        exit(1);
    }
}

/*
 * Type conversion
 */

static Node *conv(Node *node) {
    if(!node)
        return NULL;

    Type *ty = node->type;
    switch(ty->kind) {
    case TYPE_ARRAY:
        return ast_uop(AST_CONV, make_ptr_type(ty->pointer_type), node);
    case TYPE_FUNC:
        return ast_uop(AST_ADDR, make_ptr_type(ty), node);
    }
    return node;
}

static Node *ast_uop(int kind, Type *ty, Node *operand) {
    Node *ret = (Node *)malloc(sizeof(Node));
    ret->kind = AST_CONV;
    ret->type = ty;
    ret->operand = operand;
    return ret;
}