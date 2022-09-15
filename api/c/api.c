#include "laurelang.h"
#include "laureimage.h"
#include "api.h"

#include <stdlib.h>
#include <assert.h>

#define JSON_MAX_LEN 1024

/* session_new
Creates new API session with default settings.
*/
laure_api_session laure_api_session_new() {
    laure_session_t *internal_session = laure_session_new();
    time_t current_time;
    time(&current_time);
    laure_api_session session = {
        current_time, laure_api_get_default_settings(), internal_session
    };
    return session;
}

/* get_default_settings
Creates default settings for API session.
*/
laure_api_settings laure_api_get_default_settings() {
    laure_api_settings settings = {
        stdout, // log out
        INFO,   // log level
        1,      // safe mode
    };
    return settings;
}

/* set_log_out
Sets session stream for logging.
Receives session ptr and logging stream.
*/
void laure_api_set_log_out(laure_api_session *session, FILE *log_out) {
    session->settings.log_out = log_out;
}

/* set_log_level
*/
void laure_api_set_log_level(laure_api_session *session, laure_api_log_level level) {
    session->settings.log_level = level;
}

/* log
Cretes log if session allows given level.
*/
void laure_api_log(laure_api_session *session, laure_api_log_level min_level, string msg) {
    if (min_level >= session->settings.log_level) {
        fprintf(session->settings.log_out, "[laurelang_api] %s\n", msg);
    }
}

laure_api_consultion_result laure_api_cr_ok() {
    laure_api_consultion_result cr;
    cr.t = api_cr_ok;
    cr.payload = NULL;
    return cr;
}

laure_api_consultion_result laure_api_cr_error(laure_api_consultion_result_t t, string payload) {
    laure_api_consultion_result cr = {t, payload};
    return cr;
}

/* consult_path
Consults instances from laurelang source from given path.
Path can me either folder (module) or single source.
*/
laure_api_consultion_result laure_api_consult_path(laure_api_session *session, string path) {
    int result[1] = {0};
    laure_api_logf(session, DEBUG, "running consult %s", path);
    laure_consult_recursive((laure_session_t*)session->session_ptr, path, result);
    if (*result != 0) {
        return laure_api_cr_error(api_cr_name_undef, NULL);
    }
    return laure_api_cr_ok();
}

/* initialize_buffers
Initializes global abstract machine buffers
*/
void laure_api_initialize_buffers() {
    laure_set_translators();
    laure_init_name_buffs();
}

/* session_prepare
Prepares session for querying: adds default instances,
loads standard library if no_library isn't set
*/
void laure_api_session_prepare(laure_api_session *session) {
    laure_api_log(session, DEBUG, "initializing session");
    laure_register_builtins((laure_session_t*)session->session_ptr);
    laure_api_consultion_result result = laure_api_load_library(session);
    if (result.t != api_cr_ok) {
        laure_api_log(session, ERROR, "cannot load library");
    }
}

/* load_library
Loads library into session
*/
laure_api_consultion_result laure_api_load_library(laure_api_session *session) {
    if (session->settings.no_library) {
        return laure_api_cr_ok();
    }
    char path[PATH_MAX];
    realpath(lib_path, path);
    return laure_api_consult_path(session, path);
}

struct laure_api_query_receiver_context {
    laure_api_session *session;
    laure_api_query_receiver_t json_receiver;
    void *payload;
};

qresp laure_api_query_scope_receiver(control_ctx *cctx, void *ctx__) {
    laure_scope_t *scope = cctx->scope;
    struct laure_api_query_receiver_context *ctx = (struct laure_api_query_receiver_context*)ctx__;

    char json[JSON_MAX_LEN];
    int cnt = 1022;

    strcpy(json, "{");
    for (int i = 0; i < cctx->vpk->tracked_vars_len; i++) {
        Instance *ins = laure_scope_find_by_key(scope, cctx->vpk->tracked_vars[i], false);
        int c = strlen(cctx->vpk->tracked_vars[i]);
        if (cnt - c <= 1)
            break;
        
        if (ins) {
            string repr = ins->repr(ins);
            c += strlen(repr);
            if (cnt - c <= 1) {
                free(repr);
                break;
            }
            strcat(json, "\"");
            strcat(json, ins->name);
            strcat(json, "\":{\"data\":\"");
            
            strcat(json, repr);
            strcat(json, "\"}");
            free(repr);
        }
    }
    strcat(json, "}");

    int result = ctx->json_receiver(ctx->session, json, ctx->payload);
    if (result == 0)
        return respond(q_continue, NULL);
    else
        return respond(q_stop, NULL);
}

laure_api_query_result laure_api_query(
    laure_api_session *session,
    string query,
    laure_api_query_receiver_t json_receiver,
    void *payload,
    laure_api_query_flag flags[LLAPI_FLAGS_MAX]
) {
    laure_parse_many_result parse_result = laure_parse_many(query, ',', NULL);
    if (! parse_result.is_ok) {
        laure_api_logf(session, ERROR, "failed to parse query `%s`: %s", query, parse_result.err);
        laure_api_query_result lqr = {api_qr_error, NULL};
        return lqr;
    }
    laure_expression_set *set = laure_expression_compose(parse_result.exps);
    qcontext *qctx = qcontext_new(laure_expression_compose(set));

    struct laure_api_query_receiver_context rctx[1];
    rctx->session = session;
    rctx->json_receiver = json_receiver;
    rctx->payload = payload;

    var_process_kit *vpk = laure_vpk_create(set);
    vpk->mode = SENDSCOPE;
    vpk->scope_receiver = laure_api_query_scope_receiver;
    vpk->payload = rctx;

    control_ctx *cctx = control_new(
        (laure_session_t*)session->session_ptr, 
        ((laure_session_t*)session->session_ptr)->scope, 
        qctx, vpk, NULL, false
    );

    laure_backtrace_nullify(LAURE_BACKTRACE);
    qresp response = laure_start(cctx, set);

    laure_api_query_result lqr = {api_qr_ok, NULL};
    return lqr;
}