#include "2cc.h"

static FILE *output_fp = NULL;

static void emit_global_decl(Node *node);

static void ensure_gvar_init(Node *node);
static void emit_gsave(char *varname);
static void emit_expr(Node *node);
static void emit_literal(Node *node);
static void emit_binary(Node *node);
static void emit_if(Node *node);

static char *get_resigter_size();
static void emitf(int line, char *fmt, ...);

static char *make_lavel();

static void emit_je(char *label);
static void emit_label(char *label);

static void push(char *reg);
static void pop(char *reg);

#define emit(...) emitf(__LINE__, "\t" __VA_ARGS__)

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
        case AST_IF:
            emit_if(node);
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

static void emit_global_decl(Node *node) {
    ensure_gvar_init(node->value);
    emit_gsave(node->varname);
}

static void emit_literal(Node *node) { emit("mov $%s, #rax", node->val); }

static void emit_expr(Node *node) {
    switch(node->kind) {
    case AST_LITERAL:
        emit_literal(node);
        break;

    default:
        emit_binary(node);
        break;
    }
}

static void ensure_gvar_init(Node *node) { emit_expr(node); }

static void emit_binary(Node *node) {
    char *op = NULL;
    switch(node->kind) {
    case AST_ADD:
        op = "add";
        break;
    case AST_SUB:
        op = "sub";
        break;
    case AST_MUL:
        op = "imul";
        break;
    }

    emit_expr(node->left);
    push("rax");
    emit_expr(node->right);
    emit("mov #rax, #rcx");
    pop("rax");

    if(node->kind == AST_DIV) {
        emit("xor #edx, #edx");
        emit("div #rcx");
    } else {
        emit("%s #rcx, #rax", op);
    }
}

static void emit_if(Node *node) {
    emit_expr(node->cond);
    char *label = make_lavel();
    emit_je(label);
    if(node->then)
        emit_expr(node->then);

    emit_label(label);
}

static void emit_je(char *label) {
    emit("test #rax, #rax");
    emit("je %s", label);
}

static void emit_label(char *label) { emit("%s:", label); }

static void emit_gsave(char *varname) {
    char *reg = get_resigter_size();
    char *addr = format("%s(%%rip)", varname);
    emit("mov #%s, %s", reg, addr);
}

static char *get_resigter_size() { return "rax"; }

static void push(char *reg) { emit("push #%s", reg); }
static void pop(char *reg) { emit("pop #%s", reg); }

static char *make_lavel() {
    static int c = 0;
    return format(".L%d", c++);
}