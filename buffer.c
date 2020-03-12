#include "2cc.h"

#define INIT_BUF_SIZE 20

static void buf_extend_realloc(Buffer *buf);
static int roundup(int n);
static void buf_extend(Buffer *buf);

struct _Buffer {
    char *body;
    int size;
    int len;
};

Buffer *make_buffer() {
    Buffer *buf = (Buffer *)malloc(sizeof(Buffer));
    buf->body = (char *)malloc(sizeof(char) * INIT_BUF_SIZE);
    buf->size = INIT_BUF_SIZE;
    buf->len = 0;
    return buf;
}

static int roundup(int n) {
    if(n == 0)
        return 0;
    int r = 1;
    while(n > r)
        r *= 2;
    return r;
}

static void buf_extend(Buffer *buf) {
    if(buf->len + 1 < buf->size)
        return;
    int new_size = roundup(buf->len + 1);
    char *new_buf = (char *)malloc(sizeof(char) * new_size);
    memcpy(new_buf, buf->body, buf->len);
    free(buf->body);
    buf->body = new_buf;
    buf->size = new_size;
}

static void buf_extend_realloc(Buffer *buf) {
    int new_size = buf_get_size(buf) * 2;
    Buffer *new_buf = make_buffer();
    new_buf->size = new_size;
    new_buf->len = buf->len;
    memcpy(new_buf, buf->body, buf->len);
    free(buf);

    buf = new_buf;
}

void buf_write(Buffer *buf, int c) {
    buf_extend(buf);

    buf->body[buf->len++] = c;
}

char *vformat(char *fmt, va_list ap) {
    Buffer *b = make_buffer();
    va_list aq;
    while(1) {
        int avail = buf_get_size(b) - buf_get_len(b);
        va_copy(aq, ap);
        int written =
            vsnprintf(buf_get_body(b) + buf_get_len(b), avail, fmt, aq);
        va_end(aq);
        if(avail <= written) {
            buf_extend_realloc(b);
            continue;
        }
        b->len += written;
        return buf_get_body(b);
    }
}

char *format(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *r = vformat(fmt, ap);
    va_end(ap);
    return r;
}

char *buf_get_body(Buffer *buf) { return buf->body; }

int buf_get_size(Buffer *buf) { return buf->size; }

int buf_get_len(Buffer *buf) { return buf->len; }