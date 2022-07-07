#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>

#define say(...) do {printf("  %scompiler%s: ", LAURUS_NOBILIS, NO_COLOR); printf(__VA_ARGS__);} while (0)

FILE *STREAM = 0;

apply_result_t compiler_fact_receiver(laure_session_t *comp_session, string fact) {
    if (str_starts(fact, "//")) {
        apply_result_t apply_res;
        apply_res.status = apply_ok;
        return apply_res;
    }
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
    debug("compiling %s %s\n", EXPT_NAMES[lpr.exp->t], lpr.exp->s ? lpr.exp->s : "");
    assert(STREAM != 0);
    bool is_ok = laure_compiler_compile_expression(lpr.exp, STREAM);
    apply_res.error = 0;
    apply_res.status = is_ok ? apply_ok : apply_error;
    return apply_res;
}

void laure_consult_bytecode(laure_session_t *session, FILE *file) {
    Bitstream *bits = bitstream_new(file);
    laure_compiler_consult_bytecode(session, bits);
    free(bits);
}

int laure_compiler_cli(laure_session_t *comp_session, int argc, char *argv[]) {
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
            for (uint i = 0; i < (argc - 2); i++) {
                string flag = argv[2 + i];
                // apply flag
            }
            STREAM = fopen(output, "wb");
            debug("writing signature\n");
            assert(SIGNATURE_CHARBITS <= 8);
            printf("SIGNATURE: %s%s%s (loss %db)\n", GRAY_COLOR, SIGNATURE, NO_COLOR, 8 - SIGNATURE_CHARBITS);
            write_signature(STREAM);
            int failed[1];
            *failed = 0;
            printf("COMPILING %s%s%s INTO %s%s%s\n", BOLD_WHITE, filename, NO_COLOR, BOLD_WHITE, output, NO_COLOR);
            debug("running consult with compiler endpoint\n");
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