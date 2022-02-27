#include "server.h"

#define parse_cases_start if (0) {}
#define parse_case_method(__data, __method_s, __method_enum, __event) else if (str_starts(__data, __method_s)) {__event->request_t = __method_enum; __data += strlen(__method_s) + 1;}
#define parse_cases_end else 

event_im *event_copy(event_im *event) {
    return event;
}

bool event_is_instantiated(event_im *e) {
    return e->slot_id >= 0;
}

bool event_equals(event_im *im1, event_im *im2) {
    return im1 == im2;
}

string event_repr(Instance *event) {
    event_im *e = (event_im*)event->image;
    string method;
    switch (e->request_t) {
        case GET: method = "GET"; break;
        case POST: method = "POST"; break;
        case PUT: method = "PUT"; break;
        case DELETE: method = "DELETE"; break;
        default: break;
    }
    char buff[128];
    snprintf(buff, 128, "(Event %s %s)", method, e->path);
    return strdup(buff);
}

qresp server_event_path(preddata *pd, control_ctx *cctx) {
    Instance *event_ins = pd_get_arg(pd, 0);
    event_im *ev = (event_im*)event_ins->image;
    Instance *path = pd->resp;

    struct ArrayImage *str = (struct ArrayImage*)path->image;

    char p[130];
    snprintf(p, 130, "\"%s\"", ev->path);

    laure_expression_t exp;
    exp.s = strdup( p );
    exp.t = let_custom;
    exp.docstring = NULL;
    exp.flag = 0;
    exp.is_header = false;
    exp.ba = NULL;

    bool res = str->translator->invoke(&exp, str, cctx->stack);
    return respond(res ? q_true : q_false, 0);
}

event_im *parse_request(string data, int slot) {
    if (! data) return NULL;
    event_im *event = malloc(sizeof(event_im));
    event->t = IMG_CUSTOM_T;
    event->translator = NULL;
    event->slot_id = slot;
    event->copy = event_copy;
    event->identifier = strdup(EVENT_IDENTIF);
    event->eq = event_equals;
    event->is_instantiated = event_is_instantiated;
    parse_cases_start
    parse_case_method(data, "GET", GET, event)
    parse_case_method(data, "POST", POST, event)
    parse_case_method(data, "PUT", PUT, event)
    parse_case_method(data, "DELETE", DELETE, event)
    parse_cases_end {
        printf("unknown method\n");
        free(event);
        return NULL;
    }
    string temp = data;
    while (temp[0] != ' ') temp++;
    if (strlen(data) - strlen(temp) > 127) {free(event); return NULL;};
    strncpy(event->path, data, strlen(data) - strlen(temp));
    return event;
}