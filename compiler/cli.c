#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>

#define say(...) do {printf("  %scompiler%s: ", LAURUS_NOBILIS, NO_COLOR); printf(__VA_ARGS__);} while (0)

FILE *STREAM = 0;

apply_result_t compiler_fact_receiver(laure_session_t *comp_session, string fact) {
    apply_result_t apply_res;
    laure_parse_result lpr = laure_parse(fact);
    if (! lpr.is_ok) {
        say("PARSE ERROR");
        if (lpr.err)
            printf("    %s\n", lpr.err);
        apply_res.status = apply_error;
        apply_res.error = strdup("parsing error");
        return apply_res;
    }
    assert(STREAM != 0);
    bool is_ok = laure_compile_expression(lpr.exp, STREAM);
    apply_res.error = 0;
    apply_res.status = is_ok ? apply_ok : apply_error;
    return apply_res;
}

int compiler_cli(laure_session_t *comp_session, int argc, char *argv[]) {
    printf("(Laurelang Compiler)\n");
    if (argc == 0) {
        say("use %slaure compile help%s to see help message\n", YELLOW_COLOR, NO_COLOR);
    } else if (argc >= 1) {
        if (argc == 1) {
            if (streq(argv[0], "help")) {
                say("command list\n");
                printf("  compile {filename} {output filename} [-flags]\n");
                return 0;
            } else {
                say("command %s is undefined\n", argv[0]);
            }
        } else {
            string filename = argv[0];
            string output = argv[1];
            printf("COMPILING %s%s%s INTO %s%s%s\n", BOLD_WHITE, filename, NO_COLOR, BOLD_WHITE, output, NO_COLOR);
            for (uint i = 0; i < (argc - 2); i++) {
                string flag = argv[2 + i];
                // apply flag
            }
            STREAM = fopen(output, "w+");
            int failed[1];
            *failed = 0;
            laure_consult_recursive_with_receiver(comp_session, filename, failed, compiler_fact_receiver);
            printf("COMPILATION: ");
            if (*failed)
                printf("%sFAILURE", RED_COLOR);
            else
                printf("%sSUCCESS", GREEN_COLOR);
            printf("%s\n", NO_COLOR);
            return *failed;
        }
    }
    return 0;
}