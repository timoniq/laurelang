#include "server.h"
#include <builtin.h>

int package_include(laure_session_t *session) {
    LAURE_SESSION = session;

    // Create Event
    Cell cell; cell.link_id = laure_stack_get_uid(session->stack);
    event_im *im = malloc(sizeof(event_im));
    im->t = IMG_CUSTOM_T;
    im->translator = NULL;
    im->request_t = GET;
    im->copy = event_copy;
    im->identifier = strdup(EVENT_IDENTIF);
    im->eq = event_equals;
    im->is_instantiated = event_is_instantiated;
    im->slot_id = -1;
    cell.instance = instance_new(strdup("Event"), NULL, im);
    cell.instance->repr = event_repr;

    laure_stack_insert(session->stack, cell);

    laure_cle_add_predicate(
        session, "serve", 
        server_serve, 
        1, "port:int", "Event", false, 
        "Gets events"
    );

    laure_cle_add_predicate(
        session, "path", 
        server_event_path, 
        1, "e:Event", "string", false, 
        "Gets path from event"
    );

    laure_cle_add_predicate(
        session, "send", 
        server_send, 
        2, "e:Event data:string", NULL, false, 
        "Send"
    );

    return 0;
}