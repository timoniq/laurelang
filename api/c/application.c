#include "api.h"

int receiver(laure_api_session *session, char *json, void *payload) {
    printf("%s\n", json);
    return 0;
}

int main(int argc, char *argv[]) {
    laure_api_session session = laure_api_session_new();
    laure_api_initialize_buffers();
    laure_api_set_log_level(&session, DEBUG);
    laure_api_session_prepare(&session);
    laure_api_consultion_result result = laure_api_consult_path(&session, argv[1]);
    int flags[LLAPI_FLAGS_MAX];
    laure_api_flags_zeros(flags);
    flags[LLAPI_FLAG_TYPE] = 1;
    laure_api_query(&session, "message(x), n ~ int, x = repr(n), fac(n) = y", receiver, NULL, flags);
    return 0;
}