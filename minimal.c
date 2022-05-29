#include "laurelang.h"
#include "predpub.h"
#include <stdio.h>
#include <readline/readline.h>

int main() {
    printf("laure (minimal)\n");
    laure_session_t *session = laure_session_new();
    laure_set_translators();
    laure_init_name_buffs();
    laure_register_builtins(session);
    string line;
    while ((line = readline("?- ")) != NULL) {
        laure_parse_many_result lpmr = laure_parse_many(line, ',', NULL);
        if (! lpmr.is_ok)
            printf("error: %s\n", lpmr.err);
        else {
            qcontext *qctx = qcontext_new(laure_expression_compose(lpmr.exps));
            var_process_kit *vpk = laure_vpk_create(lpmr.exps);
            control_ctx *cctx = control_new(session, session->scope, qctx, vpk, NULL, false);
            qresp response = laure_start(cctx, qctx->expset);
            if (response.state == q_error)
                printf("error: %s\n", response.payload);
            else if (! response.payload)
                printf("false\n");
        }
    }
    return 0;
}