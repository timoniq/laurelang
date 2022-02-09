#include "laurelang.h"
#include "utils.h"

laure_trace *LAURE_TRACE       = NULL;
laure_session_t *LAURE_SESSION = NULL;
char *LAURE_CURRENT_ADDRESS    = NULL;
char *LAURE_INTERPRETER_PATH   = NULL;
uint LAURE_GC_COLLECTED        = 0;
uint LAURE_RECURSION_DEPTH     = 0;
laure_gc_treep_t  *GC_ROOT     = NULL;

#define TRACE_PRINT_INDENT 2

laure_session_t *laure_session_new() {
    laure_session_t *sess = (laure_session_t*)malloc(sizeof(laure_session_t));
    sess->stack = laure_stack_parent();
    sess->_doc_buffer = NULL;
    sess->signal = 0;
    memset(sess->_included_filepaths, 0, laure_included_fp_max);
    laure_trace_init();
    LAURE_SESSION = sess;
    return sess;
}

void laure_trace_init() {
    if (LAURE_TRACE != NULL) return;
    LAURE_TRACE = malloc(sizeof(laure_trace));
    LAURE_TRACE->length = 0;
    LAURE_TRACE->linked = NULL;
}

laure_trace_linked *laure_trace_last_linked() {
    if (!LAURE_TRACE || LAURE_TRACE->linked == NULL) {
        return NULL;
    }
    laure_trace_linked *l = LAURE_TRACE->linked;
    while (l->next != NULL) {
        l = l->next;
    }
    return l;
}

void laure_trace_add(string data, string address) {
    assert(LAURE_TRACE != NULL);

    laure_trace_linked *linked = malloc(sizeof(laure_trace_linked));
    linked->address = address;
    linked->data = data;
    linked->prev = laure_trace_last_linked();
    linked->next = NULL;

    if (LAURE_TRACE->length == 0) {
        LAURE_TRACE->linked = linked;
    } else {
        if (LAURE_TRACE->length == LAURE_TRACE_LIMIT) {
            if (LAURE_TRACE->linked != NULL)
                LAURE_TRACE->linked = LAURE_TRACE->linked->next;
            else
                LAURE_TRACE->length = 0;
            LAURE_TRACE->length--;
        }
        if (linked->prev != NULL)
            linked->prev->next = linked;
    }

    LAURE_TRACE->length++;
}

void laure_trace_erase() {
    assert(LAURE_TRACE != NULL);
    LAURE_TRACE->active = false;
    laure_trace_linked *l = laure_trace_last_linked();
    do {
        if (l == NULL) break;
    } while(l = l->prev);
    LAURE_TRACE->length = 0;
    LAURE_TRACE->linked = NULL;
    LAURE_TRACE->comment = NULL;
}

void laure_print_indent(int indent) {
    for (int i = 0; i < indent * TRACE_PRINT_INDENT; i++) printf(" ");
}

void laure_trace_print() {
    printf("\n%strace%s\n", RED_COLOR, NO_COLOR);

    if (LAURE_TRACE->length == 0) {
        printf("  no trace :S\n");
    } else {
        laure_trace_linked *l = LAURE_TRACE->linked;
        int indent = 1;
        do {
            if (l == NULL) break;
            laure_print_indent(indent);
            printf("In %s:\n", l->address);
            laure_print_indent(indent+1);
            //! data beautifiers
            printf("%s%s%s\n", RED_COLOR, l->data, NO_COLOR);
            indent++;

            if (l->prev == NULL && LAURE_TRACE->comment != NULL) {
                laure_print_indent(indent);
                printf("%s(%s)%s\n", RED_COLOR, LAURE_TRACE->comment, NO_COLOR);
                break;
            }
        } while (l = l->next);
    }
    printf("\n");
}

void laure_trace_comment(string comment) {
    LAURE_TRACE->comment = comment;
}