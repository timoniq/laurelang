#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"

#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <readline/readline.h>

#ifndef DOC_BUFFER_LEN
#define DOC_BUFFER_LEN 512
#endif

#define HEADERSZ 60

short int LAURE_ASK_IGNORE = 0;

void print_errhead(string str) {
    printf("  %s", RED_COLOR);
    printf("%s", str);
    printf("%s\n", NO_COLOR);
}

void print_header(string header, uint sz) {
    if (sz == 0) {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        sz = w.ws_col;
    }
    uint side = (sz - strlen(header)) / 2;
    char spaces[128];
    memset(spaces, ' ', side);
    spaces[side-1] = 0;
    printf("%s", RED_COLOR);
    printf("%s%s%s", spaces, header, spaces);
    printf("%s\n", NO_COLOR);
}

void ask_for_halt() {
    if (LAURE_ASK_IGNORE) return;
    string line = readline("Should consulting be continued (use yy to ignore asking further)? Y/yy/n: ");
    short int mode;
    if (strlen(line) == 0 || str_eq(line, "Y")) mode = 1;
    else if (str_eq(line, "yy")) mode = 2;
    else if (str_eq(line, "n")) exit(6);
    else {
        free(line);
        ask_for_halt();
        return;
    }
    free(line);
    if (mode) {
        if (mode == 2) LAURE_ASK_IGNORE = true;
        return;
    }
}

apply_result_t respond_apply(apply_status_t status, string e) {
    apply_result_t ar;
    ar.status = status;
    ar.error = e;
    return ar;
};

int laure_load_shared(laure_session_t *session, char *path) {
    void *lib = dlopen(path, RTLD_NOW);

    // Shared CLE (C logic environment) extension
    if (!lib) {
        print_errhead("failed to load shared CLE extension");
        string error = dlerror();
        if (strstr(error, "no such file")) {
            strcpy(error, "Shared object is undefined");
        }
        printf("    %s%s%s: \n        %s\n", BOLD_WHITE, path, NO_COLOR, error);
        ask_for_halt();
        return false;
    }

    uint *dfn = dlsym(lib, "DFLAG_N");
    *dfn = DFLAG_N;

    void *dfs = dlsym(lib, "DFLAGS");
    memcpy(dfs, DFLAGS, DFLAG_MAX * 32 * 2);

    ulong **link_id = dlsym(lib, "LAURE_LINK_ID");
    *link_id = LAURE_LINK_ID;

    void (*init_nb)() = dlsym(lib, "laure_init_name_buffs");
    init_nb();

    void (*set_transl)() = dlsym(lib, "laure_set_translators");
    set_transl();
    
    int (*perform_upload)(laure_session_t*) = dlsym(lib, "on_include");
    if (!perform_upload) {
        print_errhead("invalid shared cle; cannot load");
        printf("    function on_include(laure_session_t*) is undefined in CLE extension %s\n", path);
        ask_for_halt();
        return false;
    }
    int response = perform_upload(session);
    if (response != 0) {
        print_errhead("shared cle on_include returned non-zero code");
        printf("cle extension upload resulted with non-zero code (%d), failure", response);
        ask_for_halt();
        return false;
    }
    return true;
}

void strrev_via_swap(string s) {
    int l = strlen(s);

    for (int i = 0; i < l / 2; i++) {
        char b = s[i];
        s[i] = s[l-i-1];
        s[l-i-1] = b;
    }
}

string get_work_dir_path(string f_addr) {
    string naddr = strdup(f_addr);
    strrev_via_swap(naddr);
    while (naddr[0] != '/') naddr++;
    strrev_via_swap(naddr);
    return naddr;
}

string get_nested_ins_name(Instance *atom, uint nesting, laure_scope_t *scope) {
    assert(atom != NULL);
    char buff[64];
    strcpy(buff, atom->name);
    for (int i = 0; i < nesting; i++) {
        strcat(buff, "[]");
    }
    if (!laure_scope_find_by_key(scope->glob, buff, true)) {
        Instance *ins = atom;
        
        /* == nestings ==
        while (nesting) {
            void *img = array_u_new(ins);
            ins = instance_new(strdup("el"), NULL, img);
            ins->repr = array_repr;
            nesting--;
        }

        ins->name = strdup(buff);
        ins->repr = array_repr;
        */

        // create constant nested type
        // which will be used later for hinting
        laure_scope_insert(scope->glob, ins);
    }
    return strdup(buff);
}

void *laure_apply_pred(laure_expression_t *predicate_exp, laure_scope_t *scope) {
    struct InstanceSet *args_set = instance_set_new();
    uint all_count = laure_expression_get_count(predicate_exp->ba->set);
    if (predicate_exp->ba->has_resp) all_count--;

    uint idx = 0;
    uint *nestings = malloc(sizeof(void*) * (all_count - predicate_exp->ba->body_len));

    for (int i = predicate_exp->ba->body_len; i < all_count; i++) {
        laure_expression_t *aexp = laure_expression_set_get_by_idx(predicate_exp->ba->set, i);
        uint nesting = aexp->flag;
        string tname;
        if (nesting) {
            Instance *to_nest = laure_scope_find_by_key(scope, aexp->s, true);
            if (!to_nest) {
                return NULL;
            }
            tname = get_nested_ins_name(to_nest, nesting, scope);
        } else {
            tname = aexp->s;
        }
        Instance *arg = laure_scope_find_by_key(scope, tname, true);
        if (! arg) {
            return NULL;
        }
        instance_set_push(args_set, arg);
        nestings[idx] = nesting;
        idx++;
    }

    Instance *resp;
    uint resp_nesting = 0;
    if (predicate_exp->ba->has_resp) {
        laure_expression_t *rexp = laure_expression_set_get_by_idx(predicate_exp->ba->set, idx);
        string tname;
        resp_nesting = rexp->flag;
        if (resp_nesting) {
            Instance *to_nest = laure_scope_find_by_key(scope, rexp->s, true);
            if (!to_nest) {
                return NULL;
            }
            tname = get_nested_ins_name(to_nest, resp_nesting, scope);
        } else tname = rexp->s;
        resp = laure_scope_find_by_key(scope, tname, true);
        if (! resp) {
            return NULL;
        }
    } else {
        resp = NULL;
    }

    struct PredicateImage *img = predicate_header_new(args_set, resp, predicate_exp->t == let_constraint);
    img->header.nestings = nestings;
    img->header.response_nesting = resp_nesting;
    return img;
}

apply_result_t laure_consult_predicate(
    laure_session_t *session, 
    laure_scope_t *scope, 
    laure_expression_t *predicate_exp, 
    string address
) {
    assert(predicate_exp->t == let_pred || predicate_exp->t == let_constraint);
    Instance *pred_ins = laure_scope_find_by_key(scope, predicate_exp->s, true);

    bool is_header = (pred_ins == NULL && predicate_exp->is_header);

    if (is_header) {
        // headers may be include/cle_include functions
        if (
            str_eq(predicate_exp->s, "include") 
            || str_eq(predicate_exp->s, "cle_include")
        ) {
            for (int i = 0; i < laure_expression_get_count(predicate_exp->ba->set); i++) {
                laure_expression_t *arg = laure_expression_set_get_by_idx(predicate_exp->ba->set, i);
                if (arg->s[0] == '"') arg->s++;
                if (lastc(arg->s) == '"') lastc(arg->s) = 0;

                string n;

                if (str_starts(arg->s, "/")) {
                    n = strdup(arg->s);
                } else if (str_starts(arg->s, "@/")) {
                    arg->s++;
                    n = malloc( strlen(lib_path) + strlen(arg->s) + 1 );
                    strcpy(n, lib_path);
                    strcat(n, arg->s);
                } else {
                    string wdir = get_work_dir_path(address);
                    n = malloc( strlen(wdir) + strlen(arg->s) + 1 );
                    strcpy(n, wdir);
                    strcat(n, arg->s);
                }

                string path_ = malloc(PATH_MAX);
                memset(path_, 0, PATH_MAX);
                realpath(n, path_);

                if (str_eq(predicate_exp->s, "cle_include")) {
                    bool result = laure_load_shared(session, path_);
                    free(path_);
                    return respond_apply((apply_status_t)result, NULL);
                } else {
                    FILE *f = fopen(path_, "r");
                    if (!f) {
                        free(path_);
                        return respond_apply(apply_error, "failed to find file");
                    }
                    fclose(f);
                    laure_consult_recursive(session, path_);
                }
                free(path_);
            }
            return respond_apply(apply_ok, NULL);
        }

        if (is_header && predicate_exp->ba->body_len > 0) {
            return respond_apply(apply_error, "header should not contain body");
        }

        Instance *maybe_header = pred_ins;

        void *img = laure_apply_pred(predicate_exp, scope);

        if (! img) return respond_apply(apply_error, "predicate hint is undefined");

        string name = strdup(predicate_exp->s);
        Instance *ins = instance_new(name, LAURE_DOC_BUFF, img);
        ins->repr = predicate_exp->t == let_pred ? predicate_repr : constraint_repr;

        laure_scope_insert(scope, ins);

        LAURE_DOC_BUFF = NULL;
        return respond_apply(apply_ok, NULL);
    } else {
        if (! pred_ins)
            return respond_apply(apply_error, "header for predicate is undefined");
        predicate_addvar(pred_ins->image, predicate_exp);
        return respond_apply(apply_ok, NULL);
    }
}

apply_result_t laure_apply(laure_session_t *session, string fact) {

    if (strlen(fact) > 1 && str_starts(fact, "//")) {
        // Saving comment
        fact = fact + 2;
        if (fact[0] == ' ') fact++;
        if (LAURE_DOC_BUFF == NULL) {
            LAURE_DOC_BUFF = strdup(fact);
        } else {
            char buffer[512];
            memset(buffer, 0, 512);
            int free_space = 512 - strlen(LAURE_DOC_BUFF) - strlen(fact) - 3;
            if (free_space < 0) {
                apply_result_t apply_res;
                apply_res.error = "doc buff overflow";
                apply_res.status = apply_error;
                return apply_res;
            }
            strcpy(buffer, LAURE_DOC_BUFF);
            strcat(buffer, "\n");
            strcat(buffer, fact);
            free(LAURE_DOC_BUFF);
            LAURE_DOC_BUFF = strdup(buffer);
        }
        return respond_apply(apply_ok, NULL);
    }
    
    laure_parse_result result = laure_parse(fact);
    if (!result.is_ok) {
        return respond_apply(apply_error, result.err);
    }

    laure_expression_t *exp = result.exp;
    laure_expression_set *expset = laure_expression_compose_one(exp);
    qcontext *qctx = qcontext_new(expset);
    control_ctx *cctx = control_new(session->scope, qctx, NULL, NULL, true);
    cctx->silent = true;

    qresp response = laure_start(cctx, expset);
    if (response.error) {
        return respond_apply(apply_error, response.error);
    }
    return respond_apply(apply_ok, NULL);
}

int laure_init_structures(laure_session_t *session) {
}

string consult_single(laure_session_t *session, string fname, FILE *file) {
    void **ifp = session->_included_filepaths;

    while (ifp[0]) {
        string fp = *ifp;
        if (str_eq(fp, fname)) {
            printf("  %sFile%s %s%s was already consulted, skipping%s\n", RED_COLOR, BOLD_WHITE, fname, RED_COLOR, NO_COLOR);
            return NULL;
        }
        ifp++;
    }

    *ifp = strdup(fname);

    assert(fname && file);
    int ln = 0;

    struct stat *sb = malloc(sizeof(struct stat));
    memset(sb, 0, sizeof(struct stat));

    if (fstat(fileno(file), sb) == 0 && S_ISDIR(sb->st_mode)) {

        free(sb);

        string rev = malloc(strlen(fname) + 1);
        memset(rev, 0, strlen(fname) + 1);
        strcpy(rev, fname);
        
        strrev_via_swap(rev);

        string spl = strtok(rev, "/");
        
        string s = malloc(strlen(spl) + 1);
        memset(s, 0, strlen(spl) + 1);
        strcpy(s, spl);

        strrev_via_swap(s);

        string p = malloc(strlen(fname) + strlen(s) + 6);
        memset(p, 0, strlen(fname) + strlen(s) + 6);
        strcpy(p, fname);
        strcat(p, "/");
        strcat(p, s);
        strcat(p, ".le");

        if (access(p, F_OK) != 0) {
            printf("%sUnable to find init file %s for package.\n", RED_COLOR, p);
            printf("Package %s is skipped.%s\n", fname, NO_COLOR);
            return NULL;
        }
        
        fclose(file);
        return p;
    }

    char line[512];
    memset(line, 0, 512);

    char buff[512];
    memset(buff, 0, 512);

    ssize_t read = 0;
    size_t len = 0;

    string readinto = malloc(512);
    memset(readinto, 0, 512);

    while ((read = getline(&readinto, &len, file)) != -1) {
        strcpy(line, readinto);
        uint idx = 0;
        while (line[idx] == ' ') idx++;
        if (idx > 0 && str_starts(line + idx, "//")) continue;

        if (!strlen(line)) continue;
        if (lastc(line) == '\n')
            lastc(line) = 0;
        
        char last = lastc(line);

        if (buff[0]) {
            char newln[512];
            memset(newln, 0, 512);
            strcpy(newln, buff);
            strcat(newln, line);
            strcpy(line, newln);
            strcpy(buff, line);
        }

        if (str_starts(line, "//")) {
            laure_apply(session, line);
            continue;
        }

        if (last != '}' && last != '.') {
            strcpy(buff, line);
        } else {
            if (lastc(line) == '.') lastc(line) = 0;
            string line_ = strdup(line);
            LAURE_CURRENT_ADDRESS = fname;
            apply_result_t result = laure_apply(session, line_);
            if (result.error) {
                ;
                // printf("apply error while consulting: %s\n", result.error);
            }
            buff[0] = 0;
        }
    }

    free(readinto);
    fclose(file);
    laure_init_structures(session);
    return NULL;
}

void laure_consult_recursive(laure_session_t *session, string path) {
    string next = path;
    while (next) {
        next =  consult_single(session, next, fopen(next, "r"));
    }
}