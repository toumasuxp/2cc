#include "2cc.h"

#define INIT_MAP_SIZE 30

static void map_extend(Map *map);

struct _Map {
    int size;
    int len;
    char **key;
    void **val;
};

Map *map_init() {
    Map *map = (Map *)malloc(sizeof(Map));
    map->size = INIT_MAP_SIZE;
    map->len = 0;
    map->key = (char **)malloc(sizeof(char *) * INIT_MAP_SIZE);
    map->val = (void **)malloc(sizeof(void *) * INIT_MAP_SIZE);
    return map;
}

void *map_get(Map *map, char *key) {
    int i;

    for(i = map->len - 1; i >= 0; i--) {
        if(!strcmp(key, map->key[i]))
            return map->val[i];
    }

    return NULL;
}

void map_set(Map *map, char *key, void *val) {
    map_extend(map);
    map->key[map->len] = key;
    map->val[map->len] = val;
    map->len++;
}

static void map_extend(Map *map) {
    if(map->len + 3 < map->size)
        return;

    int size = map->size * 2;
    char **key = (char **)malloc(sizeof(char *) * size);
    void **val = (void **)malloc(sizeof(void *) * size);
    int len = map->len - 1;
    memcpy(key, map->key, len);
    memcpy(val, map->val, len);
    map->key = key;
    map->val = val;
    map->size = size;
}