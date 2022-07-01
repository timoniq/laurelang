#ifndef LAURELANG_H
#define LAURELANG_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <setjmp.h>
#include "expr.h"

#define ulong unsigned long
#define streq(s, s2) strcmp(s, s2) == 0
#define str_eq(s, s2) (strcmp(s, s2) == 0)
#define str_starts(s, start) (strncmp(start, s, strlen(start)) == 0)
#define string char*
#ifndef NULL
#define NULL (void*)0
#endif

extern ulong *LAURE_LINK_ID;
typedef enum {false, true} bool;

typedef unsigned int uint;

typedef struct laure_instance {
    string name;
    string doc;
    bool locked;
    string (*repr)(struct laure_instance *ins);
    void *image;
} Instance;

typedef struct laure_control_ctx control_ctx;
jmp_buf JUMPBUF;

/* =-----------=
     Scope
=-----------= */

#ifdef SCOPE_LINKED

// Efficient implementation of linked scope

typedef struct linked_scope {
    Instance *ptr;
    ulong link;
    struct linked_scope *next;
    string owner;
} linked_scope_t;

typedef struct laure_scope {
    long *nlink;
    uint idx, repeat;
    linked_scope_t *linked;
    struct laure_scope *tmp, *glob, *next;
} laure_scope_t;

#define laure_scope_iter(scope, to_ptr, body) do { \
        linked_scope_t *to_ptr; \
        linked_scope_t *__l = scope->linked; \
        while (__l) { \
            to_ptr = __l; \
            body; \
            __l = __l->next; \
        } \
    } while (0)

#define SCOPE_CELL_T linked_scope_t*

#else

// Implementation of stodgy scope needed for debug

typedef struct laure_cell {
    Instance *ptr;
    ulong link;
    uint idx;
} laure_cell;

#define max_cells 256

typedef struct laure_scope {
    unsigned long *nlink;
    uint idx, repeat, count;
    laure_cell cells[max_cells];
    struct laure_scope *glob, *next;
    string owner;
} laure_scope_t;

#define laure_scope_iter(scope, to_ptr, body) do { \
            laure_cell *to_ptr = NULL; \
            for (uint idx = 0; idx < scope->count; idx++) { \
                to_ptr = &scope->cells[idx]; \
                body; \
            } \
        } while (0)

#define SCOPE_CELL_T laure_cell

#endif

SCOPE_CELL_T laure_scope_insert(laure_scope_t *scope, Instance *ptr);
SCOPE_CELL_T laure_scope_insert_l(laure_scope_t *scope, Instance *ptr, ulong link);
SCOPE_CELL_T laure_scope_find(
    laure_scope_t *scope, 
    bool (*checker)(SCOPE_CELL_T, void*),
    void *payload,
    bool copy,
    bool search_glob
);
Instance *laure_scope_find_by_key(laure_scope_t *scope, string key, bool search_glob);
Instance *laure_scope_find_by_key_l(laure_scope_t *scope, string key, ulong *link, bool search_glob);
Instance *laure_scope_find_by_link(laure_scope_t *scope, ulong link, bool search_glob);
Instance *laure_scope_change_link_by_key(laure_scope_t *scope, string key, ulong new_link, bool search_glob);
Instance *laure_scope_change_link_by_link(laure_scope_t *scope, ulong link, ulong new_link, bool search_glob);
laure_scope_t *laure_scope_create_copy(control_ctx *cctx, laure_scope_t *scope);
void laure_scope_show(laure_scope_t *scope);
ulong laure_scope_generate_link();
string laure_scope_generate_unique_name();
laure_scope_t *laure_scope_create_global();
laure_scope_t *laure_scope_new(laure_scope_t *global, laure_scope_t *next);
void laure_scope_free(laure_scope_t *scope);
string laure_scope_get_owner(laure_scope_t *scope);

/* =-----------=
    Parser
=-----------= */

struct laure_expression_set_;

typedef struct {
    // set consists of
    // body of length body_len
    // resp if has_resp
    // the rest is args
    struct laure_expression_set_ *set;
    uint body_len;
    bool has_resp;
} laure_expression_compact_bodyargs;

// laure_expression is compact unified container
// for all possible expressiong
typedef struct laure_expression {
    laure_expression_type t;
    char *docstring, *fullstring;
    bool is_header;

    char *s;
    struct laure_expression *link;
    uint flag, linepos; // nesting for let_var, cursor position for error handling
    laure_expression_compact_bodyargs *ba;
} laure_expression_t;

typedef struct laure_expression_set_ {
    laure_expression_t *expression;
    struct laure_expression_set_ *next;
} laure_expression_set;

#define EXPSET_ITER(set_, ptr_, body) do { \
            laure_expression_set *_set = set_; \
            while (_set) { \
                ptr_ = _set->expression; \
                body; \
                _set = _set->next; \
            } \
        } while (0);

typedef struct string_linked_ {
    char *s;
    struct string_linked_ *next;
} string_linked;

string_linked *string_split(char *s, int delimiter);

laure_expression_set *laure_expression_set_link(laure_expression_set *root, laure_expression_t *new_link);
uint laure_expression_get_count(laure_expression_set *root);
void laure_expression_destroy(laure_expression_t *expression);
void laure_expression_set_destroy(laure_expression_set *root);
laure_expression_t *laure_expression_set_get_by_idx(laure_expression_set *root, uint idx);
laure_expression_compact_bodyargs *laure_bodyargs_create(laure_expression_set *set, uint body_len, bool has_resp);
void laure_expression_show(laure_expression_t *exp, uint indent);
laure_expression_t *laure_expression_create(laure_expression_type t, char *docstring, bool is_header, char *s, uint flag, laure_expression_compact_bodyargs *ba, string q);
laure_expression_compact_bodyargs *laure_bodyargs_create(laure_expression_set *set, uint body_len, bool has_resp);
laure_expression_set *laure_get_all_vars_in(laure_expression_t *exp, laure_expression_set *linked);
laure_expression_set *laure_expression_set_link_branch(laure_expression_set *root, laure_expression_set *branch);
laure_expression_set *laure_expression_compose_one(laure_expression_t *exp);
laure_expression_set *laure_expression_compose(laure_expression_set *set);

typedef struct {
    bool is_ok;
    union {
        char *err;
        laure_expression_t *exp;
    };
} laure_parse_result;

typedef struct {
    bool is_ok;
    union {
        char *err;
        laure_expression_set *exps;
    };
} laure_parse_many_result;

laure_parse_result laure_parse(char *query);
laure_parse_many_result laure_parse_many(const string query_, char divisor, laure_expression_set *linked_opt);
bool laure_parser_needs_continuation(char *query);
bool laure_is_silent(control_ctx *cctx);

/* =-----------=
    String
=-----------= */

size_t laure_string_strlen(const char *s);
size_t laure_string_substrlen(const char *s, size_t n);
bool laure_string_is_char(const char *s);
int laure_string_put_len(int c_);
int laure_string_put_char_bare(char *_dst, int _ch);
int laure_string_get_char(char **_s);
int laure_string_put_char(char *dst, int c);
int laure_string_peek_char(const char *s);
size_t laure_string_len_char(const char *_s);
int laure_string_getc(int (*fn)(), void *p);
int laure_string_char_at_pos(const char *buff, size_t buff_len, size_t i);
size_t laure_string_offset_at_pos(const char *buff, size_t buff_len, size_t i);

typedef struct pattern_element {
    int c;
    short any_count;
} pattern_element;

int laure_string_pattern_parse(char s[], pattern_element *pattern[]);
bool laure_string_pattern_match(char *s, char *p);

#ifndef DISABLE_COLORING
#define RED_COLOR "\033[31;1m"
#define GREEN_COLOR "\033[32;1m"
#define YELLOW_COLOR "\033[33;1m"
#define GRAY_COLOR "\033[90;1m"
#define NO_COLOR "\033[0m"
#define BOLD_WHITE "\033[37;1m"
#define GREEN_BACK "\033[48;5;36m"
#define RED_BACK "\033[41;1m"
#define LAURUS_NOBILIS "\033[38;5;113m"
#define BOLD_DEC "\e[1m"
#else
#define RED_COLOR ""
#define GREEN_COLOR ""
#define YELLOW_COLOR ""
#define GRAY_COLOR ""
#define NO_COLOR ""
#define BOLD_WHITE ""
#define RED_BACK ""
#define LAURUS_NOBILIS ""
#define BOLD_DEC ""
#endif
#define lastc(s) s[strlen(s)-1]
#define colored(s) YELLOW_COLOR, s, NO_COLOR

/* =-----------=
Miscellaneous
=-----------= */

#ifndef lib_path
#define lib_path "lib"
#endif

#ifndef PROMPT
#define PROMPT "?- "
#endif

void print_header(string header, uint sz);
void strrev_via_swap(string s);

#ifdef DEBUG
#define debug(...) do { printf("DEBUG: "); printf(__VA_ARGS__); } while (0)
#else
#define debug(...) {}
#endif

/* =-----------=
   Session
=-----------= */

#define DFLAG_MAX 32

extern uint DFLAG_N;
extern char DFLAGS[DFLAG_MAX][2][32];
extern char* EXPT_NAMES[];
extern char* IMG_NAMES[];
extern char* MOCK_NAME;

char *get_dflag(char *flagname);
void add_dflag(char *flagname, char *value);

#define included_fp_max 128

typedef struct {
    laure_scope_t *scope;
    char *_included_filepaths[included_fp_max];
} laure_session_t;

laure_session_t *laure_session_new();

extern uint               LAURE_TIMEOUT;
extern clock_t            LAURE_CLOCK;
extern char              *LAURE_INTERPRETER_PATH;
extern char              *LAURE_CURRENT_ADDRESS;
extern char              *LAURE_DOC_BUFF;
extern short int          LAURE_ASK_IGNORE;
extern bool               LAURE_WS;

extern Instance *CHAR_PTR;
extern struct Translator 
        *INT_TRANSLATOR, 
        *CHAR_TRANSLATOR, 
        *STRING_TRANSLATOR,
        *ARRAY_TRANSLATOR,
        *ATOM_TRANSLATOR;

char *get_dflag(char *flagname);
void add_dflag(char *flagname, char *value);

/* =-----------=
   Querying
=-----------= */

typedef struct laure_qresp qresp;
typedef struct laure_vpk var_process_kit;

qresp laure_eval(control_ctx *cctx, laure_expression_t *e, laure_expression_set *expset);
void laure_register_builtins(laure_session_t*);
void laure_set_translators();

var_process_kit *laure_vpk_create(laure_expression_set *expset);
void laure_vpk_free(var_process_kit*);
qresp laure_showcast(control_ctx *cctx);
qresp laure_send(control_ctx *cctx);

void laure_init_name_buffs();
string laure_get_argn(uint idx);
string laure_get_respn();
string laure_get_contn();

/* =-----------=
   Consulting
=-----------= */
typedef struct apply_result apply_result_t;

void laure_consult_recursive(laure_session_t *session, string path, int *failed);
apply_result_t laure_apply(laure_session_t *session, string fact);
apply_result_t laure_consult_predicate(
    laure_session_t *session, 
    laure_scope_t *scope, 
    laure_expression_t *predicate_exp, 
    string address
);
void *laure_apply_pred(laure_expression_t *predicate_exp, laure_scope_t *scope);
int laure_load_shared(laure_session_t *session, char *path);

/* =--------=
    Error
=--------= */
typedef enum laure_error_kind {
    syntaxic_err,
    type_err,
    too_broad_err,
    undefined_err,
    internal_err,
    access_err,
    instance_err,
    signature_err,
    runtime_err,
} laure_error_kind;

typedef struct laure_error {
    laure_error_kind kind;
    char *msg;
    laure_expression_t *reason;
} laure_error;

laure_error *laure_error_create(
    laure_error_kind k, 
    char *msg, 
    laure_expression_t *reason
);
void laure_error_free(laure_error *err);
void laure_error_write(
    laure_error *err, 
    string buff, 
    size_t buff_sz
);

extern laure_error *LAURE_ACTIVE_ERROR;

#define SINGLE_DOCMARK (char*)0x2
#define BACKTRACE_CHAIN_CAPACITY 100

struct chain_p {
    laure_expression_t *e;
    uint times;
};

typedef struct laure_backtrace {
    string log;
    struct chain_p chain[BACKTRACE_CHAIN_CAPACITY];
    uint cursor;
} laure_backtrace;

extern laure_backtrace *LAURE_BACKTRACE;

laure_backtrace laure_backtrace_new();
void laure_backtrace_add(laure_backtrace *backtrace, laure_expression_t *e);
void laure_backtrace_print(laure_backtrace *backtrace);
void laure_backtrace_nullify(laure_backtrace *backtrace);

/* =--------=
WS (weighted search)
=--------= */

#ifndef DISABLE_WS

typedef struct laure_ws laure_ws;
typedef unsigned long weight_t;
typedef float optimality_t;

laure_ws *laure_ws_create(laure_ws *next);
laure_ws *laure_ws_next(laure_ws *ws);
void laure_ws_free(laure_ws *ws);
optimality_t laure_accuracy_count(laure_ws *ws);

void laure_push_transistion(laure_ws *ws, optimality_t acc);
size_t laure_count_transistions(laure_ws *ws);
void laure_restore_transistions(laure_ws *ws, size_t to_sz);

// math
optimality_t laure_ws_soften(optimality_t o);

#endif

#endif