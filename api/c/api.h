#pragma once

#include <stdio.h>
#include <time.h>

#define LLAPI_FLAG_TYPE 0

typedef enum laure_api_log_level {
    DEBUG,
    INFO,
    ERROR
} laure_api_log_level;

typedef struct laure_api_settings {
    FILE *log_out;
    laure_api_log_level log_level;
    int mode;
    int no_library;
} laure_api_settings;

typedef struct laure_api_session {
    time_t created_at;
    laure_api_settings settings;
    void *session_ptr;
} laure_api_session;

laure_api_session laure_api_session_new();
void laure_api_session_prepare(laure_api_session *session);

void laure_api_set_log_out(laure_api_session *session, FILE *log_out);
void laure_api_set_log_level(laure_api_session *session, laure_api_log_level level);
void laure_api_log(laure_api_session *session, laure_api_log_level min_level, char *msg);

#define laure_api_logf(session, min_level, ...) do { if (min_level >= session->settings.log_level) { \
            char buff[128]; snprintf(buff, 128, __VA_ARGS__); \
            laure_api_log(session, min_level, buff); \
        } } while (0)

laure_api_settings laure_api_get_default_settings();

typedef enum laure_api_consultion_result_t {
    api_cr_ok,
    api_cr_name_undef,
    api_cr_memory_err,
} laure_api_consultion_result_t;

typedef struct laure_api_consultion_result {
    laure_api_consultion_result_t t;
    char *payload;
} laure_api_consultion_result;

laure_api_consultion_result laure_api_cr_ok();
laure_api_consultion_result laure_api_cr_error(laure_api_consultion_result_t t, char *payload);

laure_api_consultion_result laure_api_consult_path(laure_api_session *session, char *path);
void laure_api_initialize_buffers();
laure_api_consultion_result laure_api_load_library(laure_api_session *session);

typedef enum laure_api_query_result_t {
    api_qr_ok,
    api_qr_error
} laure_api_query_result_t;

typedef struct laure_api_query_result {
    laure_api_query_result_t t;
    char *payload;
} laure_api_query_result;

#define LLAPI_FLAGS_MAX 16

typedef int (*laure_api_query_receiver_t)(laure_api_session*, char*, void*);

laure_api_query_result laure_api_query(
    laure_api_session *session,
    char *query,
    laure_api_query_receiver_t json_receiver,
    void *payload,
    int *flags
);

void laure_api_flags_zeros(int *flags);