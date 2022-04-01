#include "laurelang.h"

ulong  *LAURE_LINK_ID          = NULL;
uint    LAURE_TIMEOUT          = 0;
clock_t LAURE_CLOCK            = 0;
char *LAURE_INTERPRETER_PATH   = NULL;
char *LAURE_CURRENT_ADDRESS    = NULL;
char *LAURE_DOC_BUFF           = NULL;

uint DFLAG_N = 0;
char DFLAGS[DFLAG_MAX][2][32] = {{0, 0}};

laure_session_t *laure_session_new() {
    laure_session_t *session = malloc(sizeof(laure_session_t));
    session->scope = laure_scope_create_global();
    memset(session->_included_filepaths, 0, included_fp_max * sizeof(void*));
    return session;
}

string get_dflag(string flagname) {
    for (uint idx = 0; idx < DFLAG_N; idx++) {
        string f = DFLAGS[idx][0];
        if (str_eq(f, flagname)) return DFLAGS[idx][1];
    }
    return NULL;
}

void add_dflag(string flagname, string value) {
    strcpy(DFLAGS[DFLAG_N][0], flagname);
    strcpy(DFLAGS[DFLAG_N++][1], value);
    if (DFLAG_N >= DFLAG_MAX) {
        printf("too much dflags\n");
        exit(6);
    }
}