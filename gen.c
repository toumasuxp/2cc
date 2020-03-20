#include "2cc.h"

static FILE *output_fp = NULL;

static void emit_global_decl(Node *node);

static void ensure_gvar_init(Node *node);
static void emit_gsave(char *varname);
static void emit_expr(Node *node);
static void emit_literal(Node *node);
static void emit_binary(Node *node);
static void emit_if(Node *node);
static void emit_while(Node *node);
static void emit_continue(Node *node);
static void emit_break(Node *node);
static void emit_component(Node *node);
static void emit_assign(Node *node);

static void emit_funcdef(Node *node);

static char *get_resigter_size();
static void emitf(int line, char *fmt, ...);

static void emit_jmp(char *label);
static void emit_je(char *label);
static void emit_label(char *label);
static void emit_cmp(char *inst, Node *node);
static void emit_arithmetic(char *inst, Node *node);
static void emit_log_and(Node *node);
static void emit_log_or(Node *node);

static void emit_data_primitive(Node *node);
static int eval_intval(Node *node);

static void emit_funcdef(Node *func);
static void emit_func_prologue(Node *func);
static void emit_ret();

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
    Node *node = NULL;
    int i = 0;
    for(node = (Node *)vec_get(toplevel, i++); node != NULL;
        node = (Node *)vec_get(toplevel, i++)) {
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
    emit_func_prologue(func);
    emit_expr(func->body);
    emit_ret();
}

static void emit_func_prologue(Node *func) {
    emit_label(func->func_name);

    push("rbp");
    emit("mov #rsp, #rbp");

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

static void emit_ret() {
    emit("leave");
    emit("ret");
}

static void emit_global_decl(Node *node) {
    emit(".data");
    emit_label(node->ident);
    emit_data_primitive(node);
}

static void emit_data_primitive(Node *node) {
    switch(node->type->size) {
    case 1:
        emit(".byte %d", eval_intval(node->val));
        break;
    case 2:
        emit(".word %d", eval_intval(node->val));
        break;
    case 4:
        emit(".int %d", eval_intval(node->val));
        break;
    }
}

static int eval_intval(Node *node) {
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

static void emit_literal(Node *node) { emit("mov $%d, #rax", node->int_val); }

static void emit_expr(Node *node) {
    switch(node->kind) {
    case AST_LITERAL:
        emit_literal(node);
        break;
    case AST_GLOBAL_DECL:
        emit_global_decl(node);
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
    default:
        emit_binary(node);
        break;
    }
}

static void emit_component(Node *node) {
    Vector *stmt = node->stmt;

    int i;
    for(i = 0; i < vec_len(stmt); i++)
        emit_expr(vec_get(stmt, i));
}

static void ensure_gvar_init(Node *node) { emit_expr(node); }

static void emit_binary(Node *node) {
    char *op = NULL;
    switch(node->kind) {
    case AST_SIMPLE_ASSIGN:
        emit_assign(node);
        return;
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

static void emit_assign(Node *node) {
    push("rax");
    emit_expr(node->right);
    emit("mov #rax, %s(#rip)", node->left->varname);
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

static void emit_je(char *label) {
    emit("test #rax, #rax");
    emit("je %s", label);
}

static void emit_jmp(char *label) { emit("jmp %s", label); }

static void emit_label(char *label) { emit_noindent("%s:", label); }

static void emit_gsave(char *varname) {
    char *reg = get_resigter_size();
    char *addr = format("%s(%%rip)", varname);
    emit("mov #%s, %s", reg, addr);
}

static char *get_resigter_size() { return "rax"; }

static void push(char *reg) { emit("push #%s", reg); }
static void pop(char *reg) { emit("pop #%s", reg); }

char *make_label() {
    static int c = 0;
    return format(".L%d", c++);
}