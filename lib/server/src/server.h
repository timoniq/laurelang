#include "laurelang.h"
#include "laureimage.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define MAX_CONNECTIONS 1000
#define DEFAULT_PORT "80"
#define EVENT_IDENTIF "__server_Event"

typedef enum {
    GET,
    POST,
    PUT,
    DELETE,
} event_request_t;

typedef struct {
    ENHANCED_IMAGE_HEAD(void);
    int slot_id;
    event_request_t request_t;
    char path[128];
} event_im;

qresp server_serve(preddata*, control_ctx*);
qresp server_event_path(preddata*, control_ctx*);
qresp server_send(preddata*, control_ctx*);

event_im *parse_request(string, int);

event_im *event_copy(event_im *event);
bool event_is_instantiated(event_im *e);
bool event_equals(event_im *im1, event_im *im2);
string event_repr(Instance *event);

extern int listenfd, clients[MAX_CONNECTIONS];