#ifndef LLPARSER_H
#define LLPARSER_H

#include "laurelang.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define ERR_MAX_LEN 100

#define error_result(err_) do {laure_parse_result lpr; lpr.is_ok = false; lpr.err = err_; return lpr;} while (0)
#define error_format(...) do {char _errmsg[ERR_MAX_LEN]; snprintf(_errmsg, ERR_MAX_LEN, __VA_ARGS__); error_result(strdup(_errmsg));} while (0)
#define lineinfo(str) do {char info[ERR_MAX_LEN]; snprintf(info, ERR_MAX_LEN, "%s:%d", __FILE__, __LINE__); str = strdup(info);} while (0)
#define printindent(indent) for (int __i = 0; __i < indent; __i++) printf(" ");

string DIGITS = "0123456789";
string RESTRICTED = "[](). ";
string NOTSUPER = "-+:><=";
string ELLIPSIS = NULL;

#ifndef LAURE_SYNTAX_INFIX_PREPOSITION
#define LAURE_SYNTAX_INFIX_PREPOSITION "of"
#endif

#define NS_SEPARATOR ':'
#define GENERIC_OPEN '<'
#define GENERIC_CLOSE '>'

char* EXPT_NAMES[] = {"Expression Set", "Variable", "Predicate Call", "Declaration", "Assertion", "Imaging", "Predicate Declaration", "Choice (Packed)", "Choice (Unpacked)", "Naming", "Value", "Constraint", "Structure Definition", "Structure", "Array", "Unify", "Quantified Expression", "Domain", "Implication", "Reference", "Cut", "Atom", "Command", "Generic DT/Char", "Atom Sign", "Auto", "[Nope]"};

laure_expression_t *laure_expression_create(
    laure_expression_type t, 
    string docstring, bool is_header, 
    string s, uint flag, 
    laure_expression_compact_bodyargs *ba,
    string q
) {
    if (! ELLIPSIS)
        ELLIPSIS = strdup("...");
    laure_expression_t *exp = malloc(sizeof(laure_expression_t));
    exp->t = t;
    exp->docstring = docstring;
    exp->is_header = is_header;
    exp->s = s;
    exp->flag = flag;
    exp->ba = ba;
    exp->fullstring = strdup(q);
    exp->flag2 = 0;
    exp->link = NULL;
    return exp;
}

laure_expression_set *laure_expression_set_link(laure_expression_set *root, laure_expression_t *new_link) {

    laure_expression_set *new_branch = malloc(sizeof(laure_expression_set));
    new_branch->expression = new_link;
    new_branch->next = NULL;

    if (root) {
        laure_expression_set *temp = root;
        while (temp->next) {
            temp = temp->next;
        }
        temp->next = new_branch;
    } else
        root = new_branch;
    
    return root;
}

laure_expression_set *laure_expression_set_link_branch(laure_expression_set *root, laure_expression_set *branch) {
    if (!root) return branch;

    laure_expression_set *pos = root;
    while (pos->next) {
        pos = pos->next;
    }
    pos->next = branch;
    return root;
}

bool is_fine_name_for_var(string s) {
    size_t sz = laure_string_strlen(s);
    
    if (!sz) return false;

    if ((s[0] == '\'' && lastc(s) == '\'') || (s[0] == '"' && lastc(s) == '"')) return false;
    int ch = laure_string_char_at_pos(s, strlen(s), 0);

    if (ch == '-' && laure_string_strlen(s) > 1) return false;

    for (int i = 0; i < strlen(DIGITS); i++) {
        if (ch == DIGITS[i]) return false;
    }

    for (int i = 0; i < sz; i++) {
        int c = laure_string_char_at_pos(s, strlen(s), i);
        for (int j = 0; j < strlen(RESTRICTED); j++) {
            if (RESTRICTED[j] == c) return false;
        }
    }
    
    return true;
}

bool is_super_fine_name_for_var(string s) {
    if (! is_fine_name_for_var(s)) return false;
    for (uint i = 0; i < strlen(NOTSUPER); i++) {
        char c = NOTSUPER[i];
        if (s[0] == c) return false;
    }
    return true;
}

uint laure_expression_get_count(laure_expression_set *root) {
    if (!root) return 0;

    uint count = 0;
    while (root && root->expression) {
        count++;
        root = root->next;
    }
    return count;
}

laure_expression_t *laure_expression_set_get_by_idx(laure_expression_set *root, uint idx) {
    if (!root) return NULL;
    laure_expression_t *exp = NULL;
    uint cur_idx = 0;

    EXPSET_ITER(root, exp, {
        if (idx == cur_idx) {
            return exp;
        }
        cur_idx++;
    });

    return NULL;
}

void laure_expression_destroy(laure_expression_t *expression) {
    if (expression->ba) {
        laure_expression_set_destroy(expression->ba->set);
        free(expression->ba);
    }
    free(expression);
}

string rough_strip_string(string s) {
    if (str_starts(s, "\"") && lastc(s) == '"') {
        char n[40];
        strcpy(n, s+1);
        strcpy(s, n);
        lastc(s) = 0;
        s = rough_strip_string(s);
    }
    return s;
}

void laure_expression_set_destroy(laure_expression_set *root) {
    if (!root) return;
    laure_expression_set_destroy(root->next);
    laure_expression_destroy(root->expression);
    free(root);
}

laure_expression_compact_bodyargs *laure_bodyargs_create(laure_expression_set *set, uint body_len, bool has_resp) {
    laure_expression_compact_bodyargs *ba = malloc(sizeof(laure_expression_compact_bodyargs));
    ba->set = set;
    ba->body_len = body_len;
    ba->has_resp = has_resp;
    return ba;
}

string read_til(const string s, int c) {

    // reads string until char
    size_t sz = laure_string_strlen(s);

    if (laure_string_char_at_pos(s, strlen(s), 0) == c) {
        return "";
    }

    size_t real_sz = strlen(s);

    uint braced = 0;
    uint bracketed = 0;
    uint parathd = 0;
    bool escaped = 0;
    bool text = 0;

    for (uint i = 0; i < sz; i++) {
        int ch = laure_string_char_at_pos(s, strlen(s), i);

        if (ch == '\\') {
            escaped = 1;
        } else if (escaped) {
            escaped = 0;
        } else if (
            parathd == 0
            && braced == 0
            && bracketed == 0
            && !text
            && (ch == c)
        ) {
            uint cur_i = i - 1;
            // slice from last_i to cur_i

            string slice = malloc(sizeof(char) * (cur_i + 1) * 4);
            memset(slice, 0, sizeof(char) * (cur_i + 1) * 4);
            
            for (int j = 0; j < cur_i + 1; j++) {
                char nch[4];
                int n = laure_string_char_at_pos(s, strlen(s), j);
                laure_string_put_char(nch, n);
                strcat(slice, nch);
            }

            return slice;
        } else if (ch == '"') {
            text = !text;
        } else if (text) {
            {};
        } else if (ch == '(') {
            parathd++;
        } else if (ch == ')' && parathd != 0) {
            parathd--;
        } else if (ch == '{') {
            braced++;
        } else if (ch == '}' && braced != 0) {
            braced--;
        } else if (ch == '[') {
            bracketed++;
        } else if (ch == ']' && bracketed != 0) {
            bracketed--;
        }
    }

    return NULL;
}

bool laure_parser_needs_continuation(string query) {

    size_t sz = laure_string_strlen(query);

    int braced = 0;
    int bracketed = 0;
    int parathd = 0;
    int escaped = 0;
    int text = 0;

    for (uint i = 0; i < sz; i++) {
        int ch = laure_string_char_at_pos(query, strlen(query), i);

        if (ch == '\\') {
            escaped = 1;
        } else if (escaped) {
            escaped = 0;
        } else if (ch == '"') {
            text = !text;
        } else if (text) {
            {};
        } else if (ch == '(') {
            parathd++;
        } else if (ch == ')' && parathd != 0) {
            parathd--;
        } else if (ch == '{') {
            braced++;
        } else if (ch == '}' && braced != 0) {
            braced--;
        } else if (ch == '[') {
            bracketed++;
        } else if (ch == ']' && bracketed != 0) {
            bracketed--;
        }
    }

    return (
        escaped ||
        text ||
        braced != 0 ||
        bracketed != 0 ||
        parathd != 0
    );
}

void str_lower(string str) {
    for(int i = 0; str[i]; i++){
        str[i] = tolower(str[i]);
    }
}


string_linked *string_split(string s_, int delimiter) {
    string s = strdup(s_);
    string_linked *linked = malloc(sizeof(string_linked));
    string_linked *ptr = linked;

    for (;;) {
        string sub = read_til(s, delimiter);
        if (!sub) break;
        ptr->s = strdup(sub);
        ptr->next = malloc(sizeof(string_linked));
        s = s + strlen(sub) + 1;
        ptr = ptr->next;
    }
    
    ptr->s = strdup(s);
    ptr->next = NULL;
    return linked;
}

typedef struct element_ {
    char c;
    short any_count;
} element;

int easy_pattern_parse_final(char s[], element *pattern[]) {

    if (!pattern[0]) return !(strlen(s));

    while (pattern[0]) {
        element *pe = pattern[0];

        if (!strlen(s)) {
            while (pattern[0]) {
                if (!pattern[0]->any_count) {
                    return 0;
                }
                pattern++;
            }
            return 1;
        }

        char c = s[0];

        if (pe->any_count) {
            for (int j = 0; j < strlen(s) + 1; j++) {
                if (easy_pattern_parse_final(s + j, pattern + 1)) {
                    return 1;
                }
                if (pe->c && s[j] != pe->c) {
                    s--;
                    goto end;
                }
            }
        }

        if (pe->c != 0
            && pe->c != c) {
            return 0;
        }

        end:
        pattern++;
        s++;
    }

    return (!strlen(s)) && (!pattern[0]);
}

bool easy_pattern_parse(char *s, char *p) {

    int idx = 0;
    element *pattern[64];
    memset(pattern, 0, 64 * sizeof(void*));

    for (int i = 0; i < strlen(p); i++) {
        char c = p[i];
        switch (c) {
            case '*': {
                pattern[idx-1]->any_count = 1;
                break;
            }
            case '.': {
                pattern[idx] = malloc(sizeof(element));
                pattern[idx]->c = 0;
                pattern[idx]->any_count = 0;
                idx++;
                break;
            }
            default: {
                pattern[idx] = malloc(sizeof(element));
                pattern[idx]->c = c;
                pattern[idx]->any_count = 0;
                idx++;
                break;
            }
        }
    }

    bool res = easy_pattern_parse_final(s, pattern);
    for (int i = 0; i < idx; i++) {
        free(pattern[i]);
    }
    return res;
}

string string_clean(string s) {
    while (s[0] == ' ') {
        s++;
    }
    while (s[strlen(s)-1] == ' ') {
        s[strlen(s) - 1] = 0;
    }
    return s;
}

uint pop_nestings(string s) {
    uint nesting = 0;
    while (strlen(s) > 2 && str_eq(s + strlen(s) - 2, "[]")) {
        nesting++;
        s[strlen(s) - 2] = 0;
    }
    return nesting;
}

uint pop_generic(string* name, string *tname, string *query) {
    string generic_before = read_til(*name, GENERIC_OPEN);
    printf("%s|%s\n", *name, generic_before);
    uint t_nest = 0;
    if (generic_before && strlen(generic_before)) {
        string generic = (*name) + strlen(generic_before) + 1;

        if (lastc(generic) == GENERIC_CLOSE) {
            char type_name[64];
            uint l = strlen(generic) - 1;
            if (l > 63) l = 63;
            strncpy(type_name, generic, l);
            type_name[l] = 0;
            *tname = strdup(type_name);
            t_nest = pop_nestings(*tname);
            *name = strdup(generic_before);
            if (query)
                *query = (*query) + strlen(generic) + 1 + (t_nest * 2);
        }
    }
    return t_nest;
}

laure_parse_many_result laure_parse_many(const string query_, char divisor, laure_expression_set *linked_opt) {
    string s = string_clean( strdup(query_) );

    if (! strlen(s)) {
        laure_parse_many_result res;
        res.is_ok = true;
        res.exps = NULL;
        return res;
    }

    size_t sz = laure_string_strlen(s);
    laure_expression_set *linked = NULL;

    size_t real_sz = strlen(s);

    uint braced = 0;
    uint bracketed = 0;
    uint parathd = 0;
    bool escaped = 0;
    bool text = 0;

    uint last_i = 0;

    for (uint i = 0; i < sz; i++) {
        int ch = laure_string_char_at_pos(s, strlen(s), i);

        if (ch == '\\') {
            escaped = 1;
        } else if (escaped) {
            escaped = 0;
        } else if (
            parathd == 0
            && braced == 0
            && bracketed == 0
            && !text
            && ch == divisor
        ) {
            uint cur_i = i - 1;
            if (i == sz - 1)
                cur_i++;
            
            // slice from last_i to cur_i
            
            char slice[1024];
            uint slice_len = 0;

            for (int j = last_i; j < cur_i + 1; j++) {
                char nch[4];
                int n = laure_string_char_at_pos(s, strlen(s), j);
                int len = laure_string_put_char(nch, n);
                if (slice_len + len >= 1024) {
                    printf("Code piece too long.\n");
                    break;
                }
                strcpy(slice + slice_len, nch);
                slice_len += len;
            }

            slice[slice_len] = 0;
            char *slice_ptr = strdup(slice);
            char *slice_ = string_clean(slice_ptr);
            if (lastc(slice_) == divisor) lastc(slice_) = 0;
            
            laure_parse_result res = laure_parse(slice_);
            free(slice_ptr);
            if (!res.is_ok) {
                laure_expression_set_destroy(linked);
                laure_parse_many_result lpmr;
                lpmr.is_ok = false;
                lpmr.err = res.err;
                return lpmr;
            }

            linked = laure_expression_set_link(linked, res.exp);

            last_i = i + 1;
        } else if (ch == '"') {
            text = !text;
        } else if (text) {
            {};
        } else if (ch == '(') {
            parathd++;
        } else if (ch == ')' && parathd != 0) {
            parathd--;
        } else if (ch == '{') {
            braced++;
        } else if (ch == '}' && braced != 0) {
            braced--;
        } else if (ch == '[') {
            bracketed++;
        } else if (ch == ']' && bracketed != 0) {
            bracketed--;
        }
    }

    if (
        parathd == 0
        && braced == 0
        && bracketed == 0
        && !text
    ) {
        if (last_i - 1 != sz - 1 || sz == 1) {
            char slice[512] = {0};
            int i = 0;
            for (int j = last_i; j < sz; j++) {
                int n = laure_string_char_at_pos(s, strlen(s), j);
                i += laure_string_put_char(slice + i, n);
            }
            
            string str = string_clean(slice);

            laure_parse_result res = laure_parse(str);
            if (!res.is_ok) {
                laure_expression_set_destroy(linked);
                laure_parse_many_result lpmr;
                lpmr.is_ok = false;
                lpmr.err = res.err;
                return lpmr;
            }

            linked = laure_expression_set_link(linked, res.exp);
        }
    }

    laure_parse_many_result lprn;
    lprn.is_ok = true;
    lprn.exps = linked;
    return lprn;
}

laure_parse_result laure_parse(string query) {

    query = string_clean(query);
    string originate = query;

    size_t len = laure_string_strlen(query);

    if (len == 0)
        error_result("query is empty");

    if (query[0] == '(' && lastc(query) == ')'){
        query = strdup(query);
        query++;
        lastc(query) = 0;
        return laure_parse(query);
    }

    if (str_eq(query, "$")) {
        laure_parse_result lpr;
        lpr.is_ok = true;
        lpr.exp = laure_expression_create(let_ref, NULL, 0, NULL, 0, NULL, query);
        return lpr;
    } else if (str_eq(query, "ID")) {
        laure_parse_result lpr;
        lpr.is_ok = true;
        lpr.exp = laure_expression_create(let_auto, NULL, 0, NULL, AUTO_ID, NULL, query);
        return lpr;
    }
    
    char det = query[0];
    query++;

    switch (det) {
        case '\'': {
            string dup = strdup(query);

            uint nesting = 0;
            while (strlen(dup) > 2 && str_eq(dup + strlen(dup) - 2, "[]")) {
                nesting++;
                dup[strlen(dup) - 2] = 0;
            }

            if (lastc(dup) != '\'')
                error_result("invalid generic or char; add `'`");
            string s = read_til(dup, '\'');
            if (! strlen(s))
                error_result("generic name or char value cannot be empty");
            laure_parse_result lpr;
            lpr.is_ok = true;
            lpr.exp = laure_expression_create(let_singlq, NULL, false, s, nesting,  NULL, --query);
            return lpr;
        }
        case '!': {
            if (! strlen(query)) {
                laure_parse_result lpr;
                lpr.is_ok = true;
                lpr.exp = laure_expression_create(let_cut, NULL, false, strdup("!"), 0, NULL, query);
                return lpr;
            } else {
                string setting = string_clean(query);
                query--;
                
                if (str_starts(setting, "error ")) {
                    string msg = setting + 6;

                    laure_parse_result msg_lpr = laure_parse(msg);
                    if (! msg_lpr.is_ok) return msg_lpr;
                    
                    laure_parse_result lpr;
                    lpr.is_ok = true;
                    lpr.exp = laure_expression_create(
                        let_command,
                        NULL,
                        false,
                        msg,
                        command_error,
                        NULL,
                        query
                    );
                    lpr.exp->link = msg_lpr.exp;
                    return lpr;
                }

                if (str_starts(setting, "use ") || str_starts(setting, "useso ")) {
                    bool use_kwd = str_starts(setting, "use ");

                    string names;
                    if (use_kwd)
                        names = setting + 4;
                    else
                        names = setting + 6;
                    if (str_starts(names, "(") && lastc(names) == ')') {
                        names++;
                        lastc(names) = 0;
                    }

                    names = string_clean(names);

                    laure_parse_result lpr;
                    lpr.is_ok = true;
                    lpr.exp = laure_expression_create(
                        let_command,
                        NULL,
                        false,
                        names,
                        use_kwd ? command_use : command_useso,
                        NULL,
                        query
                    );
                    return lpr;
                }
                string n = read_til(setting, '=');
                string v = NULL;
                if (n) {
                    // particular value
                    v = setting + strlen(n) + 1;
                } else {
                    n = setting;
                }
                if (str_starts(n, "\"") && lastc(n) == '\"') {
                    n++;
                    lastc(n) = 0;
                }
                laure_parse_result lpr;
                lpr.is_ok = true;
                lpr.exp = laure_expression_create(let_command, v ? strdup(v) : NULL, false, n, 0, NULL, query);
                return lpr;
            }
            break;
        }
        case ':': {
            laure_parse_many_result lpmr = laure_parse_many(query, ',', NULL);

            if (! lpmr.is_ok) {
                laure_parse_result lpr;
                lpr.is_ok = false;
                lpr.err = lpmr.err;
                return lpr;
            }

            laure_expression_set *set = NULL;

            while (lpmr.exps) {
                laure_expression_t *exp = lpmr.exps->expression;
                exp->is_header = true;
                set = laure_expression_set_link(set, exp);
                lpmr.exps = lpmr.exps->next;
            }

            laure_parse_result result;
            result.is_ok = true;
            result.exp = laure_expression_create(let_set, NULL, true, query, 0, laure_bodyargs_create(set, laure_expression_get_count(set), false), query);
            return result;
        }
        case '?':
        case '#': {
            bool endexcl = false;
            if (lastc(query) == '\r') {
                endexcl = true;
                lastc(query) = 0;
            }

            // abstract templates, feature #13
            bool is_abstract_template = false;
            if (query[0] == '#') {
                is_abstract_template = true;
                query++;
            }

            laure_expression_type type = (det == '?') ? let_pred : let_constraint;

            string body = NULL;
            string nonbody = read_til(query, '{');

            if (nonbody) {
                body = query + strlen(nonbody) + 1;
                lastc(body) = 0;
                query = nonbody;
            }

            query = string_clean(query);

            string name = NULL;
            string args = NULL;
            string resp = NULL;

            if (easy_pattern_parse(query, ".*(.*) *-> *.*")) {

                name = read_til(query, '(');
                query = query + strlen(name) + 1;

                args = read_til(query, ')');
                query = query + strlen(args) + 1;

                string sp = read_til(query, '>');
                if (sp)
                    resp = query + strlen(sp) + 2;
                
                free(sp);
            } else if (easy_pattern_parse(query, ".*(.*).*")) {

                name = read_til(query, '(');
                query = query + strlen(name) + 1;
                args = read_til(query, ')');

            } else if (easy_pattern_parse(query, "..* *-> *..*")) {
                
                name = read_til(query, '-');
                resp = query + strlen(name) + 2;

            } else {
                error_result("format is invalid :(");
            }

            // parse name
            laure_expression_t *linked_namespace = NULL;
            string ns = read_til(name, NS_SEPARATOR);
            if (ns) {
                name = name + strlen(ns) + 1;
                if (! strlen(name))
                    error_result("predicate/constraint name cannot be empty");
                laure_parse_result lpr_ns = laure_parse(ns);
                if (! lpr_ns.is_ok) error_format("failed to parse namespace: %s", lpr_ns.err);
                laure_expression_t *ns_exp = lpr_ns.exp;
                if (ns_exp->t == let_array) {
                    // many namespaces possible
                    // check they are valid
                    laure_expression_t *ptr;
                    EXPSET_ITER(ns_exp->ba->set, ptr, {
                        if (
                            ptr->t != let_var 
                            && ptr->t != let_singlq 
                            && ptr->t != let_atom_sign
                            && ptr->t != let_array
                        ) {
                            error_format("invalid expression passed as namespace in union (%s)", EXPT_NAMES[ptr->t]);
                        } else if (ptr->t == let_array) {
                            error_result("redundant nested union");
                        }
                    });
                }
                if (ns_exp->t != let_var && ns_exp->t != let_singlq && ns_exp->t != let_array)
                    error_format("namespace must be %s, @ or 'T' (single quoted), not %s", EXPT_NAMES[let_var], EXPT_NAMES[ns_exp->t]);
                linked_namespace = ns_exp;
            }

            // parse body, args and resp
            laure_expression_set *set = NULL;
            uint body_len = 0;
            bool has_resp = 0;

            if (!body && ((resp && strstr(resp, " when ")) || (strstr(query + strlen(args), " when ")))) {
                // body declaration using `when`
                if (resp) {
                    body = strdup( strstr(resp, " when ") + 5 );
                    resp[strlen(resp) - strlen(body) - 5] = 0;
                } else {
                    body = strdup( strstr(query, " when ") + 5 );
                }
                laure_parse_many_result lpmr = laure_parse_many(body, ',', set);
                if (!lpmr.is_ok) {
                    laure_expression_set_destroy(set);
                    error_result("failed parse body");
                } else {
                    body_len = laure_expression_get_count(lpmr.exps);
                    set = lpmr.exps;
                }
            } else if (body && strlen(body)) {
                laure_parse_many_result lpmr = laure_parse_many(body, ';', set);
                if (!lpmr.is_ok) {
                    laure_expression_set_destroy(set);
                    error_result("failed parse body");
                } else {
                    body_len = laure_expression_get_count(lpmr.exps);
                    set = lpmr.exps;
                }
            }

            if (args && strlen(args)) {
                laure_parse_many_result lpmr = laure_parse_many(args, ',', set);
                if (!lpmr.is_ok) {
                    laure_expression_set_destroy(set);
                    error_result("failed parse args");
                } else {
                    laure_expression_t *exp = NULL;
                    EXPSET_ITER(lpmr.exps, exp, {
                        set = laure_expression_set_link(set, exp);
                    });
                }
            }

            if (resp && strlen(resp)) {
                laure_parse_result lpr = laure_parse(resp);
                if (!lpr.is_ok) {
                    laure_expression_set_destroy(set);
                    error_result("failed parse resp");
                } else {
                    has_resp = 1;
                    set = laure_expression_set_link(set, lpr.exp);
                }
            }

            laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, body_len, has_resp);
            bool is_primitive = strlen(name) == 0 && body_len == 0;

            laure_parse_result lpr;
            lpr.is_ok = true;
            lpr.exp = laure_expression_create(type, "", false, name, PREDFLAG_GET(is_primitive, is_abstract_template, endexcl), ba, query);
            lpr.exp->link = linked_namespace;

            return lpr;
        }
        case '$': {
            // structure
            error_result("not implemented");
        }
        case '[': {
            if (lastc(query) != ']') goto gotodefault;
            if (strstr(query, "=") || strstr(query, "~")) goto gotodefault;
            if (lastc(query) != ']') {
                error_result("invalid array");
            }

            lastc(query) = 0;
            string head = read_til(query, '|');

            if (head) {
                string tail = query + strlen(head) + 1;
                char q[128];
                if (strlen(head) && strlen(tail)) 
                    snprintf(q, 128, "append([%s], %s)", head, tail);
                else if (strlen(head)) {
                    snprintf(q, 128, "[%s]", head);
                } else if (strlen(tail)) {
                    snprintf(q, 128, "%s", tail);
                } else {
                    strcpy(q, "[]");
                }
                return laure_parse(q);
            }

            laure_expression_set *set;

            if (strlen(query) == 0) {
                set = NULL;
            } else {
                laure_parse_many_result res = laure_parse_many(query, ',', NULL);
                if (!res.is_ok) {
                    error_result(res.err);
                }
                set = res.exps;
            }
            
            laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, laure_expression_get_count(set), 0);

            laure_parse_result lpr;
            lpr.is_ok = true;
            lpr.exp = laure_expression_create(let_array, "", false, strdup("array"), 0, ba, query);
            return lpr;
        }
        case '&': {
            if (query[0] == '{') {
                // Isolated set (feature #10)
                laure_parse_result lpr = laure_parse(query);
                if (! lpr.is_ok) {
                    return lpr;
                }
                lpr.exp->flag = 1;
                return lpr;
            }

            // Quantified expression
            // 1 - all
            // 2 - exists
            string quant_name = read_til(query, ' ');
            if (!quant_name)
                error_result("invalid quantifier format");
            
            query = query + strlen(quant_name) + 1;
            string vname = read_til(query, ' ');
            if (!vname) 
                error_result("var name is undefined");
            
            query = query + strlen(vname) + 1;
            if (!strlen(query)) 
                error_result("body is undefined");
            
            string body = query;
            if (laure_string_strlen(body) < 2 || body[0] != '{' || lastc(body) != '}')
                error_result("body is invalid");

            body++;
            lastc(body) = 0;
            body = string_clean(body);

            str_lower(quant_name);

            uint quant_t = 0;
            if (str_eq(quant_name, "all") || str_eq(quant_name, "a"))
                quant_t = 1;
            else if (str_eq(quant_name, "exists") || str_eq(quant_name, "e"))
                quant_t = 2;
            
            if (!quant_t)
                error_result("quantor is undefined; expected `all`/`a`, `exists`/`e`");

            laure_expression_set *set = NULL;
            uint count = 0;

            if (laure_string_strlen(body)) {
                laure_parse_many_result res = laure_parse_many(body, ';', NULL);
                if (!res.is_ok)
                    error_result(res.err);
                count = laure_expression_get_count(res.exps);
                set = laure_expression_compose(res.exps);
            }
            
            laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, count, false);
            laure_expression_t *exp = laure_expression_create(let_quant, "", false, vname, quant_t, ba, query);
            laure_parse_result pr;
            pr.is_ok = true;
            pr.exp = exp;
            return pr;
        }
        default: {
            gotodefault:
            query--;

            string _temp = read_til(query, '-');
            if (_temp && (query + strlen(_temp) + 1)[0] == '>') {
                // implication
                laure_parse_result left_result = laure_parse(strdup(_temp));
                if (!left_result.is_ok) {
                    error_format("left side of implication is invalid: %s", left_result.err);
                }

                laure_expression_set *implication_expset = laure_expression_set_link(NULL, laure_expression_create(let_set, NULL, false, strdup(_temp), 0, laure_bodyargs_create(
                    laure_expression_compose_one(left_result.exp),
                    0,
                    false
                ), query));

                string implies_for = query + strlen(_temp) + 3;
                while(implies_for[0] == ' ') implies_for++;

                if (implies_for[0] == '{' && lastc(implies_for) == '}') {
                    string q = malloc(strlen(implies_for) - 1);
                    strcpy(q, implies_for + 1);
                    lastc(q) = 0;
                    q = string_clean(q);
                    laure_parse_many_result q_result = laure_parse_many(q, ';', implication_expset);
                    if (! q_result.is_ok) {
                        free(q);
                        free(implication_expset);
                        laure_expression_destroy(left_result.exp);
                        error_format("right side of implication is invalid: %s", q_result.err);
                    }
                    implication_expset = laure_expression_set_link_branch(implication_expset, laure_expression_compose(q_result.exps));
                } else {
                    laure_parse_many_result q_result = laure_parse_many(implies_for, ',', implication_expset);
                    if (! q_result.is_ok) {
                        free(implication_expset);
                        error_format("right side of implication is invalid: %s", q_result.err);
                    }
                    implication_expset = laure_expression_set_link_branch(implication_expset, laure_expression_compose(q_result.exps));
                }

                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(implication_expset, laure_expression_get_count(implication_expset), false);
                laure_expression_t *exp = laure_expression_create(let_imply, NULL, false, strdup(_temp), 0, ba, query);
                laure_parse_result res;
                res.is_ok = true;
                res.exp = exp;
                return res;
            }

            laure_parse_result lpr;
            lpr.is_ok = true;

            string left;

            uint flag = 0;

            left = read_til(query, '=');

            if (!left || (lastc(left) == '>' || lastc(left) == '<')) {
                left = read_til(query, '~');
                flag = 1;
            }

            if (!left) {
                left = read_til(query, '%');
                flag = 2;
            }

            if (left) {
                // assert/image/name instruction
                // expression bodyargs consists of two asserted instances

                laure_expression_type type = let_nope;
                switch (flag) {
                    case 0: {
                        type = let_assert;
                        break;
                    }
                    case 1: {
                        type = let_image;
                        break;
                    }
                    case 2: {
                        type = let_name;
                        break;
                    }
                    default:
                        break;
                }

                string right = query + strlen(left) + 1;

                // parse left and right
                laure_parse_result left_result = laure_parse(left);
                laure_parse_result right_result = laure_parse(right);

                if (!left_result.is_ok || !right_result.is_ok) {
                    error_result("invalid left or right side of expression");
                }

                laure_expression_set *set = laure_expression_set_link(NULL, left_result.exp);
                set = laure_expression_set_link(set, right_result.exp);

                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                laure_expression_t *exp = laure_expression_create(type, "", false, query, 0, ba, query);

                lpr.exp = exp;
                return lpr;
            }

            flag = 0;
            left = read_til(query, '|');

            if (left) {
                laure_parse_many_result res = laure_parse_many(query, '|', NULL);
                if (!res.is_ok) {
                    error_result("invalid choice expression");
                }
                laure_expression_set *set = res.exps;
                
                // choice expression (unpacked)
                laure_expression_compact_bodyargs *ba =
                    laure_bodyargs_create(set, laure_expression_get_count(set), 0);
                
                lpr.exp = laure_expression_create(let_choice_1, "", false, query, 0, ba, query);
                return lpr;
            }

            if (lastc(query) == '?') {
                // force unify operation
                string vn = strdup(query);
                lastc(vn) = 0;
                if (!is_fine_name_for_var(vn)) {
                    free(vn);
                    error_result("something to unify should be var");
                }
                lpr.exp = laure_expression_create(let_unify, "", false, vn, 0, NULL, query);
                return lpr;
            }

            if (query[0] == '{') {
                if (lastc(query) != '}') error_result("invalid set");
                query = strdup(query);
                query++;
                lastc(query) = 0;
                laure_parse_many_result lpmr = laure_parse_many(query, ';', NULL);
                if (!lpmr.is_ok) {
                    error_format("cannot parse {body}: %s", lpmr.err);
                } else {
                    laure_parse_result lpr;
                    lpr.is_ok = true;
                    laure_expression_set *optzd_expset = laure_expression_compose(lpmr.exps);
                    lpr.exp = laure_expression_create(let_set, NULL, false, NULL, 0, laure_bodyargs_create(optzd_expset, 0, false), query);
                    return lpr;
                }
            }

            string temp = read_til(query, ' ');

            if (temp && strlen(temp)) {
                laure_parse_result res1 = laure_parse(temp);
                string right = query + strlen(temp) + 1;

                if (!res1.is_ok) {
                    string li;
                    lineinfo(li);
                    error_format("error %s\n", li);
                }

                laure_expression_t *el1 = res1.exp;

                string temp2 = read_til(right, ' ');

                if (!temp2 || !strlen(temp2)) {
                    // count = 2
                    // declaration
                    // `type Var`
                    laure_parse_result res2 = laure_parse(right);
                    if (!res2.is_ok) {
                        error_format("can't parse right declaration side (%s)", res2.err);
                    }
                    laure_expression_t *el2 = res2.exp;

                    if (el1->t != let_var) {
                        error_result("second element in declaration should be var");
                    }

                    laure_expression_set *set = laure_expression_set_link(NULL, el1);
                    set = laure_expression_set_link(set, el2);

                    laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                    lpr.exp = laure_expression_create(let_decl, "", false, strdup(query), 0, ba, query);
                    return lpr;

                } else {
                    string temp3 = right + strlen(temp2) + 1;

                    laure_parse_result res2 = laure_parse(temp2);
                    if (!res2.is_ok) {
                        error_format("can't parse middle (%s)", res2.err);
                    }
                    laure_expression_t *el2 = res2.exp;

                    laure_parse_result res3 = laure_parse(temp3);
                    if (!res3.is_ok) {
                        error_format("can't parse right side (%s)", res3.err);
                    }
                    laure_expression_t *el3 = res3.exp;

                    if (temp3 && strlen(temp3)) {
                        // count = 3
                        // maybe predicate call
                        // specific patterns for predicate notations
                        if (strcmp(el2->s, LAURE_SYNTAX_INFIX_PREPOSITION) == 0) {
                            // predicate call with one argument
                            // `length of Something`

                            // add generic if needed
                            string t_name = el1->ba ? el1->ba->set->expression->s : NULL;
                            uint t_nest = el1->ba ? res1.flag : 0;

                            laure_expression_set *set = laure_expression_set_link(NULL, el3);
                            laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 1, 0);
                            lpr.exp = laure_expression_create(let_pred_call, t_name ? t_name : "", false, el1->s, t_nest, ba, query);
                            return lpr;
                        } else {
                            // predicate call with two arguments
                            // `A + 1`

                            // add generic if needed
                            string t_name = el2->ba ? el2->ba->set->expression->s : NULL;
                            uint t_nest = el2->ba ? res2.flag : 0;
                            
                            laure_expression_set *set = laure_expression_set_link(NULL, el1);
                            set = laure_expression_set_link(set, el3);
                            laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                            lpr.exp = laure_expression_create(let_pred_call, t_name ? t_name : "", false, el2->s, t_nest, ba, query);
                            lpr.exp->flag2 = strlen(temp) + 1;
                            return lpr;
                        }
                    }
                }
            } else {

                if (easy_pattern_parse(query, "..*(.*)")) {
                    if (!easy_pattern_parse(query, "..*(.*)")) {
                        error_result("invalid 2");
                    }
                    
                    string name = read_til(query, '(');

                    laure_expression_t *first_linked_argument = NULL;
                    string ns = read_til(name, NS_SEPARATOR);
                    if (ns) {
                        name = name + strlen(ns) + 1;
                        if (! strlen(name))
                            error_result("predicate/constraint name cannot be empty");
                        laure_parse_result lpr_ns = laure_parse(ns);
                        if (! lpr_ns.is_ok) error_format("failed to parse namespace: %s", lpr_ns.err);
                        laure_expression_t *ns_exp = lpr_ns.exp;
                        // if (ns_exp->t != let_var)
                        //    error_format("namespace must be %s not %s", EXPT_NAMES[let_var], EXPT_NAMES[ns_exp->t]);
                        first_linked_argument = ns_exp;
                        query += strlen(ns) + 1;
                    }

                    string generic_before = read_til(name, GENERIC_OPEN);
                    string t_name = NULL;
                    uint t_nest = 0;
                    if (generic_before && strlen(generic_before)) {
                        string generic = name + strlen(generic_before) + 1;

                        if (lastc(generic) == GENERIC_CLOSE) {
                            char type_name[64];
                            uint l = strlen(generic) - 1;
                            if (l > 63) l = 63;
                            strncpy(type_name, generic, l);
                            type_name[l] = 0;
                            t_name = strdup(type_name);
                            t_nest = pop_nestings(t_name);
                            name = strdup(generic_before);
                            query += strlen(generic) + 1 + (t_nest * 2);
                        }
                    }

                    query = query + strlen(name) + 1;
                    string args = read_til(query, ')');

                    if (str_starts(name, "\\")) name++;

                    laure_expression_set *args_exps = NULL;

                    if (args && strlen(args)) {
                        laure_parse_many_result args_res = laure_parse_many(args, ',', NULL);
                        if (!args_res.is_ok) {
                            error_result(args_res.err);
                        }
                        args_exps = args_res.exps;
                    }

                    if (first_linked_argument) {
                        if (! args_exps) {
                            args_exps = laure_expression_set_link(NULL, first_linked_argument);
                        } else {
                            args_exps = laure_expression_set_link_branch(
                                laure_expression_set_link(NULL, first_linked_argument),
                                args_exps
                            );
                        }
                    }

                    laure_expression_compact_bodyargs *ba = laure_bodyargs_create(args_exps, laure_expression_get_count(args_exps), 0);
                    lpr.exp = laure_expression_create(let_pred_call, t_name ? t_name : "", false, name, t_nest, ba, originate);
                    return lpr;
                }

                string dup = strdup(query);

                if (dup[0] == '-' && is_super_fine_name_for_var(dup + 1)) {
                    // negate variable
                    // {x = -y} means {x = y * -1}
                    string var_name = dup + 1;
                    char nquery[256];
                    snprintf(nquery, 256, "*(-1, %s)", dup + 1);
                    return laure_parse(strdup(nquery));
                } else if (lastc(dup) == ']' && dup[strlen(dup)-2] != '[') {
                    string vname = read_til(dup, '[');
                    if (! vname || ! strlen(vname) || ! is_fine_name_for_var(vname)) {
                        if (! vname || strlen(vname) > 0) {
                            error_format("invalid declaration `%s` for array by_idx, must be var", vname);
                        }
                    } else {
                        string idx_s = malloc(strlen(dup) - strlen(vname) - 1);
                        strncpy(idx_s, dup + strlen(vname) + 1, strlen(dup) - strlen(vname) - 2);
                        char nquery[256];
                        snprintf(nquery, 256, "by_idx(%s, %s)", vname, idx_s);
                        free(idx_s);
                        return laure_parse(strdup(nquery));
                    }
                }

                if (str_starts(query, "@")) {
                    // flag = 0 | 1
                    // 0 SINGLE
                    // 1 SET
                    query++;
                    if (str_starts(query, "{")) {
                        if (lastc(query) != '}') {
                            error_result("invalid atomic set; expected `}`");
                        }
                        query++;
                        lastc(query) = 0;
                        if (! laure_string_strlen(query)) error_result("empty atomic sets are forbidden");
                        laure_parse_many_result lpmr = laure_parse_many(query, ',', NULL);
                        query[strlen(query)] = '}';
                        if (! lpmr.is_ok) {
                            error_format("error parsing atomic set: %s", lpmr.err);
                        }
                        laure_expression_t *ptr; EXPSET_ITER(lpmr.exps, ptr, {
                            if (ptr->t != let_var && ptr->t != let_custom) {
                                error_format("error parsing atomic set, atom -> `%s`, Atom declaration cannot be %s\nMust be var-like or data-like", ptr->s, ptr->t != let_atom ? EXPT_NAMES[ptr->t] : "another atom");
                            }
                        });
                        laure_parse_result lpr;
                        lpr.is_ok = true;
                        query--;
                        lpr.exp = laure_expression_create(
                            let_atom, NULL, false, query, 1, 
                            laure_bodyargs_create(lpmr.exps, laure_expression_get_count(lpmr.exps), 0),
                            query
                        );
                        return lpr;
                    } else {
                        if (! strlen(query)) {
                            // "@" atom sign query
                            laure_parse_result lpr;
                            lpr.is_ok = true;
                            lpr.exp = laure_expression_create(let_atom_sign, NULL, false, --query, 0, NULL, query);
                            return lpr;
                        }
                        laure_parse_result lpr = laure_parse(query);
                        if (! lpr.is_ok) error_format("cannot read atom `@%s`", query);
                        else if (lpr.exp->t != let_var && lpr.exp->t != let_custom) {
                            error_format(
                                "error parsing atom -> `%s`, Atom declaration cannot be %s\nMust be var-like or data-like", 
                                query, lpr.exp->t != let_atom ? EXPT_NAMES[lpr.exp->t] : "another atom"
                            );
                        }
                        lpr.exp = laure_expression_create(let_atom, NULL, false, lpr.exp->s, 0, NULL, query);
                        return lpr;
                    }
                }

                uint nesting = 0;
                uint gnestings = 0;
                while (strlen(dup) > 2 && str_eq(dup + strlen(dup) - 2, "[]")) {
                    nesting++;
                    dup[strlen(dup) - 2] = 0;
                }

                if (is_fine_name_for_var(dup)) {
                    laure_expression_compact_bodyargs *ba = NULL;
                    if (lastc(dup) == '>' && strstr(dup, "<") > dup) {
                        string clarif = strstr(dup, "<") + 1;
                        lastc(clarif) = 0;
                        *(clarif - 1) = 0;
                        gnestings = pop_nestings(clarif);
                        laure_parse_many_result lpr = laure_parse_many(clarif, ',', NULL);
                        if (! lpr.is_ok) {
                            error_format("error parsing clarification: %s", lpr.err);
                        }
                        laure_expression_t *ptr;
                        EXPSET_ITER(lpr.exps, ptr, {
                            if (! (ptr->t == let_var || ptr->t == let_singlq)) {
                                error_format("invalid clarification, clarificator can't be %s", EXPT_NAMES[ptr->t]);
                            }
                        });
                        ba = laure_bodyargs_create(lpr.exps, laure_expression_get_count(lpr.exps), 0);
                    }
                    lpr.exp = laure_expression_create(let_var, "", false, dup, nesting, ba, query);
                    lpr.flag = gnestings;
                    return lpr;
                }

                // custom
                // integer/string/custom image macros
                laure_parse_result lpr;
                lpr.is_ok = true;
                lpr.exp = laure_expression_create(let_custom, "", false, dup, nesting, NULL, query);
                return lpr;
            }

            break;
        }
    }

    laure_parse_result lpr;
    lpr.is_ok = true;
    lpr.exp = NULL;
    return lpr;
}

void laure_expression_show(laure_expression_t *exp, uint indent) {

    if (!exp) {printf("none\n"); return;}

    switch (exp->t) {
        case let_constraint:
        case let_pred: {
            printindent(indent);
            printf("predicate %s [\n", exp->s);
            laure_expression_t *ptr = NULL;
            if (!exp->ba) {
                return;
            }
            EXPSET_ITER(exp->ba->set, ptr, {
                laure_expression_show(ptr, indent + 2);
            });
            printindent(indent);
            printf("]\n");
            break;
        }

        case let_array: {
            printindent(indent);
            printf("array [\n");
            laure_expression_t *ptr = NULL;
            EXPSET_ITER(exp->ba->set, ptr, {
                laure_expression_show(ptr, indent + 2);
            });
            printindent(indent);
            printf("]\n");
            break;
        }

        case let_quant: {
            printindent(indent);
            printf("quantified %s %s [\n", exp->flag ? "All" : "Exists", exp->s);
            laure_expression_t *ptr = NULL;
            EXPSET_ITER(exp->ba->set, ptr, {
                laure_expression_show(ptr, indent + 2);
            });
            printindent(indent);
            printf("]\n");
            break;
        }

        case let_var: {
            printindent(indent);
            printf("var %s\n", exp->s);
            break;
        }

        case let_name: {
            printindent(indent);
            printf("naming [\n");
            laure_expression_t *ptr = NULL;
            EXPSET_ITER(exp->ba->set, ptr, {
                laure_expression_show(ptr, indent + 2);
            });
            printindent(indent);
            printf("]\n");
            break;
        }

        case let_pred_call: {
            printindent(indent);
            printf("call pred %s [\n", exp->s);
            printindent(indent + 2);
            printf("docstring: %s; nesting: %d\n", exp->docstring, exp->flag);
            laure_expression_t *ptr = NULL;
            EXPSET_ITER(exp->ba->set, ptr, {
                laure_expression_show(ptr, indent + 2);
            });
            printindent(indent);
            printf("]\n");
            break;
        }

        case let_assert: {
            printindent(indent);
            printf("assert [\n");
            laure_expression_t *ptr = NULL;
            EXPSET_ITER(exp->ba->set, ptr, {
                laure_expression_show(ptr, indent + 2);
            });
            printindent(indent);
            printf("]\n");
            break;
        }

        case let_unify: {
            printindent(indent);
            printf("unify %s\n", exp->s);
            break;
        }

        case let_imply: {
            printindent(indent);
            printf("imply [\n");
            laure_expression_t *ptr = NULL;
            EXPSET_ITER(exp->ba->set->expression->ba->set, ptr, {
                laure_expression_show(ptr, indent + 2);
            });
            printindent(indent); printf("] for [\n");
            ptr = NULL;
            EXPSET_ITER(exp->ba->set->next, ptr, {
                laure_expression_show(ptr, indent + 2);
            });
            printindent(indent); printf("]\n");
            break;
        }

        case let_custom: {
            printindent(indent);
            printf("data {%s%s%s}\n", GRAY_COLOR, exp->s, NO_COLOR);
            break;
        }

        case let_cut: {
            printindent(indent);
            printf("cut\n");
            break;
        }

        case let_atom: {
            printindent(indent);
            printf("atom\n");
            break;
        }

        case let_set: {
            printindent(indent);
            if (exp->flag) printf("isolated ");
            printf("set {\n");
            laure_expression_t *ptr = NULL;
            EXPSET_ITER(exp->ba->set, ptr, {
                laure_expression_show(ptr, indent + 2);
            });
            printindent(indent);
            printf("}\n");
            break;
        }

        case let_command: {
            printindent(indent);
            printf("command ");
            switch (exp->flag) {
                case command_setflag: {
                    printf("set flag %s to %s", exp->s, exp->docstring ? exp->docstring : "<default>");
                    break;
                }
                case command_use: {
                    printf("use kdb %s", exp->s);
                    break;
                }
                case command_useso: {
                    printf("use shared lib %s", exp->s);
                    break;
                }
            }
            printf("\n");
            break;
        }
    
        default: {
            printindent(indent);
            printf("%s\n", EXPT_NAMES[exp->t]);
            break;
        }
    }
}

laure_expression_set *laure_get_all_vars_in(laure_expression_t *exp, laure_expression_set *linked) {
    if (!exp) return NULL;
    switch (exp->t) {
    case let_var:
    case let_unify:
    case let_decl: {
        linked = laure_expression_set_link(linked, exp);
        break;
    }
    case let_array:
    case let_choice_2:
    case let_assert:
    case let_set:
    case let_pred_call: {
        laure_expression_t *e = NULL;
        EXPSET_ITER(exp->ba->set, e, {
            linked = laure_get_all_vars_in(e, linked);
        });
        break;
    }
    default:
        break;
    }
    return linked;
}

laure_expression_set *laure_expression_compose_one(laure_expression_t *exp) {
    if (! exp) return NULL;
    laure_expression_set *set = NULL;

    switch (exp->t) {
        case let_assert: {
            laure_expression_t *left = laure_expression_set_get_by_idx(exp->ba->set, 0);
            laure_expression_t *right = laure_expression_set_get_by_idx(exp->ba->set, 1);

            if (left->t == let_choice_1 || right->t == let_choice_1) {
                laure_expression_t *choice = left->t == let_choice_1 ? left : right;
                laure_expression_t *notchoice = left->t == let_choice_1 ? right : left;
                laure_expression_set *nset = NULL;

                laure_expression_t *ptr = NULL;
                EXPSET_ITER(choice->ba->set, ptr, {
                    //! invalid.
                    laure_expression_set *s = laure_expression_set_link_branch(
                        laure_expression_set_link(NULL, notchoice),
                        laure_expression_compose_one(ptr)
                    );
                    laure_expression_compact_bodyargs *ba = laure_bodyargs_create(
                        s,
                        laure_expression_get_count(s),
                        false
                    );
                    laure_expression_t *assert_exp = laure_expression_create(let_assert, "", false, NULL, 0, ba, exp->fullstring);
                    laure_expression_set *choice_set = laure_expression_compose_one(assert_exp);

                    laure_expression_compact_bodyargs *ba_ = laure_bodyargs_create(choice_set, 1, false);

                    nset = laure_expression_set_link(
                        nset, laure_expression_create(let_set, "", false, NULL, 0, ba_, exp->fullstring)
                    );
                });

                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(nset, laure_expression_get_count(nset), false);
                laure_expression_t *nexp = laure_expression_create(let_choice_2, "", false, NULL, 0, ba, ELLIPSIS);
                set = laure_expression_set_link(set, nexp);
                break;
            }

            if (left->t == let_decl) {
                laure_expression_t *dtype = laure_expression_set_get_by_idx(left->ba->set, 0);
                laure_expression_t *dname = laure_expression_set_get_by_idx(left->ba->set, 1);
                
                // int a = 5
                // composes into
                // a ~ int; a = 5

                laure_expression_t *image_exp = laure_expression_create(
                    let_image, "", false, NULL, 
                    0, laure_bodyargs_create(
                        laure_expression_set_link(
                            laure_expression_set_link(NULL, dname), 
                            dtype
                        ), 
                        2, false
                    ), exp->fullstring
                );

                laure_expression_set *right_branch = laure_expression_compose_one(right);

                laure_expression_t *assert_exp = laure_expression_create(
                    let_assert, "", false, NULL,
                    0, laure_bodyargs_create(
                        laure_expression_set_link_branch(
                            laure_expression_set_link(NULL, dname),
                            right_branch
                        ),
                        2, false
                    ), exp->fullstring
                );

                set = laure_expression_set_link(set, image_exp);
                set = laure_expression_set_link_branch(set, laure_expression_compose_one(assert_exp));
                break;
            }

            if (left->t == let_pred_call && right->t == let_pred_call) {
                string vname = laure_scope_generate_unique_name();
                laure_expression_t *var = laure_expression_create(let_var, left->s, false, vname, 0, NULL, exp->fullstring);
                left->ba->set = laure_expression_set_link(left->ba->set, var);
                left->ba->has_resp = true;
                right->ba->set = laure_expression_set_link(right->ba->set, var);
                right->ba->has_resp = true;
                set = laure_expression_set_link_branch(set, laure_expression_compose_one(left));
                set = laure_expression_set_link_branch(set, laure_expression_compose_one(right));
                break;
            } else if (left->t == let_pred_call || right->t == let_pred_call) {
                laure_expression_t *pc;
                laure_expression_t *resp;
                if (left->t == let_pred_call) {
                    pc = left;
                    resp = right;
                } else {
                    pc = right;
                    resp = left;
                }
                pc->ba->has_resp = true;
                pc->ba->set = laure_expression_set_link(pc->ba->set, resp);
                set = laure_expression_set_link_branch(set, laure_expression_compose_one(pc));
                break;
            }
            break;
        }
        case let_pred_call: {
            laure_expression_set *args = NULL;

            for (int i = 0; i < exp->ba->body_len; i++) {
                laure_expression_t *arg_exp = laure_expression_set_get_by_idx(exp->ba->set, i);
                if (arg_exp->t == let_var || arg_exp->t == let_custom || arg_exp->t == let_array || arg_exp->t == let_atom || arg_exp->t == let_singlq) {
                    args = laure_expression_set_link(args, arg_exp);
                } else {
                    char buff[16];
                    snprintf(buff, 16, "$%lu", laure_scope_generate_link());

                    laure_expression_t *var = laure_expression_create(let_var, arg_exp->s, false, strdup(buff), 0, NULL, exp->fullstring);

                    laure_expression_set *assert_set = laure_expression_set_link(NULL, var);
                    assert_set = laure_expression_set_link(assert_set, arg_exp);

                    laure_expression_compact_bodyargs *ba = laure_bodyargs_create(assert_set, 2, false);
                    laure_expression_t *assert_exp = laure_expression_create(let_assert, NULL, false, NULL, 0, ba, exp->fullstring);
                    args = laure_expression_set_link(args, var);
                    set = laure_expression_set_link_branch(set, laure_expression_compose_one(assert_exp));
                }
            }
            
            if (exp->ba->has_resp) {
                args = laure_expression_set_link(args, laure_expression_set_get_by_idx(exp->ba->set, exp->ba->body_len));
            }
            
            exp->ba->set = args;
            set = laure_expression_set_link(set, exp);
            break;
        }
        case let_choice_1: {
            laure_expression_set *nset = NULL;
            laure_expression_t *ptr = NULL;
            EXPSET_ITER(exp->ba->set, ptr, {

                laure_expression_compact_bodyargs *ba;

                if (ptr->t == let_set) {
                    laure_expression_set *eset = laure_expression_compose(ptr->ba->set);
                    ba = laure_bodyargs_create(eset, laure_expression_get_count(eset), false);
                } else {
                    laure_expression_set *eset = laure_expression_compose_one(ptr);
                    ba = laure_bodyargs_create(eset, 1, false);
                }

                laure_expression_t *e = laure_expression_create(let_set, NULL, false, NULL, 0, ba, exp->fullstring);
                nset = laure_expression_set_link(nset, e);
            });
            set = laure_expression_set_link(
                set, 
                laure_expression_create(
                    let_choice_2, NULL, 
                    false, NULL, 0, 
                    laure_bodyargs_create(nset, laure_expression_get_count(nset), false),
                    exp->fullstring
                )
            );
            break;
        }
        default: break;
    }

    if (!set) {
        set = laure_expression_set_link(NULL, exp);
    }
    return set;
}

laure_expression_set *laure_expression_compose(laure_expression_set *set) {
    if (! set) return NULL;
    laure_expression_set *new_set = NULL;
    laure_expression_t *ptr = NULL;
    EXPSET_ITER(set, ptr, {
        laure_expression_set *composed = laure_expression_compose_one(ptr);
        if (composed) {
            new_set = laure_expression_set_link_branch(new_set, composed);
        } else
            new_set = laure_expression_set_link(new_set, ptr);
    });
    return new_set;
}

#endif