#include "laurelang.h"

ulong  *LAURE_LINK_ID           = NULL;
uint    LAURE_TIMEOUT           = 0;
clock_t LAURE_CLOCK             = 0;
char *LAURE_INTERPRETER_PATH    = NULL;
char *LAURE_CURRENT_ADDRESS     = NULL;
char *LAURE_DOC_BUFF            = NULL;
laure_error *LAURE_ACTIVE_ERROR = NULL;
bool LAURE_WS                   = false;
bool LAURE_FLAG_NEXT_ORDERING   = false;

uint DFLAG_N = 0;
char DFLAGS[DFLAG_MAX][2][32] = {{0, 0}};

laure_backtrace *LAURE_BACKTRACE = NULL;

void init_backtrace() {
    if (! LAURE_BACKTRACE) {
        LAURE_BACKTRACE = laure_alloc(sizeof(laure_backtrace));
    }
    *LAURE_BACKTRACE = laure_backtrace_new();
}

laure_session_t *laure_session_new() {
    laure_session_t *session = laure_alloc(sizeof(laure_session_t));
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
    for (uint i = 0; i < DFLAG_N; i++) {
        if (str_eq(DFLAGS[i][0], flagname)) {
            strcpy(DFLAGS[i][1], value);
            return;
        }
    }
    strcpy(DFLAGS[DFLAG_N][0], flagname);
    strcpy(DFLAGS[DFLAG_N++][1], value);
    if (DFLAG_N >= DFLAG_MAX) {
        printf("too much dflags\n");
        exit(6);
    }
}