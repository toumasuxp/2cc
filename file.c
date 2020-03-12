#include "2cc.h"

struct _File {
    FILE *fp;
    int column;
    int line;
    int buf[10];
    int buf_len;
};

static File *main_file;

static int is_whitespace(int c);
static int read_file_buf();

static int is_whitespace(int c) {
    return (c == ' ' || c == '\t' || c == '\f' || c == '\v');
}

static int read_file_buf() { return main_file->buf[--main_file->buf_len]; }

void file_init(char *filename) {
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
    main_file = file;
}

int readc() {
    int c;

    if(main_file->buf_len > 0) {
        c = read_file_buf();
    } else {
        c = getc(main_file->fp);
    }

    if(c == '\n') {
        main_file->line++;
        main_file->column = 1;
    } else {
        main_file->column++;
    }
    return c;
}

void unreadc(int c) {
    if(c == EOF)
        return;

    main_file->buf[main_file->buf_len++] = c;

    if(c == '\n') {
        main_file->column = 1;
        main_file->line--;
    } else {
        main_file->column--;
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
