#include <laurelang.h>
#include <predpub.h>
#include <laureimage.h>
#include <math.h>
#include <time.h>

#define test_suite_version "0.1"
#define up printf("\033[A")
#define erase printf("\33[2K\r")

#define DOC_tests_run "used to run all tests\ntest name should start with `test_`"

// error codes
#define GENERATOR_FAULT_VALUE "Value"
#define GENERATOR_FAULT_COUNT "Count"

#define SKIP_CHAR ';'

struct receiver_payload {
    char **data;
    uint data_cnt;
    bool passed;
    uint idx;
    string got_invalid;
};

typedef enum {
    full,
    onlyresults,
    nologs,
} test_suite_mode;

#define result_n(q) \
        (q.state == q_true) ? "true" \
        : (q.state == q_false) ? "false" \
        : (q.state == q_error) ? "error" \
        : (q.state == q_yield) ? ((q.payload == (void*)1) ? "true" : "false") \
        : (q.state == q_continue) ? "true, continue" \
        : (q.state == q_stop) ? "true, stop" \
        : "?"

bool test_suite_receiver(string repr, struct receiver_payload *payload) {
    if (! payload->data) {
        return true;
    }
    assert(repr[0] == '=');
    repr++;
    if (payload->idx > payload->data_cnt - 1) {
        payload->passed = false;
        return false;
    } else if (strcmp(payload->data[payload->idx], "...") == 0) {
        payload->idx++;
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

bool should_skip(string_linked *skips, string s) {
    if (! skips) return false;
    do {
        if (laure_string_pattern_match(s, skips->s)) return true;
        skips = skips->next;
    } while (skips);
    return false;
}

size_t filler_size(uint maxnlen) {
    return maxnlen + 13;
}

void headerprint(string s, uint maxnlen, uint colorcodes, int isstart) {
    uint filler = 9 + maxnlen + 4;
    uint len_s = laure_string_strlen(s) + 9 - (colorcodes * 6);
    char *l, *r;
    if (isstart) {
        l = "┌──";
        r = "──┐";
    } else {
        l = "└──";
        r = "──┘";
    }
    if (filler < len_s)
        printf("%s %s %s\n", l, s, r);
    else {
        filler = (filler - len_s) / 2;
        printf("%s", l);
        for (int j = 0; j < filler; j++) printf(" ");
        printf(" %s ", s);
        for (int j = 0; j < filler; j++) printf(" ");
        if ((filler - len_s) % 2) printf(" ");
        printf("%s\n", r);
    }
}

#define print_in_block(maxnlen, ...) do { \
        printf("│ "); char buff[64]; \
        snprintf(buff, 64, __VA_ARGS__); \
        size_t __l = strlen(buff) + 4; \
        printf("%s", buff); \
        size_t __fsz = filler_size(maxnlen); if (__l > __fsz) {__fsz = 0;} else {__fsz -= __l;} \
        for (size_t __j = 0; __j < __fsz; __j++) printf(" "); \
        printf("│\n"); \
        } while (0)

qresp test_predicate_run(preddata *pd, control_ctx *cctx) {

    test_suite_mode mode = full;
    string modestr = get_dflag("mode");
    if (modestr) {
        switch (modestr[0]) {
            case 'f': case 'F': case 0: {
                mode = full;
                break;
            }
            case 'r': case 'R': case 1: {
                mode = onlyresults;
                break;
            }
            case 'n': case 'N': case 2: {
                mode = nologs;
                break;
            }
            default: {
                printf("test suite mode `%s` not recognized\n", modestr);
                break;
            }
        }
    }

    string stopkwd = get_dflag("stopkwd");
    string alloc_stats = get_dflag("alloc_stats");

    if (mode == full) {
        printf("\n(Running test suite v%s)\n", test_suite_version);
        printf("(Collecting tests)\n");
    }

    Instance *tests[256];
    memset(tests, 0, sizeof(void*) * 256);
    int len = 0;

    laure_scope_iter(cctx->scope->glob, cellptr, {
        Instance *ins = cellptr->ptr;
        if (str_starts(ins->name, "test_")) {
            if (len >= 255) {
                printf("too many tests. max 256 per suite\n");
                break;
            }
            laure_image_head h = read_head(ins->image);
            if (h.t == PREDICATE_FACT || h.t == CONSTRAINT_FACT) {
                tests[len] = ins;
                len++;
            }
        }
    });

    if (!len) {
        if (mode == full)
            printf("Collected 0 tests. Terminating.\n");
        return respond(q_true, NULL);
    }

    if (mode == full) printf("Collected %d tests.\n", len);
    uint tests_passed = 0;

    laure_session_t *sess = laure_alloc(sizeof(laure_session_t));

    sess->scope = cctx->scope->glob;
    sess->scope->glob = sess->scope;
    memset(sess->_included_filepaths, 0, included_fp_max * sizeof(void*));

    string comments[256];
    memset(comments, 0, sizeof(void*) * 256);

    uint max_name_len = 0;

    for (int i = 0; i < len; i++) {
        Instance *predicate = tests[i];
        if (laure_string_strlen(predicate->name) > max_name_len) {
            max_name_len = laure_string_strlen(predicate->name);
        }
    }

    if (mode == full) headerprint("Running tests", max_name_len, 0, 1);

    clock_t t = clock();

    string skip_str = get_dflag("skip");
    string_linked *skips = skip_str ? string_split(skip_str, SKIP_CHAR) : NULL;

    uint checked_len = len;
    uint skipped = 0;

    if (alloc_stats) {
        laure_allocator_reset_stats();
    }

    for (int i = 0; i < len; i++) {
        printf("│ ");
        Instance *predicate = tests[i];
        struct PredicateImage *pred_im = (struct PredicateImage*)predicate->image;
        char spaces[128] = {0};
        for (int j = laure_string_strlen(predicate->name); j < max_name_len; j++) {
            strcat(spaces, " ");
            if (j >= 127) break;
        }

        if (should_skip(skips, predicate->name)) {
            if (mode == full) printf("%s: %s%somited%s │\n", predicate->name, spaces, YELLOW_COLOR, NO_COLOR);
            checked_len--; skipped++;
            continue;
        }
        if (mode == full) printf("%s: %s%srunned%s │\n", predicate->name, spaces, YELLOW_COLOR, NO_COLOR);

        int argc = 0;
        char **data = 0;
        if (predicate->doc && pred_im->header.args->length > 0) {
            argc = 1;
            for (int j = 0; j < strlen(predicate->doc); j++) {
                if (predicate->doc[j] == '\n') argc++;
            }
            data = laure_alloc(sizeof(void*) * argc);
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
        for (int j = 0; j < pred_im->header.args->length; j++) {
            char argn[64];
            snprintf(argn, 64, "%s_ARG%d", predicate->name, j);
            strcat(query, argn);
            if (j != pred_im->header.args->length - 1) strcat(query, ",");
        }
        strcat(query, ")");
        
        laure_parse_result res = laure_parse(query);
        if (!res.is_ok) {
            return respond(q_error, res.err);
        }

        laure_expression_set *expset = laure_expression_compose_one(res.exp);

        qcontext nqctx[1];
        *nqctx = qcontext_temp(NULL, expset, NULL);

        control_ctx ncctx[1];
        ncctx->data = NULL;
        ncctx->cut = false;
        ncctx->qctx = nqctx;
        ncctx->scope = cctx->scope;
        ncctx->silent = false;
        ncctx->tmp_answer_scope = laure_scope_new(cctx->scope->glob, cctx->scope);
        ncctx->tmp_answer_scope->next = NULL;
        ncctx->vpk = laure_vpk_create(expset);
        ncctx->this_break = false;
        ncctx->vpk->sender_receiver = test_suite_receiver;

        struct receiver_payload *payload = laure_alloc(sizeof(struct receiver_payload));
        payload->idx = 0;
        payload->passed = true;
        payload->data = data;
        payload->data_cnt = argc;
        payload->got_invalid = NULL;

        ncctx->vpk->payload = payload;
        ncctx->vpk->mode = SENDER_REPRS;

        LAURE_CLOCK = clock();
        qresp response = laure_start(ncctx, expset);

        if (alloc_stats) {
            print_in_block(max_name_len, "");
            print_in_block(max_name_len, "%s allocs", predicate->name);
            laure_allocator_stats stats = laure_allocator_stats_get();
            print_in_block(max_name_len, "  alloc   %zu", stats.allocated);
            print_in_block(max_name_len, "  realloc %zu", stats.reallocated);
            print_in_block(max_name_len, "  free    %zu", stats.freed);
            print_in_block(max_name_len, "");
            printf("\n");
            laure_allocator_reset_stats();
        }

        if (mode == full) {
            up;
            erase;
        }

        printf("│ ");

        if (! payload->passed && payload->got_invalid) {
            if (mode == full) printf("%s: %s%sfailed%s │\n", predicate->name, spaces, RED_COLOR, NO_COLOR);
            char buff[256];
            snprintf(buff, 256, "  %sargument idx = %d: \n        got %s%s%s \n        exp %s%s%s", NO_COLOR, payload->idx + 1, RED_COLOR, payload->got_invalid, NO_COLOR, YELLOW_COLOR, payload->data[payload->idx], NO_COLOR);
            comments[i] = strdup( buff );
        } else if (payload->idx != payload->data_cnt) {
            if (mode == full) printf("%s: %s%sfailed%s │\n", predicate->name, spaces, RED_COLOR, NO_COLOR);
            char buff[64];
            snprintf(buff, 64, "  %s%d relations expected, %d generated", NO_COLOR, payload->data_cnt, payload->idx);
            comments[i] = strdup( buff );
        } else if (payload->data_cnt > 0) {
            // all found
            if (mode == full) printf("%s: %s%spassed%s │\n", predicate->name, spaces, GREEN_COLOR, NO_COLOR);
            tests_passed++;
        } else {
            switch (response.state) {
                case q_yield: {
                    if (response.payload == (char*)0x1) {
                        if (mode == full)
                        printf("%s: %s%spassed%s │\n", predicate->name, spaces, GREEN_COLOR, NO_COLOR);
                        tests_passed++;
                    } else {
                        printf("%s: %s%sfailed%s │\n", predicate->name, spaces, RED_COLOR, NO_COLOR);
                    }
                    break;
                }
                case q_error: {
                    if (mode == full)
                    printf("%s: %s%sfailed%s │\n", predicate->name, spaces, RED_COLOR, NO_COLOR);
                    if (! LAURE_ACTIVE_ERROR) {
                        comments[i] = strdup(response.payload);
                    } else {
                        char buff[256];
                        laure_error_write(LAURE_ACTIVE_ERROR, buff, 256);
                        comments[i] = strdup(buff);
                    }
                    break;
                }
                default: {
                    if (mode == full) 
                    printf("%s: %s%sunknown%s │\n", predicate->name, spaces, GRAY_COLOR, NO_COLOR);
                    break;
                }
            }
        }
        laure_free(payload);
        laure_scope_free(ncctx->tmp_answer_scope);
        if (stopkwd && (strstr(predicate->name + 5, stopkwd) || strcmp(stopkwd, ".") == 0)) {
            printf("%sCaught keyword. Press any key to continue%s\n", LAURUS_NOBILIS, NO_COLOR);
            getchar();
            up; erase;
            up; erase;
        }
    }

    len = checked_len;

    t = clock() - t;
    double elapsed = ((double)t) / CLOCKS_PER_SEC;

    laure_free(sess);

    if (mode != nologs) {
        headerprint("Done. Showing results", max_name_len, 0, 0);
        if (skipped) printf("Skipped %d tests.\n", skipped);

        for (int i = 0; i < len; i++) {
            if (comments[i]) {
                printf("\n%s%s%s: \n%s%s%s\n", BOLD_WHITE, tests[i]->name, NO_COLOR, RED_COLOR, comments[i], NO_COLOR);
                laure_free(comments[i]);
                if (i == len - 1) printf("\n");
            }
        }

        double passed_percent = ((double)tests_passed / (double)len) * 100;

        printf("━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("Succeed %d/%d (%s%d%%%s)\n", tests_passed, len, passed_percent >= 70 ? GREEN_COLOR : RED_COLOR, (int)round(passed_percent), NO_COLOR);
        printf("Time elapsed %fs\n", elapsed);
        printf("━━━━━━━━━━━━━━━━━━━━━━\n");
    }

    return respond(tests_passed == len ? q_true : q_false, NULL);
}

int on_use(laure_session_t *session) {
    laure_api_add_predicate(
        session, "tests_run", 
        test_predicate_run, 
        0, NULL, NULL, false, 
        DOC_tests_run
    );
    return 0;
}
