#include <string.h>
#include <glib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "server/xmppclient.h"

#define STREAM_REQ "<?xml version=\"1.0\"?><stream:stream to=\"localhost\" xml:lang=\"en\" version=\"1.0\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\">"
#define STREAM_RESP  "<?xml version=\"1.0\"?><stream:stream from=\"localhost\" id=\"stream1\" xml:lang=\"en\" version=\"1.0\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\">"
#define FEATURES "<stream:features></stream:features>"
#define AUTH_REQ "<iq id=\"_xmpp_auth1\" type=\"set\"><query xmlns=\"jabber:iq:auth\"><username>stabber</username><password>password</password><resource>profanity</resource></query></iq>"
#define AUTH_RESP "<iq id=\"_xmpp_auth1\" type=\"result\"/>"
#define END_STREAM "</stream:stream>"

void connection_handler(XMPPClient *client)
{
    int read_size;

    // client loop
    while(1) {
        char buf[2];
        memset(buf, 0, sizeof(buf));

        // wait for stream
        GString *stream = g_string_new("");
        errno = 0;
        gboolean received = FALSE;
        while ((!received) && ((read_size = recv(client->sock, buf, 1, 0)) > 0)) {
            g_string_append_len(stream, buf, read_size);
            if (g_strcmp0(stream->str, STREAM_REQ) == 0) {
                received = TRUE;
            }
            memset(buf, 0, sizeof(buf));
        }

        // error
        if (read_size == -1) {
            perror("Error receiving on connection");
            xmppclient_end_session(client);
            g_string_free(stream, TRUE);
            break;

        // client closed
        } else if (read_size == 0) {
            printf("\n%s:%d - Client disconnected.\n", client->ip, client->port);
            xmppclient_end_session(client);
            g_string_free(stream, TRUE);
            break;

        } else {
            printf("RECV: %s\n", STREAM_REQ);
            fflush(stdout);
            g_string_free(stream, TRUE);

            // send stream response
            int sent = 0;
            int to_send = strlen(STREAM_RESP);
            char *marker = STREAM_RESP;
            while (to_send > 0 && ((sent = write(client->sock, marker, to_send)) > 0)) {
                to_send -= sent;
                marker += sent;
            }

            printf("SENT: %s\n", STREAM_RESP);
            fflush(stdout);

            memset(buf, 0, sizeof(buf));

            // send features
            sent = 0;
            to_send = strlen(FEATURES);
            marker = FEATURES;
            while (to_send > 0 && ((sent = write(client->sock, marker, to_send)) > 0)) {
                to_send -= sent;
                marker += sent;
            }

            printf("SENT: %s\n", FEATURES);
            fflush(stdout);

            memset(buf, 0, sizeof(buf));

            // wait for auth request
            stream = g_string_new("");
            errno = 0;
            gboolean received = FALSE;
            while ((!received) && ((read_size = recv(client->sock, buf, 1, 0)) > 0)) {
                g_string_append_len(stream, buf, read_size);
                if (g_strcmp0(stream->str, AUTH_REQ) == 0) {
                    received = TRUE;
                }
                memset(buf, 0, sizeof(buf));
            }

            // error
            if (read_size == -1) {
                perror("Error receiving on connection");
                xmppclient_end_session(client);
                g_string_free(stream, TRUE);
                break;

            // client closed
            } else if (read_size == 0) {
                printf("\n%s:%d - Client disconnected.\n", client->ip, client->port);
                xmppclient_end_session(client);
                g_string_free(stream, TRUE);
                break;

            } else {
                printf("RECV: %s\n", AUTH_REQ);
                fflush(stdout);
                g_string_free(stream, TRUE);

                // send auth response
                sent = 0;
                to_send = strlen(AUTH_RESP);
                marker = AUTH_RESP;
                while (to_send > 0 && ((sent = write(client->sock, marker, to_send)) > 0)) {
                    to_send -= sent;
                    marker += sent;
                }

                printf("SENT: %s\n", AUTH_RESP);
                fflush(stdout);

                memset(buf, 0, sizeof(buf));

                // wait until stream closed
                stream = g_string_new("");
                errno = 0;
                gboolean received = FALSE;
                while ((!received) && ((read_size = recv(client->sock, buf, 1, 0)) > 0)) {
                    printf("%c", buf[0]);
                    fflush(stdout);
                    g_string_append_len(stream, buf, read_size);
                    if (g_str_has_suffix(stream->str, END_STREAM)) {
                        received = TRUE;
                    }
                    memset(buf, 0, sizeof(buf));
                }

                // error
                if (read_size == -1) {
                    perror("Error receiving on connection");
                    xmppclient_end_session(client);
                    g_string_free(stream, TRUE);
                    break;

                // client closed
                } else if (read_size == 0) {
                    printf("\n%s:%d - Client disconnected.\n", client->ip, client->port);
                    xmppclient_end_session(client);
                    g_string_free(stream, TRUE);
                    break;

                } else {
                    printf("\nEnd stream receieved");
                    xmppclient_end_session(client);
                    break;
                }
            }
        }
    }
}

int main(int argc , char *argv[])
{
    int port = 0;

    GOptionEntry entries[] =
    {
        { "port", 'p', 0, G_OPTION_ARG_INT, &port, "Listen port", NULL },
        { NULL }
    };

    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("%s\n", error->message);
        g_option_context_free(context);
        g_error_free(error);
        return 1;
    }

    g_option_context_free(context);

    if (port == 0) {
        port = 5222;
    }

    int listen_socket, client_socket;
    int c, ret;
    struct sockaddr_in server_addr, client_addr;

    if (argc == 2) {
        port = atoi(argv[1]);
    }

    printf("Starting on port: %d...\n", port);

    // create socket
    errno = 0;
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // ipv4, tcp, ip
    if (listen_socket == -1) {
        perror("Could not create socket");
        return 0;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // bind socket to port
    errno = 0;
    ret = bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        perror("Bind failed");
        return 0;
    }

    // set socket to listen mode
    errno = 0;
    ret = listen(listen_socket, 5);
    if (ret == -1) {
        perror("Listen failed");
        return 0;
    }
    puts("Waiting for incoming connections...");

    // connection accept
    c = sizeof(struct sockaddr_in);
    errno = 0;
    client_socket = accept(listen_socket, (struct sockaddr *)&client_addr, (socklen_t*)&c);
    if (client_socket == -1) {
        perror("Accept failed");
        return 0;
    }

    XMPPClient *client = xmppclient_new(client_addr, client_socket);

    connection_handler(client);
    return 0;
}
