#ifndef PREDPUB_H
#define PREDPUB_H

#include "laurelang.h"

typedef enum {
    q_false,    // no payload
    q_true,     // no payload
    q_yield,    // succeed: 0x1, fail: 0x0
    q_error,    // error message for internal
    q_stop,     // succeed: 0x1, fail: 0x0
    q_continue, // no payload

    /* Following states can only be returned from c predicates */

    /* Bag full
    ---
    no_payload - only for bag predicate
    name - from bag predicate wrapper 
    --- */
    q_bag_full,
    /* Instantiate first 
    payload is argument idx to instantiate
    */
    q_instantiate_first,
} qresp_state;

typedef struct laure_qresp {
    qresp_state state;
    void *payload;
} qresp;

typedef struct laure_gen_resp {
    bool r;
    qresp qr;
} gen_resp;

qresp respond(qresp_state s, string e);

#define error_max_length 128

#define RESPOND_ERROR(k, exp, ...) do {\
        char _errmsg[error_max_length]; \
        snprintf(_errmsg, error_max_length, __VA_ARGS__); \
        LAURE_ACTIVE_ERROR = laure_error_create(k, strdup(_errmsg), exp); \
        return respond(q_error, (void*)1);} while (0)

#define RESPOND_TRUE            respond(q_true, NULL)
#define RESPOND_FALSE           respond(q_false, NULL)
#define RESPOND_YIELD(code)     respond(q_yield, code)
#define RESPOND_BAG_FULL        respond(q_bag_full, (void*)true)
#define RESPOND_BAG_FULL_E      respond(q_bag_full, (void*)false)

#define RESPOND_INSTANTIATE_FIRST(argument_idx) \
        respond(q_instantiate_first, (void*)argument_idx)

#define INT_ASSIGN(im, bi) do { \
        if(! int_check(im, bi)) {laure_free(bi->words); laure_free(bi); return respond(q_false, 0);} \
        im->state = I; \
        im->i_data = bi; \
        } while (0)

typedef struct laure_control_ctx control_ctx;
typedef struct laure_vpk var_process_kit;

typedef struct bigint bigint;
bool int_check(void *img, bigint *bi);

typedef struct laure_qcontext qcontext;

typedef struct Bag Bag;

qcontext qcontext_temp(qcontext *next, laure_expression_set *expset, Bag *bag);

typedef struct laure_builtin_predicate_hint {
    char *arg_types; // argname:typename splitted by ' ' (NULL if no args; typename == "_" means no typehint)
    char *resp_type; // resp type name (NULL if no resp), "_" means no typehint
} laure_builtin_predicate_hint;

typedef void (*single_proc)(laure_scope_t*, char*, void*);
typedef bool (*sender_rec)(char*, void*);
typedef qresp (*scope_rec)(struct laure_control_ctx*, void *payload);

typedef enum VPKMode {
    INTERACTIVE,
    SENDER_REPRS,
    SILENT,
    SENDSCOPE,
} vpk_mode_t;

typedef struct laure_vpk {

    vpk_mode_t mode;
    bool       do_process;
    void      *payload;

    char** tracked_vars;
    int     tracked_vars_len;
    bool    interactive_by_name;

    single_proc single_var_processor;
    sender_rec  sender_receiver;
    scope_rec scope_receiver;

} var_process_kit;

control_ctx *control_new(laure_session_t *session, laure_scope_t* scope, qcontext* qctx, var_process_kit* vpk, void* data, bool no_ambig);
qcontext *qcontext_new(laure_expression_set *expset);

var_process_kit vpk_create_scope_sender(scope_rec proc, void *payload);

apply_result_t laure_consult_predicate(laure_session_t *session, laure_scope_t *scope, laure_expression_t *predicate_exp, char *address);
qresp laure_start(control_ctx *cctx, laure_expression_set *expset);

#define MUST_BE(expr) if (! (expr)) return RESPOND_FALSE
#define MUST_BE_F(expr, freer) if (! (expr)) {freer; return RESPOND_FALSE;}

typedef struct laure_enum_atom {
    string atom;
    int enum_field;
} laure_enum_atom;

int laure_resolve_enum_atom(string, laure_enum_atom[], size_t);

#endif