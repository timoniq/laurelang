#ifndef LAURELANG_H
#define LAURELANG_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <time.h>


typedef unsigned int uint;
typedef enum {false, true} bool;

typedef enum {
    let_set,
    let_var,
    let_pred_call,
    let_decl,
    let_assert,
    let_image,
    let_pred,
    let_choice_1,
    let_choice_2,
    let_name,
    let_custom,
    let_constraint,
    let_struct_def,
    let_struct,
    let_array,
    let_unify,
    let_quant,
    let_domain,
    let_imply,
    let_cut,
    let_nope
} laure_expression_type;

struct laure_expression_set_;

typedef struct _Instance {
    char             * name;
    struct _Instance * derived;
    bool               locked;
    char*           (* repr ) (struct _Instance*);
    char             * doc;
    void             * image;
    // GARBAGE COLLECTION
    bool mark;
} Instance;

typedef struct Cell {
    long link_id;
    Instance *instance;
} Cell;

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
    char *docstring;
    bool is_header;

    char *s;
    uint flag; // nesting for let_var
    laure_expression_compact_bodyargs *ba;
} laure_expression_t;

typedef struct laure_expression_set_ {
    laure_expression_t *expression;
    struct laure_expression_set_ *next;
} laure_expression_set;

#ifndef FEATURE_LINKED_SCOPE

#define SCOPE_SIZE 2000
#define SCOPE_MAX_COUNT 2000

typedef struct {
    Cell *cells;
    int   count;
    unsigned short id;
} Scope;

typedef struct Stack {
    long *next_link_id;
    Scope current;
    struct Stack* next;
    struct Stack* global;
    uint repeat;
} laure_stack_t;

#define STACK_ITER(stack, cell_, body, allow_null) do { \
            if (! stack->current.cells) break; \
            for (int i = 0; i < stack->current.count; i++) { \
                Cell __cell = stack->current.cells[i]; \
                if (__cell.instance == NULL) continue; \
                cell_ = __cell; \
                body; \
            } \
        } while (0)

#else

typedef struct LinkedCell_ {
    Cell cell;
    struct LinkedCell_ *link;
} LinkedCell;

typedef struct {
    LinkedCell *cells;
    unsigned short id;
} Scope;

typedef struct Stack {
    Scope current;
    long *next_link_id;
    
    struct Stack* next;
    struct Stack* global;

    // for temp stack
    bool           is_temp;
    struct Stack *main;
    uint repeat;

} laure_stack_t;

#define STACK_ITER(stack, cell_, body, allow_null) do { \
            LinkedCell *lcell = stack->current.cells; \
            while (lcell != NULL) { \
                cell_ = lcell->cell; \
                if (cell_.instance == NULL || (!allow_null && !cell_.instance->image)) {lcell = lcell->link; continue;} \
                body; \
                lcell = lcell->link; \
            } \
        } while (0)

#endif

void laure_gc_track_instance(Instance *instance);
void laure_gc_track_image(void *image);
void laure_gc_mark_image(void *image);
void laure_gc_mark_instance(Instance *instance);
void laure_gc_mark(laure_stack_t *reachable);
void laure_image_destroy(void *img);
uint laure_gc_destroy();
uint laure_gc_run(laure_stack_t *reachable);

#define laure_included_fp_max 512

typedef struct Session {
    laure_stack_t *stack;
    short int signal; // signals // -1 false; 0 idle; 1 true
    char *_doc_buffer;
    char *_included_filepaths[laure_included_fp_max];
} laure_session_t;

#define LAURE_TRACE_LIMIT 10

typedef struct _laure_trace_linked {
    char               *data;
    char               *address;
    struct _laure_trace_linked *prev;
    struct _laure_trace_linked *next;
} laure_trace_linked;

typedef struct {
    laure_trace_linked *linked;
    int8_t length;
    bool   active;
    char  *comment;
} laure_trace;

extern laure_trace       *LAURE_TRACE;
extern laure_session_t   *LAURE_SESSION;
extern char              *LAURE_CURRENT_ADDRESS;
extern char              *LAURE_INTERPRETER_PATH;
extern uint               LAURE_GC_COLLECTED;
extern uint               LAURE_RECURSION_DEPTH;

extern uint               LAURE_TIMEOUT;
extern clock_t            LAURE_CLOCK;

#ifndef LAURE_RECURSION_DEPTH_LIMIT
#define LAURE_RECURSION_DEPTH_LIMIT 350
#endif

#define COND_TIMEOUT (LAURE_TIMEOUT != 0 && ((((double)(clock() - LAURE_CLOCK)) / CLOCKS_PER_SEC) >= LAURE_TIMEOUT))

laure_expression_set *laure_expression_set_link(laure_expression_set *root, laure_expression_t *new_link);
uint laure_expression_get_count(laure_expression_set *root);
void laure_expression_destroy(laure_expression_t *expression);
void laure_expression_set_destroy(laure_expression_set *root);
laure_expression_t *laure_expression_set_get_by_idx(laure_expression_set *root, uint idx);
laure_expression_compact_bodyargs *laure_bodyargs_create(laure_expression_set *set, uint body_len, bool has_resp);
void laure_expression_show(laure_expression_t *exp, uint indent);
laure_expression_t *laure_expression_create(laure_expression_type t, char *docstring, bool is_header, char *s, uint flag, laure_expression_compact_bodyargs *ba);
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
bool laure_parser_needs_continuation(char *query);
bool laure_is_silent(void *cctx);

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

laure_session_t *laure_session_new();
void laure_trace_init();
laure_trace_linked *laure_trace_last_linked();
void laure_trace_add(char *data, char *address);
void laure_trace_erase();
void laure_print_indent(int indent);
void laure_trace_print();
void laure_trace_comment(char *comment);
void laure_set_timeout(uint timeout);

laure_stack_t *laure_stack_parent();
laure_stack_t *laure_stack_init(laure_stack_t *next);
Instance *laure_stack_get(laure_stack_t *stack, char *key);
int laure_stack_delete(laure_stack_t *stack, char *key);
void laure_stack_insert(laure_stack_t *stack, Cell cell);
char *laure_stack_get_uname(laure_stack_t *stack);
long laure_stack_get_uid(laure_stack_t *stack);
laure_stack_t *laure_stack_clone(laure_stack_t *from, bool deep);
Cell laure_stack_get_cell(laure_stack_t *stack, char *key);
Cell laure_stack_get_cell_by_lid(laure_stack_t *stack, long lid, bool allow_global);
Cell laure_stack_get_cell_only_locals(laure_stack_t *stack, char *key);
void laure_stack_show(laure_stack_t *stack);
void laure_stack_free(laure_stack_t *stack);
size_t laure_stack_get_size_deep(laure_stack_t *stack);
void laure_stack_change_lid_by_lid(laure_stack_t *stack, long old, long);
void laure_stack_change_lid(laure_stack_t *stack, char *key, long lid);
void laure_stack_add_to(laure_stack_t *from, laure_stack_t *to);

typedef void (*single_proc)(laure_stack_t*, char*, void*);
typedef bool (*sender_rec)(char*, void*);

typedef struct _laure_qresp qresp;
typedef struct ControlCtx control_ctx;

qresp laure_eval(control_ctx *cctx, laure_expression_set *expression_set);
control_ctx *laure_control_ctx_get(laure_session_t *session, laure_expression_set *expset);

void laure_register_builtins(laure_session_t*);

void laure_consult_recursive(laure_session_t *session, char *path);

typedef enum VPKMode {
    INTERACTIVE,
    SENDER,
    SILENT,
} vpk_mode_t;

typedef struct VPK {

    vpk_mode_t mode;
    bool       do_process;
    void      *payload;

    char** tracked_vars;
    int     tracked_vars_len;
    bool    interactive_by_name;

    single_proc single_var_processor;
    sender_rec  sender_receiver;

} var_process_kit;

typedef struct string_linked_ {
    char *s;
    struct string_linked_ *next;
} string_linked;

string_linked *string_split(char *s, int delimiter);

struct laure_flag {
    int    id;
    char  *name;
    char  *doc;
    bool   readword;
};


#ifndef DISABLE_COLORING
#define RED_COLOR "\033[31;1m"
#define GREEN_COLOR "\033[32;1m"
#define YELLOW_COLOR "\033[33;1m"
#define GRAY_COLOR "\033[90;1m"
#define NO_COLOR "\033[0m"
#else
#define RED_COLOR ""
#define GREEN_COLOR ""
#define YELLOW_COLOR ""
#define GRAY_COLOR ""
#define NO_COLOR ""
#endif

#ifndef lib_path
#define lib_path "lib"
#endif

#define EXPSET_ITER(set_, ptr_, body) do { \
            laure_expression_set *_set = set_; \
            while (_set) { \
                ptr_ = _set->expression; \
                body; \
                _set = _set->next; \
            } \
        } while (0);

#endif
