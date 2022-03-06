#include "server.h"
#include <stdlib.h>
#include <unistd.h>

int listenfd, clients[MAX_CONNECTIONS];
listenfd = 0;

typedef struct {
    string name;
    string value; 
} header_t;

static header_t reqhdr[17] = { {"\0", "\0"} };
static int clientfd;

char *serve_err[3] = {
    "\0",
    "cannot get address. maybe port is busy",
    "listen error"
};

uint serve(const string port) {
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int adri = getaddrinfo(NULL, port, &hints, &res);
    if (adri != 0) {
        return 1;
    }
    for (p = res; p != NULL; p = p->ai_next) {
        int opt = 1;
        listenfd = socket(p->ai_family, p->ai_socktype, 0);
        if (listenfd == -1) continue;
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;
    }
    if (! p) return 1;

    freeaddrinfo(res);
    if (listen(listenfd, 1000000) != 0) return 2;
    return 0;
}

qresp server_serve(preddata *pd, control_ctx *cctx) {
    Instance *event = pd->resp;
    event_im *im = (event_im*)event->image;

    Instance *port_ins = pd_get_arg(pd, 0);
    if (! instantiated(port_ins)) {
        printf("server_serve: port is being set to default %s", DEFAULT_PORT);
        port_ins->image = integer_i_new(atoi(DEFAULT_PORT));
    }

    char port[28];
    bigint_write(port, 28, ((struct IntImage*)port_ins->image)->i_data);

    struct sockaddr_in clientaddr;
    socklen_t addrlen;
    char c;
    
    int slot = 0;

    int i;
    for (i = 0; i < MAX_CONNECTIONS; i++)
        clients[i] = -1;

    uint res = serve(port);
    if (res != 0)
        return respond(q_error, strdup( serve_err[res] ));

    printf("  Server running %shttp://127.0.0.1:%s%s\n", GREEN_COLOR, port, NO_COLOR);
    shutdown(clientfd, SHUT_RDWR);
    close(clientfd);

    while (true) {
        addrlen = sizeof(clientaddr);
        clients[slot] = accept(listenfd, (struct sockaddr*) &clientaddr, &addrlen);
        if (clients[slot] == -1) {
            printf("ACCEPT error\n");
        } else {
            int rcvd, fd, bytes_read;
            char *ptr;

            char *buf;
            buf = malloc(65535);
            rcvd = recv(clients[slot], buf, 65535, 0);

            if (rcvd > 0) {
                buf[rcvd] = 0;
                event_im *im = parse_request(buf, slot);
                if (! im) continue;
                event->image = im;

                laure_stack_t *nstack = laure_stack_clone(cctx->stack, true);
                control_ctx *ncctx = control_new(nstack, cctx->qctx, cctx->vpk, cctx->data, false);

                qresp result = laure_eval(ncctx, ncctx->qctx->expset);
                laure_gc_mark(cctx->stack);
                laure_gc_destroy();

                if (result.error) {
                    free(buf);
                    return result;
                }

                free(ncctx);
                laure_stack_free(nstack);
            }

            free(buf);
            shutdown(clientfd, SHUT_RDWR);
            close(clientfd);
            clients[slot] = -1;
        }
        while (clients[slot] != -1) slot = (slot + 1) % MAX_CONNECTIONS;
    }
    return respond(q_yield, 0);
}
