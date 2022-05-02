#include "laurelang.h"

laure_error *laure_error_create(
    laure_error_kind k, 
    char *msg, 
    laure_expression_t *reason
) {
    laure_error *err = malloc(sizeof(laure_error));
    err->kind = k;
    err->msg = msg;
    err->reason = reason;
    return err;
}

void laure_error_free(laure_error *err) {
    free(err->msg);
    free(err);
}

string laure_error_msg(laure_error *err) {
    char buff[128];
    switch(err->kind) {
        case syntaxic_err: {
            strcpy(buff, "syntax");
            break;
        }
        case type_err: {
            strcpy(buff, "type");
            break;
        }
        case too_broad_err: {
            strcpy(buff, "too_broad");
            break;
        }
        case undefined_err: {
            strcpy(buff, "undefined");
            break;
        }
        case internal_err: {
            strcpy(buff, "internal");
            break;
        }
        case access_err: {
            strcpy(buff, "access");
            break;
        }
        case instance_err: {
            strcpy(buff, "instance");
            break;
        }
        case signature_err: {
            strcpy(buff, "signature");
            break;
        }
    }
    strcat(buff, "( ");
    strcat(buff, RED_COLOR);
    strncat(buff, err->msg, 100);
    strcat(buff, NO_COLOR);
    strcat(buff, " )");
    return strdup(buff);
}

void laure_error_write(
    laure_error *err, 
    string buff, 
    size_t buff_sz
) {
    uint lnpos = err->reason->linepos > 127 ? 127 : err->reason->linepos;
    char place_pointer[128];
    memset(place_pointer, ' ', 128);
    place_pointer[lnpos] = 0;

    string msg = laure_error_msg(err);

    snprintf(
        buff,
        buff_sz, 
        "  %s\n  %s%s^%s error%s: %s", 
        err->reason->fullstring, 
        place_pointer, 
        GREEN_COLOR, RED_COLOR, NO_COLOR,
        msg
    );

    free(msg);
}