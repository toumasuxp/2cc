#include "2cc.h"

static char *infile;

static void parseopt(int argc, char *argv[]) { infile = argv[optind]; }

int main(int argc, char *argv[]) {
    parseopt(argc, argv);
    file_init(infile);
    lex_init();
    parse_init();
    gen_init();
    parse_toplevel();
    printf("start gen code\n");
    Vector *tops = get_top_levels();
    gen_toplevel(tops);

    printf("compile complete!!\n");
    return 1;
}
