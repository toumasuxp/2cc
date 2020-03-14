#include "2cc.h"

int main() {
    lex_init();
    parse_init();
    gen_init();
    file_init("sample.dd");
    parse_toplevel();
    Vector *tops = get_top_levels();
    gen_toplevel(tops);

    printf("compile complete!!\n");
    return 1;
}
