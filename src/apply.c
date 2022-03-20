#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"

#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef DOC_BUFFER_LEN
#define DOC_BUFFER_LEN 512
#endif

apply_result_t respond_apply(apply_status_t status, string e) {
    apply_result_t ar;
    ar.status = status;
    ar.error = e;
    return ar;
};

bool laure_load_shared(laure_session_t *session, char *path) {
    void *lib = dlopen(path, RTLD_NOW);

    // Shared CLE (C logic environment) extension
    if (!lib) {
        printf("failed to load shared CLE extension %s\n\\ %s\n", path, dlerror());
        return false;
    }

    uint *dfn = dlsym(lib, "DFLAG_N");
    *dfn = DFLAG_N;

    void *dfs = dlsym(lib, "DFLAGS");
    memcpy(dfs, DFLAGS, DFLAG_MAX * 32 * 2);
    
    int (*perform_upload)(laure_session_t*) = dlsym(lib, "package_include");
    if (!perform_upload) {
        printf("function cle_perform_upload(laure_session_t*) is undefined in CLE extension %s\n", path);
        return false;
    }
    int response = perform_upload(session);
    if (response != 0) {
        printf("cle extension upload resulted with non-zero code, failure");
        return false;
    }
    return true;
}

void strrev_via_swap(string s) {
    int l = strlen(s);
    int mid;

    if (l % 2 == 0) {
        mid = l / 2;
    } else {
        mid = (l - 1) / 2;
    }
    l--;

    for (int i = 0; i < mid; i++) {
        char b = s[i];
        s[i] = s[l-i];
        s[l-i] = b;
    }
}

string get_work_dir_path(string f_addr) {
    string naddr = strdup(f_addr);
    strrev_via_swap(naddr);
    while (naddr[0] != '/') naddr++;
    strrev_via_swap(naddr);
    return naddr;
}

string get_nested_ins_name(Instance *atom, uint nesting, laure_stack_t *stack) {
    assert(atom != NULL);
    char buff[64];
    strcpy(buff, atom->name);
    for (int i = 0; i < nesting; i++) {
        strcat(buff, "[]");
    }
    if (!laure_stack_get(stack, buff)) {
        Instance *ins = atom;
        
        while (nesting) {
            void *img = array_u_new(ins);
            ins = instance_new(strdup("el"), NULL, img);
            ins->repr = array_repr;
            nesting--;
        }

        ins->name = strdup(buff);
        ins->repr = array_repr;

        // create constant nested type
        Cell cell;
        cell.instance = ins;
        cell.link_id = laure_stack_get_uid(stack);
        laure_stack_insert(stack, cell);
    }
    return strdup(buff);
}

void *laure_apply_pred(laure_expression_t *predicate_exp, laure_stack_t *stack) {
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
            Instance *to_nest = laure_stack_get(stack, aexp->s);
            if (!to_nest) {
                return NULL;
            }
            tname = get_nested_ins_name(to_nest, nesting, stack);
        } else {
            tname = aexp->s;
        }
        Instance *arg = laure_stack_get(stack, tname);
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
            Instance *to_nest = laure_stack_get(stack, rexp->s);
            if (!to_nest) {
                return NULL;
            }
            tname = get_nested_ins_name(to_nest, resp_nesting, stack);
        } else tname = rexp->s;
        resp = laure_stack_get(stack, tname);
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

apply_result_t laure_consult_predicate(laure_session_t *session, laure_stack_t *stack, laure_expression_t *predicate_exp, string address) {
    assert(predicate_exp->t == let_pred || predicate_exp->t == let_constraint);
    Instance *pred_ins = laure_stack_get(stack, predicate_exp->s);

    bool is_header = (pred_ins == NULL && predicate_exp->is_header);

    if (is_header) {
        // Header predicates may have some meta-funtions
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

        void *img = laure_apply_pred(predicate_exp, stack);

        if (! img) return respond_apply(apply_error, "predicate hint is undefined");

        string name = strdup(predicate_exp->s);
        Instance *ins = instance_new(name, session->_doc_buffer, img);
        ins->repr = predicate_exp->t == let_pred ? predicate_repr : constraint_repr;

        Cell cell;
        cell.instance = ins;
        cell.link_id = laure_stack_get_uid(stack);
        laure_stack_insert(stack, cell);

        session->_doc_buffer = NULL;
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
        if (session->_doc_buffer == NULL) {
            session->_doc_buffer = strdup(fact);
        } else {
            char buffer[512];
            memset(buffer, 0, 512);
            int free_space = 512 - strlen(session->_doc_buffer) - strlen(fact) - 3;
            if (free_space < 0) {
                apply_result_t apply_res;
                apply_res.error = "doc buff overflow";
                apply_res.status = apply_error;
                return apply_res;
            }
            strcpy(buffer, session->_doc_buffer);
            strcat(buffer, "\n");
            strcat(buffer, fact);
            free(session->_doc_buffer);
            session->_doc_buffer = strdup(buffer);
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
    control_ctx *cctx = control_new(session->stack, qctx, NULL, NULL, true);
    cctx->silent = true;

    qresp response = laure_eval(cctx, expset);
    if (response.error) {
        return respond_apply(apply_error, response.error);
    }
    return respond_apply(apply_ok, NULL);
}

int laure_init_structures(laure_session_t *session) {
    #ifndef FEATURE_LINKED_SCOPE
    return 0;
    #else
    int count = 0;

    Cell cell;
    STACK_ITER(session->stack, cell, {
        if (read_head(cell.instance->image).t == STRUCTURE) {
            struct StructureImage *structure = (struct StructureImage*)cell.instance->image;
            if (!structure->is_initted) {
                // laure_structure_init(session->heap, session->stack, structure);
                count++;
            }
        }
    }, false);
    return count;
    #endif
}

string consult_single(laure_session_t *session, string fname, FILE *file) {
    void **ifp = session->_included_filepaths;

    while (ifp[0]) {
        string fp = *ifp;
        if (str_eq(fp, fname)) {
            printf("\\ %sfile%s %s was already consulted%s\n", RED_COLOR, NO_COLOR, fname, RED_COLOR, NO_COLOR);
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

        if (last != '}' && last != '.' && !str_starts(line, "//")) {
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
