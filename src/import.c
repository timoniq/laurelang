#include "laurelang.h"

#define SPLIT_CHAR ':'
#define MAX_STATEMENT_MOD_C 32

// source :: std/parser.c
string read_til(const string s, int c);

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

                string sm_clean = submod;
                while (*sm_clean == ' ') sm_clean++;

                laure_import_mod_ll *submod_ll = laure_parse_import(
                    strdup(sm_clean)
                );
                mod_ptrs[modc++] = submod_ll;
                
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