#include "2cc.h"

static FILE *output_fp = NULL;

static char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r9", "r11"};

static void emit_global_decl(Node *node);

static void ensure_gvar_init(Node *node);
static void emit_store(Node *node, int kind);
static void emit_gsave(Node *var);
static void emit_lsave(int loff);
static void emit_expr(Node *node);
static void emit_literal(Node *node);
static void emit_binary(Node *node);
static void emit_if(Node *node);
static void emit_while(Node *node);
static void emit_continue(Node *node);
static void emit_return(Node *node);
static void emit_break(Node *node);
static void emit_component(Node *node);
static void emit_assign(Node *node);

static char *get_resigter_size();
static void emitf(int line, char *fmt, ...);

static void emit_jmp(char *label);
static void emit_je(char *label);
static void emit_label(char *label);
static void emit_cmp(char *inst, Node *node);
static void emit_arithmetic(char *inst, Node *node);
static void emit_log_and(Node *node);
static void emit_log_or(Node *node);

static void emit_global_data(Node *node);
static void emit_global_data_array(Node *node);
static void emit_data_primitive(Node *val, Type *type);

static void emit_funcdef(Node *func);
static void emit_func_prologue(Node *func);
static void emit_ret();
static void emit_func_call(Node *node);
static void push_func_params(Vector *params);

static void emit_deref(Node *node);
static void emit_addr(Node *node);
static void emit_assign_deref(Node *node);

static void emit_lvar(Node *node);

static void emit_local_decl(Node *node);

static void emit_lload(Node *node, char *inst);
static void emit_gvar(Node *node);

static char *get_data_size(int size);
static void emit_local_data_array(Node *node);
static void emit_pointer_arith(int kind, Node *left, Node *right);
static void emit_conv(Node *node);

static void push(char *reg);
static void pop(char *reg);

#define emit(...) emitf(__LINE__, "\t" __VA_ARGS__)
#define emit_noindent(...) emitf(__LINE__, "" __VA_ARGS__)

void gen_init() {
    char *output_filename = "sample.s";

    if((output_fp = fopen(output_filename, "w")) == NULL) {
        printf("ERROR!! cannot open output file\n");
        exit(1);
    }
}

void gen_toplevel(Vector *toplevel) {
    for(int i = 0; i < vec_len(toplevel); i++) {
        Node *node = (Node *)vec_get(toplevel, i);
        switch(node->kind) {
        case AST_GLOBAL_DECL:
            emit_global_decl(node);
            break;
        case AST_FUNCDEF:
            emit_funcdef(node);
            break;
        case AST_END:
            return;
        }
    }
}

static void emitf(int line, char *fmt, ...) {
    char buf[256];
    int i = 0;

    for(char *p = fmt; *p; p++) {
        if(*p == '#') {
            buf[i++] = '%';
            buf[i++] = '%';
        } else {
            buf[i++] = *p;
        }
    }
    buf[i] = '\0';

    va_list args;
    va_start(args, fmt);
    int col = vfprintf(output_fp, buf, args);
    va_end(args);

    fprintf(output_fp, "\n");
}

static void emit_funcdef(Node *func) {
    printf("emit_funcdef\n");
    emit_func_prologue(func);
    emit_expr(func->body);
    emit_ret();
    printf("emit_funcdef end\n");
}

static void emit_func_prologue(Node *func) {
    emit_label(func->func_name);

    push("rbp");
    emit("mov #rsp, #rbp");

    push_func_params(func->params);

    int off = 0;
    int local_sizes = 0;
    for(int i = 0; i < vec_len(func->local_vars); i++) {
        Node *var = vec_get(func->local_vars, i);
        int size = var->type->size;
        off -= size;
        var->loff = off;
        local_sizes += size;
    }

    if(local_sizes) {
        emit("sub $%d, #rsp", local_sizes);
    }
}

static void push_func_params(Vector *params) {
    int nreg = 0;
    int off = 0;
    for(int i = 0; i < vec_len(params); i++) {
        Param *param = vec_get(params, i);

        push(regs[nreg++]);
        off -= 8;
        param->loff = off;
    }
}

static int get_param_len(Node *func) { return vec_len(func->params); }

static void emit_ret() {
    emit("leave");
    emit("ret");
}

static void emit_global_decl(Node *node) {
    emit(".data");
    emit_label(node->ident);
    emit_global_data(node);
}

static void emit_global_data(Node *node) {
    switch(node->type->kind) {
    case TYPE_ARRAY:
        emit_global_data_array(node);
        break;
    default:
        emit_data_primitive(node->val, node->type);
    }
}

static void emit_global_data_array(Node *node) {

    for(int i = 0; i < node->type->array_len; i++) {
        Node *val = vec_get(node->val->array_init_list, i);

        if(val) {
            emit_data_primitive(val, node->type->pointer_type);
        } else {
            char *data_size = get_data_size(node->type->pointer_type->size);
            emit("%s 0", data_size);
        }
    }
}

static char *get_data_size(int size) {
    switch(size) {
    case 1:
        return format(".byte ");
    case 4:
        return format(".int ");
    case 8:
        return format(".quad ");
    }
}

static void emit_local_decl(Node *node) {
    printf("emit_local_decl\n");
    switch(node->type->kind) {
    case TYPE_ARRAY:
        emit_local_data_array(node);
        break;
    default:
        emit_expr(node->val);
        emit_lsave(node->loff);
    }
}

static void emit_local_data_array(Node *node) {
    Node *val;
    for(int i = 0; i < vec_len(node->val->array_init_list); i++) {
        int loff = node->loff + node->type->pointer_type->size * i;
        Node *val = vec_get(node->val->array_init_list, i);
        emit_expr(val);
        emit_lsave(loff);
    }
}

static void emit_data_primitive(Node *val, Type *type) {
    switch(type->size) {
    case 1:
        emit(".byte %d", eval_intval(val));
        break;
    case 2:
        emit(".word %d", eval_intval(val));
        break;
    case 4:
        emit(".int %d", eval_intval(val));
        break;
    }
}

static void emit_literal(Node *node) { emit("mov $%d, #rax", node->int_val); }

static void emit_expr(Node *node) {
    switch(node->kind) {
    case AST_LITERAL:
        emit_literal(node);
        break;
    case AST_GVAR:
        printf("emit_addr AST_GVAR\n");
        emit_gvar(node);
        break;
    case AST_LVAR:
        emit_lvar(node);
        break;
    case AST_DEREF:
        emit_deref(node);
        break;
    case AST_ADDR:
        printf("emit_addr AST_ADDR\n");
        emit_addr(node->operand);
        break;
    case AST_FUNC_CALL:
        emit_func_call(node);
        break;
    case AST_CONV:
        emit_conv(node);
        break;
    case AST_SIMPLE_ASSIGN:
        emit_assign(node);
        break;
    case AST_GLOBAL_DECL:
        emit_global_decl(node);
        break;
    case AST_LOCAL_DECL:
        emit_local_decl(node);
        break;
    case AST_COMPONENT:
        emit_component(node);
        break;
    case AST_BREAK:
        emit_break(node);
        break;
    case AST_CONTINUE:
        emit_continue(node);
        break;
    case AST_RETURN:
        emit_return(node);
        break;
    default:
        emit_binary(node);
        break;
    }
}

static void emit_conv(Node *node) { emit_expr(node->operand); }

static void emit_component(Node *node) {
    Vector *stmt = node->stmt;

    int i;
    for(i = 0; i < vec_len(stmt); i++)
        emit_expr(vec_get(stmt, i));
}

static void emit_lvar(Node *node) { emit_lload(node, "rbp"); }

static void emit_gvar(Node *node) {}
static void ensure_gvar_init(Node *node) { emit_expr(node); }

static void emit_func_call(Node *node) {
    // 引数をサポートしていないので、callを作成するだけ
    emit("call %s", node->call_func_name);
}

static void emit_binary(Node *node) {
    printf("emit_binary\n");
    if(node->type->kind == TYPE_POINTER) {
        emit_pointer_arith(node->kind, node->left, node->right);
        return;
    }
    char *op = NULL;
    switch(node->kind) {
    case AST_EQUAL:
        emit_cmp("sete", node);
        return;
    case AST_LESS_EQ:
        emit_cmp("setle", node);
        return;
    case AST_LESS:
        emit_cmp("setl", node);
        return;
    case AST_LOG_AND:
        emit_log_and(node);
        return;
    case AST_LOG_OR:
        emit_log_or(node);
        return;
    case AST_GRE_EQ:
        emit_cmp("setge", node);
        return;
    case AST_GRE:
        emit_cmp("setg", node);
        return;
    case AST_ADD:
        emit_arithmetic("add", node);
        return;
    case AST_SUB:
        emit_arithmetic("sub", node);
        return;
    case AST_MUL:
        emit_arithmetic("imul", node);
        return;
    case AST_DIV:
        emit_arithmetic("div", node);
    }
}

static void emit_pointer_arith(int kind, Node *left, Node *right) {
    printf("emit_pointer_arith\n");
    emit_expr(left);
    push("rcx");
    push("rax");
    emit_expr(right);
    int size = left->type->pointer_type->size;
    if(size > 1)
        emit("imul $%d, #rax", size);
    emit("mov #rax, #rcx");
    pop("rax");
    switch(kind) {
    case AST_ADD:
        emit("add #rcx, #rax");
        break;
    case AST_SUB:
        emit("sub #rcx, #rax");
        break;
    default:
        printf("invalid operator '%d'\n", kind);
        exit(1);
    }
    pop("rcx");
}

static void emit_assign(Node *node) {
    printf("emit_assign\n");
    push("rax");
    emit_label("emit_assign");
    emit_label("emit_assign_expr");
    emit_expr(node->right);
    emit_label("emit_assign_expr_end");
    emit_store(node->left, node->left->kind);
    emit_label("emit_assign_end");
}

static void emit_store(Node *node, int kind) {
    switch(kind) {
    case AST_DEREF:
        printf("emit_store AST_DEREF\n");
        emit_label("emit_assign_deref");
        emit_assign_deref(node);
        emit_label("emit_assign_deref_end");
        return;
    case AST_LVAR:
        printf("emit_store AST_LVAR\n");
        emit_lsave(node->loff);
        return;
    case AST_GVAR:
        printf("emit_store AST_GVAR\n");
        emit_gsave(node);
        return;
    }
}

static void emit_gsave(Node *var) {
    char *reg = get_resigter_size();
    char *addr = format("%s(%%rip)", var->ident);
    emit("mov %s, %s", reg, addr);
}

static void emit_lsave(int loff) {
    char *reg = get_resigter_size();
    char *addr = format("%d(%%rbp)", loff);
    emit("mov %s, %s", reg, addr);
}

static void emit_assign_deref(Node *node) {
    push("rax");
    emit_expr(node->operand);
    emit("mov (#rsp), #rcx");
    emit("mov #rcx, %d(#rax)", node->operand->loff);
    pop("rax");
}

static void emit_arithmetic(char *inst, Node *node) {
    emit_expr(node->left);
    push("rax");
    emit_expr(node->right);
    emit("mov #rax, #rcx");
    pop("rax");

    if(node->kind == AST_DIV) {
        emit("xor #edx, #edx");
        emit("div #rcx");
    } else {
        emit("%s #rcx, #rax", inst);
    }
}

static void emit_log_and(Node *node) {
    char *end = make_label();
    emit_expr(node->left);
    emit("test #rax, #rax");
    emit("mov $0, #rax");
    emit("je %s", end);
    emit_expr(node->right);
    emit("test #rax, #rax");
    emit("mov $0, #rax");
    emit("je %s", end);
    emit("mov $1, #rax");
    emit_label(end);
}

static void emit_log_or(Node *node) {
    char *end = make_label();
    emit_expr(node->left);
    emit("test #rax, #rax");
    emit("mov $1, #rax");
    emit("je %s", end);
    emit_expr(node->right);
    emit("test #rax, #rax");
    emit("mov $1, #rax");
    emit("je %s", end);
    emit("mov $0, #rax");
    emit_label(end);
}

static void emit_cmp(char *inst, Node *node) {
    emit_expr(node->left);
    push("rax");
    emit_expr(node->right);
    pop("rcx");
    emit("cmp #eax, #ecx");
    emit("%s #al", inst);
    emit("movzb #al, #eax");
}

static void emit_if(Node *node) {
    emit_expr(node->cond);
    char *label = make_label();
    emit_je(label);
    if(node->then) {
        printf("FILE %s, LINE %d\n", __FILE__, __LINE__);
        emit_expr(node->then);
    }

    emit_label(label);
}

static void emit_while(Node *node) {
    emit_label(node->lbegin);
    emit_expr(node->cond);
    emit_je(node->lend);
    emit_expr(node->then);
    emit_jmp(node->lbegin);
    emit_label(node->lend);
}

static void emit_break(Node *node) { emit_jmp(node->jmp_label); }

static void emit_continue(Node *node) { emit_jmp(node->jmp_label); }

static void emit_return(Node *node) {
    if(node->retval)
        emit_expr(node->retval);

    emit_ret();
}

static void emit_deref(Node *node) {
    printf("emit_deref\n");
    emit_expr(node->operand);
    emit_lload(node->operand, "rax");
}

static void emit_lload(Node *node, char *inst) {
    if(node->type->kind == TYPE_ARRAY) {
        emit("lea %d(#%s), #rax", node->loff, inst);
    } else
        emit("mov %d(#%s), #rax", node->loff, inst);
}

static void emit_addr(Node *node) {
    printf("emit_addr\n");
    switch(node->kind) {
    case AST_LVAR:
        printf("emit_addr LVAR\n");
        emit("lea %d(#rbp), #rax", node->loff);
        return;
    case AST_GVAR:
        printf("emit_addr GVAR\n");
        emit("lea %d+%s(#rip), #rax", node->loff, node->ident);
        return;
    }
}

static void emit_je(char *label) {
    emit("test #rax, #rax");
    emit("je %s", label);
}

static void emit_jmp(char *label) { emit("jmp %s", label); }

static void emit_label(char *label) { emit_noindent("%s:", label); }

static char *get_resigter_size() { return "rax"; }

static void push(char *reg) { emit("push #%s", reg); }
static void pop(char *reg) { emit("pop #%s", reg); }

char *make_label() {
    static int c = 0;
    return format(".L%d", c++);
}