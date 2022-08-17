#include "standard.h"

laure_expression_t *CALL = NULL;

void ensure_call() {
    if (! CALL) {
        laure_expression_t *v = laure_expression_create(let_var, NULL, false, laure_get_argn(0), false, NULL, "");
        laure_expression_t *r = laure_expression_create(let_var, NULL, false, laure_get_respn(), 0, NULL, "");

        laure_expression_set *set = laure_expression_set_link(
            laure_expression_set_link(NULL, v), r
        );
        CALL = laure_alloc(sizeof(laure_expression_t));
        CALL->t = let_pred_call;
        CALL->s = laure_scope_generate_unique_name();
        CALL->ba = laure_bodyargs_create(set, 1, true);
        CALL->docstring = NULL;
        CALL->flag = 0;
        CALL->flag2 = 0;
    }
}

int assertive_1to1(
    control_ctx *cctx,
    laure_scope_t *global_scope,
    Instance *ins1, 
    Instance *ins2,
    struct PredicateImage *predicate
) {
    ensure_call();
    laure_expression_set set[1];
    set->expression = CALL;
    set->next = NULL;

    Instance *predicate_ins = instance_new(strdup(CALL->s), MARKER_NODELETE, predicate);
    predicate_ins->repr = predicate_repr;

    qcontext nqctx = qcontext_temp(NULL, set, NULL);
    laure_scope_t *scope = laure_scope_new(global_scope, NULL);
    laure_scope_insert(scope, predicate_ins);
    ins1->name = laure_get_argn(0);
    ins2->name = laure_get_respn();
    laure_scope_insert(scope, ins1);
    laure_scope_insert(scope, ins2);

    control_ctx occtx = *cctx;
    cctx->scope = scope;
    cctx->qctx = &nqctx;
    
    qresp response = laure_start(cctx, set);
    *cctx = occtx;
    laure_free(scope);
    laure_free(predicate_ins);

    if (response.state == q_error)
        return -1;

    if (response.state == q_true || response.state == q_yield && response.payload == (void*)1)
        return 1;
    return 0;
}

typedef struct map_ctx {
    size_t idx, len;
    array_linked_t **write;
    array_linked_t *sequence;
    Instance *ary_el;
    control_ctx *cctx;
    control_ctx rec_control_ctx;
    struct PredicateImage *predicate;
    struct ArrayImage *final_image;
    laure_scope_t *pd_scope;
    bool dest;
} map_ctx;

void map_processor(laure_scope_t *recscope, char *_, void *ctx_) {
    map_ctx *ctx = (map_ctx*) ctx_;

    Instance *mapped = NULL;

    if (ctx->idx > 0) {
        mapped = laure_scope_find_by_key(recscope, ctx->dest ? laure_get_respn() : laure_get_argn(0), false);
        assert(mapped);
        array_linked_t *nlinked = laure_alloc(sizeof(array_linked_t));
        nlinked->data = mapped;
        nlinked->next = NULL;
        ctx->write[ctx->idx - 1] = nlinked;
        if (ctx->idx > 1) {
            ctx->write[ctx->idx - 2]->next = nlinked;
        }
    }

    if (ctx->idx == ctx->len || ! ctx->sequence) {
        Domain *d = ctx->final_image->u_data.length;
        ctx->final_image->state = I;
        ctx->final_image->i_data.length = ctx->len;
        ctx->final_image->i_data.linked = ctx->write[0];
        laure_scope_t *nscope = laure_scope_create_copy(&ctx->rec_control_ctx, ctx->pd_scope);

        qcontext temp[1];
        *temp = qcontext_temp(ctx->rec_control_ctx.qctx->next, NULL, ctx->rec_control_ctx.qctx->bag);

        control_ctx new = ctx->rec_control_ctx;
        
        new.qctx = temp;
        new.scope = nscope;
        
        qresp result = laure_start(&new, NULL);
        return;
    }

    ensure_call();
    laure_expression_set set[1];
    set->expression = CALL;
    set->next = NULL;

    Instance *predicate_ins = instance_new(CALL->s, MARKER_NODELETE, ctx->predicate);
    predicate_ins->repr = predicate_repr;

    qcontext nqctx = qcontext_temp(NULL, NULL, NULL);

    laure_scope_t *scope = laure_scope_new(recscope->glob, NULL);
    laure_scope_insert(scope, predicate_ins);

    ctx->sequence->data->name = ctx->dest ? laure_get_argn(0) : laure_get_respn();
    Instance *ins = instance_deepcopy(ctx->sequence->data->name, ctx->sequence->data);

    laure_scope_insert(scope, ins);
    Instance *result = instance_deepcopy(ctx->dest ? laure_get_respn() : laure_get_argn(0), ctx->ary_el);
    laure_scope_insert(scope, result);

    array_linked_t *l = ctx->sequence;
    
    ctx->sequence = ctx->sequence->next;
    ctx->idx++;

    var_process_kit vpk = vpk_create_scope_sender(map_processor, ctx);

    control_ctx occtx = *ctx->cctx;
    ctx->cctx->scope = scope;
    ctx->cctx->qctx = &nqctx;
    ctx->cctx->vpk = &vpk;

    qresp response = laure_start(ctx->cctx, set);
    *ctx->cctx = occtx;
    ctx->sequence = l;
    ctx->idx--;

    laure_free(scope);
    laure_free(predicate_ins);
}


DECLARE(laure_predicate_map) {
    Instance *sequence = pd_get_arg(pd, 0);
    Instance *mapping = pd_get_arg(pd, 1);
    Instance *mapped = pd->resp;

    cast_image(seq_img, struct ArrayImage) sequence->image;
    cast_image(mapping_img, struct PredicateImage) mapping->image;
    cast_image(mapped_img, struct ArrayImage) mapped->image;

    if (! instantiated(mapping))
        RESPOND_ERROR(not_implemented_err, NULL, 
        "mapping search by primitive not implemented");
    else if (instantiated(sequence) && instantiated(mapped)) {
        if (seq_img->i_data.length != mapped_img->i_data.length)
            return RESPOND_FALSE;
        
        array_linked_t *linked1 = seq_img->i_data.linked,
                       *linked2 = mapped_img->i_data.linked;
        uint len = seq_img->i_data.length;

        while (linked1 && linked2 && len) {
            int result = assertive_1to1(cctx, cctx->scope->glob, linked1->data, linked2->data, (struct PredicateImage*) mapping->image);
            if (result != 1) {
                if (result == -1) return respond(q_error, 0);
                else return False;
            }
            len--;
            linked1 = linked1->next;
            linked2 = linked2->next;
        }
        return True;
    } else if (instantiated(sequence) | instantiated(mapped)) {
        struct ArrayImage *left = seq_img, 
                          *right = mapped_img;
        
        bool linst = instantiated(sequence);

        map_ctx ctx;
        ctx.cctx = cctx;
        ctx.ary_el = left->arr_el;
        ctx.idx = 0;
        ctx.len = linst ? left->i_data.length : right->i_data.length;
        ctx.predicate = mapping_img;
        ctx.rec_control_ctx = *cctx;
        ctx.sequence = linst ? left->i_data.linked : right->i_data.linked;
        ctx.write = laure_alloc(sizeof(array_linked_t**) * ctx.len);
        ctx.final_image = linst ? right : left;
        ctx.pd_scope = pd->scope;
        ctx.dest = linst;
        map_processor(cctx->scope, NULL, &ctx);
        laure_free(ctx.write);
        return RESPOND_YIELD((void*)1);
    } else {
        return RESPOND_INSTANTIATE_FIRST(0);
    }
}
