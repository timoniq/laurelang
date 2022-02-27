#include "server.h"
#include "stdio.h"
#include <fcntl.h>
#include <unistd.h>

qresp server_send(preddata *pd, control_ctx *cctx) {
    Instance *event = pd_get_arg(pd, 0);
    Instance *data  = pd_get_arg(pd, 1);

    event_im *e = (event_im*)event->image;
    struct ArrayImage *str = (struct ArrayImage*)data->image;
    if (e->slot_id < 0) return respond(q_false, 0);

    char buf[65535];
    memset(buf, 0, 65535);
    uint len = 0;
    bool esc = false;

    assert(str->state == I);

    for (int i = 0; i < str->i_data.length; i++) {
        Instance *chins = str->i_data.array[i];
        struct CharImage *chim = (struct CharImage*)chins->image;
        if (esc) {
            esc = false;
            char buff[5];
            laure_string_put_char(buff, chim->c);
            char escaped = convert_escaped_char(buff[0]);
            buf[len] = escaped;
            len++;
            continue;
        } else if (chim->c == '\\') {
            esc = true;
            continue;
        }
        len += laure_string_put_char(buf + len, chim->c);
        if (len >= 65531) {
            printf("[!] data overflow\n");
            break;
        }
    }

    int clientfd = clients[e->slot_id];
    write(clientfd, buf, strlen(buf));
    return respond(q_true, 0);
}