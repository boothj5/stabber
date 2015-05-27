#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "server/server.h"
#include "server/prime.h"
#include "server/verify.h"

typedef struct server_args_t {
    int port;
} StabberArgs;

int
stbbr_start(int port)
{
    return server_run(port);
}

void
stbbr_set_timeout(int seconds)
{
    verify_set_timeout(seconds);
}

int
stbbr_auth_passwd(char *password)
{
    prime_required_passwd(password);
    return 1;
}

int
stbbr_for(char *id, char *stream)
{
    prime_for(id, stream);
    return 1;
}

int
stbbr_verify_last(char *stanza)
{
    return verify_last(stanza);
}

int
stbbr_verify(char *stanza)
{
    return verify_any(stanza);
}

void
stbbr_send(char *stream)
{
    server_send(stream);
}

void
stbbr_stop(void)
{
    server_stop();
}
