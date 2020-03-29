#include "2cc.h"

struct _File {
    FILE *fp;
    char *file_str;
    int column;
    int line;
    int buf[10];
    int buf_len;
};

static Vector *files;

static int is_whitespace(int c);
static int read_file_buf();

static int is_whitespace(int c) {
    return (c == ' ' || c == '\t' || c == '\f' || c == '\v');
}

static int read_file_buf() {
    return current_file()->buf[--current_file()->buf_len];
}

void file_init(char *filename) {
    files = vec_init();
    FILE *fp;
    if((fp = fopen(filename, "r")) == NULL) {
        printf("cannot open file\n");
        exit(1);
    }

    File *file = (File *)malloc(sizeof(File));
    file->fp = fp;
    file->column = 1;
    file->line = 1;
    file->buf_len = 0;

    vec_push(files, file);
}

int readc() {
    int c;

    if(current_file()->buf_len > 0) {
        c = read_file_buf();
    } else {
        c = getc(current_file()->fp);
    }

    if(c == '\n') {
        current_file()->line++;
        current_file()->column = 1;
    } else {
        current_file()->column++;
    }
    return c;
}

bool expectc(int c) {
    char next = readc();
    if(next == c)
        return true;

    unreadc(next);
    return false;
}

void unreadc(int c) {
    if(c == EOF)
        return;

    current_file()->buf[current_file()->buf_len++] = c;

    if(c == '\n') {
        current_file()->column = 1;
        current_file()->line--;
    } else {
        current_file()->column--;
    }
}

void skip_space() {
    int c;

    while(1) {
        c = readc();
        if(is_whitespace(c)) {
            continue;
        } else {
            unreadc(c);
            return;
        }
    }
}

File *current_file() { return vec_tail(files); }