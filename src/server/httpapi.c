/*
 * httpapi.c
 *
 * Copyright (C) 2015 James Booth <boothj5@gmail.com>
 *
 * This file is part of Stabber.
 *
 * Stabber is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Stabber is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Stabber.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef PLATFORM_OSX
#include <pthread.h>
#else
#include <sys/prctl.h>
#endif

#include <stdint.h>
#include <sys/socket.h>
#include <microhttpd.h>
#include <glib.h>

#include "server/log.h"
#include "server/server.h"
#include "server/prime.h"
#include "server/verify.h"

struct MHD_Daemon *httpdaemmon = NULL;

#define POSTBUFFERSIZE 2048

typedef enum {
    STBBR_OP_UNKNOWN,
    STBBR_OP_SEND,
    STBBR_OP_FOR,
    STBBR_OP_VERIFY
} stbbr_op_t;

typedef struct conn_info_t {
    stbbr_op_t stbbr_op;
    GString *body;
} ConnectionInfo;

ConnectionInfo*
create_connection_info(void)
{
    ConnectionInfo *con_info = malloc(sizeof(ConnectionInfo));
    con_info->body = g_string_new("");

    return con_info;
}

void
destroy_connection_info(ConnectionInfo *con_info)
{
    if (con_info->body) {
        g_string_free(con_info->body, TRUE);
        con_info->body = NULL;
    }
    free(con_info);
}

enum MHD_Result
send_response(struct MHD_Connection* conn, const char* body, int status_code)
{
    struct MHD_Response* response;
    if (body) {
        response = MHD_create_response_from_data(strlen(body), (void*)body, MHD_NO, MHD_YES);
    } else {
        response = MHD_create_response_from_data(0, NULL, MHD_NO, MHD_YES);
    }

    if (!response) {
        return MHD_NO;
    }

    int ret = MHD_queue_response(conn, status_code, response);
    MHD_destroy_response(response);

    return ret;
}

enum MHD_Result
connection_cb(void* cls, struct MHD_Connection* conn, const char* url, const char* method, const char* version,
    const char* data, size_t* size, void** con_cls)
{
#ifdef PLATFORM_OSX
    pthread_setname_np("http");
#else
    prctl(PR_SET_NAME, "http");
#endif

    if (*con_cls == NULL) {
        ConnectionInfo *con_info = create_connection_info();

        if (g_strcmp0(method, "POST") == 0 && g_strcmp0(url, "/send") == 0) {
            con_info->stbbr_op = STBBR_OP_SEND;
        } else if (g_strcmp0(method, "POST") == 0 && g_strcmp0(url, "/for") == 0) {
            con_info->stbbr_op = STBBR_OP_FOR;
        } else if (g_strcmp0(method, "POST") == 0 && g_strcmp0(url, "/verify") == 0) {
            con_info->stbbr_op = STBBR_OP_VERIFY;
        } else {
            con_info->stbbr_op = STBBR_OP_UNKNOWN;
            return send_response(conn, NULL, MHD_HTTP_BAD_REQUEST);
        }

        *con_cls = (void*) con_info;

        return MHD_YES;
    }

    ConnectionInfo *con_info = (ConnectionInfo*) *con_cls;

    if (*size != 0) {
        g_string_append_len(con_info->body, data, *size);
        *size = 0;

        return MHD_YES;
    }

    const char *id = NULL;
    const char *query = NULL;
    int res = 0;

    switch (con_info->stbbr_op) {
        case STBBR_OP_SEND:
            server_send(con_info->body->str);

            return send_response(conn, NULL, MHD_HTTP_OK);
        case STBBR_OP_FOR:
            id = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "id");
            query = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "query");
            if (id && query) {
                return send_response(conn, NULL, MHD_HTTP_BAD_REQUEST);
            }

            if (id) {
                prime_for_id(id, con_info->body->str);
                return send_response(conn, NULL, MHD_HTTP_CREATED);
            }

            if (query) {
                prime_for_query(query, con_info->body->str);
                return send_response(conn, NULL, MHD_HTTP_CREATED);
            }

            return send_response(conn, NULL, MHD_HTTP_BAD_REQUEST);
        case STBBR_OP_VERIFY:
            res = verify_any(con_info->body->str, TRUE);
            if (res) {
                return send_response(conn, "true", MHD_HTTP_OK);
            } else {
                return send_response(conn, "false", MHD_HTTP_OK);
            }
        default:
            return send_response(conn, NULL, MHD_HTTP_BAD_REQUEST);
    }
}

void
request_completed(void* cls, struct MHD_Connection* conn, void** con_cls, enum MHD_RequestTerminationCode termcode)
{
    ConnectionInfo *con_info = (ConnectionInfo*) *con_cls;
    if (con_info) {
        destroy_connection_info(con_info);
    }
    con_info = NULL;
    *con_cls = NULL;
}

int
httpapi_start(int port)
{
    httpdaemmon = MHD_start_daemon(
        MHD_USE_SELECT_INTERNALLY,
        port,
        NULL,
        NULL,
        &connection_cb,
        NULL,
        MHD_OPTION_NOTIFY_COMPLETED,
        request_completed,
        NULL, MHD_OPTION_END);

    if (!httpdaemmon) {
        return 0;
    }

    log_println(STBBR_LOGINFO, "HTTP daemon started on port: %d", port);

    return 1;
}

void
httpapi_stop(void)
{
    MHD_stop_daemon(httpdaemmon);
    log_println(STBBR_LOGINFO, "HTTP daemon stopped.");
}
