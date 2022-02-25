#include <laurelang.h>
#include <predpub.h>
#include <laureimage.h>
#include <builtin.h>
#include <math.h>
#include <builtin.h>
#include <time.h>

#define test_suite_version "0.1"
#define up printf("\033[A")
#define erase printf("\33[2K\r")

#define DOC_tests_run "used to run all tests\ntest name should start with `test_`"

// error codes
#define GENERATOR_FAULT_VALUE "GF1"
#define GENERATOR_FAULT_COUNT "GF2"

struct receiver_payload {
    char **data;
    uint data_cnt;
    bool passed;
    uint idx;
    string got_invalid;
};

bool test_suite_receiver(string repr, struct receiver_payload *payload) {
    if (! payload->data) {
        return true;
    }
    assert(repr[0] == '=');
    repr++;
    if (strcmp(payload->data[payload->idx], "..") == 0) {
        payload->idx++;
        return false;
    }
    if (payload->idx > payload->data_cnt - 1) {
        payload->passed = false;
        return false;
    } else if (strcmp(repr, payload->data[payload->idx]) != 0) {
        payload->passed = false;
        payload->got_invalid = strdup( repr );
        return false;
    } else {
        payload->idx++;
        return true;
    }
}

void headerprint(string s, uint maxnlen, uint colorcodes) {
    uint filler = 9 + maxnlen;
    uint len_s = laure_string_strlen(s) + 9 - (colorcodes * 6);
    if (filler < len_s)
        printf("--- %s ---\n", s);
    else {
        filler = (filler - len_s) / 2;
        printf("---");
        for (int j = 0; j < filler; j++) printf(" ");
        printf(" %s ", s);
        for (int j = 0; j < filler; j++) printf(" ");
        printf("---\n");
    }
}

qresp test_predicate_run(preddata *pd, control_ctx *cctx) {
    printf("\n(Running test suite v%s)\n", test_suite_version);
    printf("(Collecting tests)\n");

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
    uint tests_passed = 0;

    laure_session_t *sess = malloc(sizeof(laure_session_t));
    sess->stack = laure_stack_clone(cctx->stack->global, true);
    sess->stack->global = sess->stack;
    sess->_included_filepaths[0] = 0;
    sess->_doc_buffer = NULL;
    sess->signal = 0;

    laure_trace_init();
    void *old_sess = LAURE_SESSION;
    LAURE_SESSION = sess;

    string comments[256];
    memset(comments, 0, sizeof(void*) * 256);

    uint max_name_len = 0;

    for (int i = 0; i < len; i++) {
        Instance *predicate = tests[i];
        if (laure_string_strlen(predicate->name) > max_name_len) {
            max_name_len = laure_string_strlen(predicate->name);
        }
    }

    headerprint("Running tests", max_name_len, 0);

    clock_t t = clock();

    for (int i = 0; i < len; i++) {
        Instance *predicate = tests[i];
        struct PredicateImage *pred_im = (struct PredicateImage*)predicate->image;
        char spaces[28] = {0};
        for (int j = laure_string_strlen(predicate->name); j < max_name_len; j++) {
            strcat(spaces, " ");
            if (j >= 27) break;
        }
        printf("%s: %s%srunning%s\n", predicate->name, spaces, YELLOW_COLOR, NO_COLOR);

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
            char argn[64];
            snprintf(argn, 64, "%s_ARG%d", predicate->name, j);
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
        payload->got_invalid = NULL;
        ncctx->vpk->payload = payload;
        ncctx->vpk->mode = SENDER;

        LAURE_RECURSION_DEPTH = 0;
        LAURE_CLOCK = clock();
        qresp response = laure_eval(ncctx, expset);

        up;
        erase;

        if (! payload->passed && payload->got_invalid) {
            printf("%s: %s%sfailed (%s)%s\n", predicate->name, spaces, RED_COLOR, GENERATOR_FAULT_VALUE, NO_COLOR);
            char buff[256];
            snprintf(buff, 256, "[%s] incorrect value generated: got `%s%s%s` expected `%s%s%s`", GENERATOR_FAULT_VALUE, RED_COLOR, payload->got_invalid, NO_COLOR, GREEN_COLOR, payload->data[payload->idx], NO_COLOR);
            comments[i] = strdup( buff );
        } else if (payload->idx != payload->data_cnt) {
            printf("%s: %s%sfailed (%s)%s\n", predicate->name, spaces, RED_COLOR, GENERATOR_FAULT_COUNT, NO_COLOR);
            char buff[64];
            snprintf(buff, 64, "[%s] %d relations expected, %d generated", GENERATOR_FAULT_COUNT, payload->data_cnt, payload->idx);
            comments[i] = strdup( buff );
        } else if (payload->data_cnt > 0) {
            // all found
            printf("%s: %s%spassed%s\n", predicate->name, spaces, GREEN_COLOR, NO_COLOR);
            tests_passed++;
        } else {
            switch (response.state) {
                case q_true: {
                    printf("%s: %s%spassed%s\n", predicate->name, spaces, GREEN_COLOR, NO_COLOR);
                    tests_passed++;
                    break;
                }
                case q_false:
                case q_error: {
                    laure_trace_erase();
                    printf("%s: %s%sfailed%s\n", predicate->name, spaces, RED_COLOR, NO_COLOR);
                    comments[i] = response.error;
                    break;
                }
                default: {
                    printf("%s: %s%sunknown%s\n", predicate->name, spaces, GRAY_COLOR, NO_COLOR);
                    break;
                }
            }
        }
    }

    t = clock() - t;
    double elapsed = ((double)t) / CLOCKS_PER_SEC;

    laure_stack_free(sess->stack);
    free(sess);

    headerprint("Showing results", max_name_len, 0);

    for (int i = 0; i < len; i++) {
        if (comments[i]) {
            printf("%s: %s\n", tests[i]->name, comments[i]);
            free(comments[i]);
        }
    }

    double passed_percent = ((double)tests_passed / (double)len) * 100;

    printf("----------------------\n");
    printf("Succeed %d/%d (%s%d%%%s)\n", tests_passed, len, passed_percent >= 70 ? GREEN_COLOR : RED_COLOR, (int)round(passed_percent), NO_COLOR);
    printf("Time elapsed %fs\n", elapsed);
    printf("----------------------\n");

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