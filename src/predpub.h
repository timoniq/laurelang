#ifndef PRED_PUB_H
#define PRED_PUB_H

#include "laurelang.h"

typedef enum {
    q_false, // 0
    q_true, // 1
    q_yield, // 2
    q_error, // 3
    q_stop, // 4
    q_continue, // 5
    q_true_s, // 6
} qresp_state;

typedef struct _laure_qresp {
    qresp_state state;
    char *error;
} qresp;


typedef struct GenResp {
    bool r;
    union {
        qresp qr;
    };
} gen_resp;

qresp respond(qresp_state s, char *e);

#define RESPOND_ERROR(msg, ...) do {\
        char _errmsg[64]; \
        snprintf(_errmsg, 64, msg, __VA_ARGS__); \
        return respond(q_error, strdup(_errmsg));} while (0)

#define RESPOND_TRUE           respond(q_true, NULL)
#define RESPOND_FALSE          respond(q_false, NULL)
#define RESPOND_YIELD          respond(q_yield, NULL)

#define BAD_QRESP(r)           (r.state == q_false || r.state == q_error)

#define ASSERT_IS_INT(gc, stack, instance) do { \
        if (!instance) { \
            void *__img = image_deepcopy(stack, laure_stack_get(stack, "int")->image); \
            instance = instance_new(strdup("__B"), NULL, __img); \
            laure_gc_treep_add(gc, GCPTR_IMAGE, __img); \
            laure_gc_treep_add(gc, GCPTR_INSTANCE, instance); \
        } \
        if (!is_int(instance)) RESPOND_ERROR("%s must be integer", instance->name); \
        } while (0)

#define ASSERT_IS_INT_ARR(instance) do { \
        if (!is_array_of_int(instance)) RESPOND_ERROR("%s must be integer array", instance->name); \
        } while (0)

#define IF_NOT_DEFINED(instance) (((Instance*)instance)->image == NULL)

#define INT_ASSIGN(im, bi) do { \
        if(! int_check(im, bi)) {free(bi->words); free(bi); return respond(q_false, 0);} \
        im->state = I; \
        im->i_data = bi; \
        } while (0)

typedef struct ControlCtx control_ctx;
typedef struct VPK var_process_kit;

bool int_check(void *img, void *bi);

typedef struct QContext {
    laure_expression_set *expset;
    struct QContext *next;
    bool constraint_mode;
    bool forbidden_ambiguation;
    bool mark;
    bool cut;
} qcontext;

struct BuiltinPredHint {
    char *arg_types; // argname:typename splitted by ' ' (NULL if no args; typename == "_" means no typehint)
    char *resp_type; // resp type name (NULL if no resp), "_" means no typehint
};

typedef enum apply_status {
    apply_error,
    apply_ok
} apply_status_t;

typedef struct apply_result {
    apply_status_t status;
    char *error;
} apply_result_t;

control_ctx *control_new(laure_stack_t* stack, qcontext* qctx, var_process_kit* vpk, void* data);
qcontext *qcontext_new(laure_expression_set *expset);

apply_result_t laure_consult_predicate(laure_session_t *session, laure_stack_t *stack, laure_expression_t *predicate_exp, char *address);

#endif