!use std:server.

: ?handler(Event).
: ?start(int).

?handler(e) {
    send(e, "HTTP/1.1 200 OK");
    send(e, "\r\n\r\n");
}

?handler(e) when
    path of e = "/",
    send(e, "hello! this is index page"),
    !.

?handler(e) when
    e send path of e.

?start(port) {
    -- each event creates a choicepoint
    serve(port) = event;
    handler(event);
}

: ?main().
?main() {
    start(8080);
}