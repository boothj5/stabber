#ifndef __H_SERVER
#define __H_SERVER

int server_run(int port);
void server_stop(void);

void server_wait_for(char *id);

void server_send(char *stream);

#endif
