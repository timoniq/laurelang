#include "laurelang.h"

#define SPLIT_CHAR ':'
#define MAX_STATEMENT_MOD_C 32
#define STD_MODULE "std"

// source :: src/parser.c
string read_til(const string s, int c);
string string_clean(string s);

laure_import_mod_ll *laure_parse_import(string import_str) {
    if (! laure_string_strlen(import_str)) return NULL;

    string rem_string = import_str;
    size_t import_str_len = laure_string_strlen(import_str);
    size_t rem_len = import_str_len;

    laure_import_mod_ll *first = NULL;
    laure_import_mod_ll *last = NULL;

    int last_was_set = false;
    
    while (laure_string_strlen(rem_string)) {
        if (last_was_set)
            break;
        
        string mod_str = read_til(rem_string, SPLIT_CHAR);
        if (! mod_str) {
            mod_str = rem_string;
            rem_string = NULL;
        } else {
            rem_string += strlen(mod_str) + 1;
        }

        rem_len = rem_string ? laure_string_strlen(rem_string) : 0;

        laure_import_mod_ll *mod = laure_alloc(sizeof(laure_import_mod_ll));
        mod->mod = mod_str;
        mod->cnext = 0;
        mod->nexts = NULL;

        if (rem_string && str_starts(rem_string, "{")) {
            laure_import_mod_ll **mod_ptrs = laure_alloc(sizeof(void*) * MAX_STATEMENT_MOD_C);
            memset(mod_ptrs, 0, sizeof(void*) * MAX_STATEMENT_MOD_C);
            int modc = 0;

            string submods = read_til(rem_string + 1, '}');
            string submod = read_til(submods, ',');
            string rem_submods;

            if (submod) {
                rem_submods = submods + strlen(submod) + 1;
            } else {
                submod = submods;
                rem_submods = NULL;
            }

            while (true) {

                if (laure_string_strlen(submod)) {

                    laure_import_mod_ll *submod_ll = laure_parse_import(
                        strdup(string_clean(submod))
                    );
                    mod_ptrs[modc++] = submod_ll;
                } else {
                    mod_ptrs[modc++] = NULL;
                }
                
                if (! rem_submods) {
                    break;
                }

                laure_free(submod);

                submod = read_til(rem_submods, ',');

                if (submod) {
                    rem_submods += strlen(submod) + 1;
                } else {
                    submod = rem_submods;
                    rem_submods = NULL;
                }
            };

            laure_free(submods);

            if (! modc) {
                laure_free(mod_ptrs);
                mod_ptrs = NULL;
            }
            mod->cnext = modc;
            mod->nexts = mod_ptrs;

            last_was_set = true;
        }

        if (! first) {
            first = mod;
        }
        if (last) {
            last->cnext = 1;
            last->nexts = laure_alloc(sizeof(void*));
            last->nexts[0] = mod;
        }
        last = mod;
    }
    return first;
}

#define MAX_PATH 1024

// source :: src/query.c
string get_work_dir_path(string f_addr);

// source :: src/apply.c
string search_path(string original_path);

string laure_import_get_use_path(
    string current_path,
    laure_import_mod_ll *mod
) {
    if (str_eq(mod->mod, ".")) {
        return strdup(current_path);
    } else {
        char buff[MAX_PATH];
        snprintf(buff, MAX_PATH, "%s/%s", current_path, mod->mod);
        return strdup(buff);
    }
}

// imports all inner instances of module into scope
int laure_import_use_mod(
    laure_session_t *session,
    laure_import_mod_ll *mod,
    string path_or_null
) {
    assert(mod && mod->mod);

    if (! mod->cnext && path_or_null) {
        // terminal import mod part
        string path = laure_import_get_use_path(path_or_null, mod);
        string p = search_path(path);
        free(path);
        if (! p)
            return 1;
        int failed[1] = {0};
        laure_consult_recursive(session, p, failed);
        free(p);
        return *failed;
    } else if (! mod->cnext) {
        // import whole module
        if (str_eq(mod->mod, STD_MODULE)) {
            return 0;
        }
        string wdir = get_work_dir_path(LAURE_CURRENT_ADDRESS ? LAURE_CURRENT_ADDRESS : "./");
        string path = laure_import_get_use_path(wdir, mod);
        string buffd = search_path(path);
        free(path);
        if (! buffd)
            return 1;
        int failed[1] = {0};
        laure_consult_recursive(session, buffd, failed);
        free(buffd);
        return *failed;
    } else {
        string path;
        if (path_or_null) {
            path = strdup(path_or_null);
        } else {
            if (str_eq(mod->mod, STD_MODULE))
                path = strdup(lib_path);
            else {
                if (str_eq(mod->mod, "") && LAURE_CURRENT_ADDRESS) {
                    path = get_work_dir_path(LAURE_CURRENT_ADDRESS);
                } else {
                    path = strdup(mod->mod);
                }
            }
        }

        if (path_or_null) {
            char buff[MAX_PATH];
            snprintf(buff, MAX_PATH, "%s/%s", path, mod->mod);
            free(path);
            path = strdup(buff);
        }

        for (int i = 0; i < mod->cnext; i++) {
            laure_import_mod_ll *submod = mod->nexts[i];
            int r = laure_import_use_mod(session, submod, path);
            if (r != 0) {
                printf("failed to consult at %s\n", path);
                return r;
            }
        }
        free(path);
        return 0;
    }
    return 0;
}