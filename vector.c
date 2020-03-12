#include "2cc.h"

#define INIT_VEC_SIZE 10

struct _Vector {
    int size;
    int len;
    void **body;
};

Vector *vec_init() {
    Vector *vec = (Vector *)malloc(sizeof(Vector));
    vec->size = INIT_VEC_SIZE;
    vec->len = 0;
    vec->body = malloc(sizeof(void *) * INIT_VEC_SIZE);
    return vec;
}

static int roundup(int n) {
    if(n == 0)
        return 0;
    int r = 1;
    while(n > r)
        r *= 2;
    return r;
}

static void extend(Vector *vec, int len) {
    if(vec->len + len <= vec->size)
        return;
    int new_size = roundup(vec->len + len);
    void *new_body = malloc(sizeof(void *) * new_size);
    memcpy(new_body, vec->body, vec->len);
    free(vec->body);
    vec->body = new_body;
    vec->size = new_size;
}

void *vec_get(Vector *vec, int index) { return vec->body[index]; }

void vec_push(Vector *vec, void *elem) {
    extend(vec, 1);
    vec->body[vec->len++] = elem;
}

void *vec_pop(Vector *vec) { return vec->body[--vec->len]; }

void *vec_head(Vector *vec) { return vec->body[0]; }

void *vec_tail(Vector *vec) {
    if(vec->len - 1 >= 0)
        return vec->body[vec->len - 1];
    else
        return vec->body[0];
}

void *vec_body(Vector *vec) { return vec->body; }

int vec_len(Vector *vec) { return vec->len; }