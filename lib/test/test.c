#include <laurelang.h>
#include <predpub.h>
#include <laureimage.h>
#include <builtin.h>
#include <math.h>
#include <builtin.h>

#define test_suite_version "0.1"
#define up printf("\033[A")
#define erase printf("\33[2K\r")

#define DOC_tests_run "used to run all tests\ntest name should start with `test_`"

struct receiver_payload {
    char **data;
    uint data_cnt;
    bool passed;
    uint idx;
};

bool test_suite_receiver(string repr, struct receiver_payload *payload) {
    if (! payload->data) {
        return true;
    }
    assert(repr[0] == '=');
    repr++;
    if (payload->idx > payload->data_cnt - 1) {
        payload->passed = false;
        return false;
    } else if (strcmp(repr, payload->data[payload->idx]) != 0) {
        payload->passed = false;
        return false;
    } else {
        payload->idx++;
        return true;
    }
}

qresp test_predicate_run(preddata *pd, control_ctx *cctx) {
    printf("\n(Running test suite v%s)\n", test_suite_version);
    printf("--- Collecting tests ---\n");

    Instance *tests[256];
    memset(tests, 0, sizeof(void*) * 256);
    int len = 0;

    Cell cell;
    STACK_ITER(cctx->stack->global, cell, {
        if (str_starts(cell.instance->name, "test_")) {
            if (len >= 255) {
                printf("too many tests. max 256 per suite\n");
                break;
            }
            ImageHead h = read_head(cell.instance->image);
            if (h.t == PREDICATE_FACT) {
                tests[len] = cell.instance;
                len++;
            }
        }
    }, false);

    if (!len) {
        printf("Collected 0 tests. Terminating.\n");
        return respond(q_true, NULL);
    }

    printf("Collected %d tests.\n", len);
    printf("--- Running tests ---\n");
    uint tests_passed = 0;

    laure_session_t *sess = malloc(sizeof(laure_session_t));
    sess->stack = laure_stack_init(cctx->stack->global);
    sess->_included_filepaths[0] = 0;
    sess->_doc_buffer = NULL;
    sess->signal = 0;
    laure_trace_init();
    void *old_sess = LAURE_SESSION;
    LAURE_SESSION = sess;

    for (int i = 0; i < len; i++) {
        Instance *predicate = tests[i];
        struct PredicateImage *pred_im = (struct PredicateImage*)predicate->image;
        printf("%s: %srunning%s\n", predicate->name, YELLOW_COLOR, NO_COLOR);

        int argc = 0;
        char **data = 0;
        if (predicate->doc && pred_im->header.args->len > 0) {
            argc = 1;
            for (int j = 0; j < strlen(predicate->doc); j++) {
                if (predicate->doc[j] == '\n') argc++;
            }
            data = malloc(sizeof(void*) * argc);
            char *ptr = strtok(predicate->doc, "\n");
            int j = 0;
            while (ptr) {
                data[j] = ptr;
                j++;
                ptr = strtok(NULL, "\n");
            }
        }

        char query[256] = {0};
        strcpy(query, predicate->name);
        strcat(query, "(");
        for (int j = 0; j < pred_im->header.args->len; j++) {
            char argn[12];
            snprintf(argn, 12, "__%d", j);
            strcat(query, argn);
            if (j != pred_im->header.args->len - 1) strcat(query, ",");
        }
        strcat(query, ")");

        laure_parse_result res = laure_parse(query);
        if (!res.is_ok) {
            return respond(q_error, res.err);
        }

        laure_expression_set *expset = laure_expression_compose_one(res.exp);
        control_ctx *ncctx = laure_control_ctx_get(sess, expset);
        ncctx->vpk->sender_receiver = test_suite_receiver;
        struct receiver_payload *payload = malloc(sizeof(struct receiver_payload));
        payload->idx = 0;
        payload->passed = true;
        payload->data = data;
        payload->data_cnt = argc;
        ncctx->vpk->payload = payload;
        ncctx->vpk->mode = SENDER;

        LAURE_RECURSION_DEPTH = 0;
        qresp response = laure_eval(ncctx, expset);

        up;
        erase;

        if (! payload->passed) {
            printf("%s: %sfailed%s (incorrect gen)%s\n", predicate->name, RED_COLOR, GRAY_COLOR, NO_COLOR);
        } else if (payload->idx != payload->data_cnt) {
            printf("%s: %sfailed%s (%d ignored)%s\n", predicate->name, RED_COLOR, GRAY_COLOR, payload->data_cnt - payload->idx, NO_COLOR);
        } else if (payload->data_cnt > 0) {
            // all found
            printf("%s: %spassed%s\n", predicate->name, GREEN_COLOR, NO_COLOR);
            tests_passed++;
        } else {
            switch (response.state) {
                case q_true: {
                    printf("%s: %spassed%s\n", predicate->name, GREEN_COLOR, NO_COLOR);
                    tests_passed++;
                    break;
                }
                case q_false:
                case q_error: {
                    laure_trace_erase();
                    printf("%s: %sfailed%s\n", predicate->name, RED_COLOR, NO_COLOR);
                    break;
                }
                default: {
                    printf("%s: %sunknown%s\n", predicate->name, GRAY_COLOR, NO_COLOR);
                    break;
                }
            }
        }

        
    }

    laure_stack_free(sess->stack);
    free(sess);

    double passed_percent = ((double)tests_passed / (double)len) * 100;

    printf("--- Result %s%d%%%s ---\n", passed_percent >= 70 ? GREEN_COLOR : RED_COLOR, (int)round(passed_percent), NO_COLOR);

    LAURE_SESSION = old_sess;
    return respond(tests_passed == len ? q_true : q_false, NULL);
}

int package_include(laure_session_t *session) {
    laure_cle_add_predicate(
        session, "tests_run", 
        test_predicate_run, 
        0, NULL, NULL, false, 
        DOC_tests_run
    );
    return 0;
}