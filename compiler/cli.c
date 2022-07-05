#include "compiler.h"

#define say(...) do {printf("  %scompiler%s: ", LAURUS_NOBILIS, NO_COLOR); printf(__VA_ARGS__);} while (0)


int compiler_cli(int argc, char *argv[]) {
    if (argc == 0) {
        say("expected filenames or command (use %scompile help%s for the list)\n", YELLOW_COLOR, NO_COLOR);
    } else if (argc >= 1) {
        if (argc == 1) {
            if (streq(argv[0], "help")) {
                say("command list\n");
                printf("  compile {filenames} [flags]\n");
                printf("    flags:\n");
                printf("      %s-o {filename}%s - output\n", BOLD_WHITE, NO_COLOR);
                return 0;
            }
        }
    }
    return 0;
}