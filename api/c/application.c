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
    printf("Consult result: %s\n", result.t == 0 ? "ok" : "failed");
    laure_api_query(&session, "x + 1 = 10", receiver, NULL, 0);
    return 0;
}