/*
 * stanza.c
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

#include <string.h>
#include <expat.h>
#include <glib.h>

#include "server/stanza.h"
#include "server/stanzas.h"

typedef struct parse_state_t {
    int depth;
    XMPPStanza *curr_stanza;
} ParseState;

static void _start_element(void *data, const char *element, const char **attributes);
static void _end_element(void *data, const char *element);
static void _handle_data(void *data, const char *content, int length);
static void _attrs_free(XMPPAttr *attr);

XMPPStanza*
stanza_new(const char *name, const char **attributes)
{
    XMPPStanza *stanza = malloc(sizeof(XMPPStanza));
    stanza->name = strdup(name);
    stanza->content = NULL;
    stanza->children = NULL;
    stanza->attrs = NULL;
    int i;
    for (i = 0; attributes[i]; i += 2) {
        XMPPAttr *attr = malloc(sizeof(XMPPAttr));
        attr->name = strdup(attributes[i]);
        attr->value = strdup(attributes[i+1]);
        stanza->attrs = g_list_append(stanza->attrs, attr);
    }

    return stanza;
}

char*
stanza_to_string(XMPPStanza *stanza)
{
    GString *stanza_str = g_string_new("<");

    g_string_append(stanza_str, stanza->name);

    if (stanza->attrs) {
        GList *curr = stanza->attrs;
        while (curr) {
            XMPPAttr *attr = curr->data;
            g_string_append(stanza_str, " ");
            g_string_append(stanza_str, attr->name);
            g_string_append(stanza_str, "=\"");
            g_string_append(stanza_str, attr->value);
            g_string_append(stanza_str, "\"");

            curr = g_list_next(curr);
        }
    }

    if (stanza->content) {
        g_string_append(stanza_str, ">");
        g_string_append(stanza_str, stanza->content->str);
        g_string_append(stanza_str, "</");
        g_string_append(stanza_str, stanza->name);
        g_string_append(stanza_str, ">");
    } else if (stanza->children) {
        g_string_append(stanza_str, ">");

        GList *curr = stanza->children;
        while (curr) {
            XMPPStanza *child = curr->data;
            char *child_str = stanza_to_string(child);
            g_string_append(stanza_str, child_str);
            free(child_str);
            curr = g_list_next(curr);
        }

        g_string_append(stanza_str, "</");
        g_string_append(stanza_str, stanza->name);
        g_string_append(stanza_str, ">");
    } else {
        g_string_append(stanza_str, "/>");
    }

    char *result = stanza_str->str;
    g_string_free(stanza_str, FALSE);

    return result;
}

void
stanza_add_child(XMPPStanza *parent, XMPPStanza *child)
{
    parent->children = g_list_append(parent->children, child);
}

XMPPStanza*
stanza_get_child_by_ns(XMPPStanza *stanza, char *ns)
{
    if (!stanza) {
        return NULL;
    }

    if (!stanza->children) {
        return NULL;
    }

    GList *curr_child = stanza->children;
    while (curr_child) {
        XMPPStanza *child = curr_child->data;
        if (!child->attrs) {
            return NULL;
        }
        GList *curr_attr = child->attrs;
        while (curr_attr) {
            XMPPAttr *attr = curr_attr->data;
            if ((g_strcmp0(attr->name, "xmlns") == 0) && (g_strcmp0(attr->value, ns) == 0)) {
                return child;
            }

            curr_attr = g_list_next(curr_attr);
        }

        curr_child = g_list_next(curr_child);
    }

    return NULL;
}

XMPPStanza*
stanza_get_child_by_name(XMPPStanza *stanza, char *name)
{
    if (!stanza) {
        return NULL;
    }

    if (!stanza->children) {
        return NULL;
    }

    GList *curr_child = stanza->children;
    while (curr_child) {
        XMPPStanza *child = curr_child->data;
        if (g_strcmp0(child->name, name) == 0) {
            return child;
        }

        curr_child = g_list_next(curr_child);
    }

    return NULL;
}

const char*
stanza_get_id(XMPPStanza *stanza)
{
    if (!stanza) {
        return NULL;
    }

    if (!stanza->attrs) {
        return NULL;
    }

    GList *curr_attr = stanza->attrs;
    while (curr_attr) {
        XMPPAttr *attr = curr_attr->data;
        if (g_strcmp0(attr->name, "id") == 0) {
            return attr->value;
        }

        curr_attr = g_list_next(curr_attr);
    }

    return NULL;
}

void
stanza_set_id(XMPPStanza *stanza, const char *id)
{
    GList *curr_attr = stanza->attrs;
    while (curr_attr) {
        XMPPAttr *attr = curr_attr->data;
        if (g_strcmp0(attr->name, "id") == 0) {
            free(attr->value);
            attr->value = strdup(id);
            return;
        }

        curr_attr = g_list_next(curr_attr);
    }

    XMPPAttr *attrnew = malloc(sizeof(XMPPAttr));
    attrnew->name = strdup("id");
    attrnew->value = strdup(id);
    stanza->attrs = g_list_append(stanza->attrs, attrnew);
}

const char*
stanza_get_attr(XMPPStanza *stanza, const char *name)
{
    if (!stanza->attrs) {
        return NULL;
    }

    GList *curr_attr = stanza->attrs;
    while (curr_attr) {
        XMPPAttr *attr = curr_attr->data;
        if (g_strcmp0(attr->name, name) == 0) {
            return attr->value;
        }

        curr_attr = g_list_next(curr_attr);
    }

    return NULL;
}

const char*
stanza_get_query_request(XMPPStanza *stanza)
{
    if (g_strcmp0(stanza->name, "iq") != 0) {
        return NULL;
    }

    const char *type = stanza_get_attr(stanza, "type");
    if (g_strcmp0(type, "result") == 0) {
        return NULL;
    }

    XMPPStanza *query = stanza_get_child_by_name(stanza, "query");
    if (!query) {
        return NULL;
    }

    const char *xmlns = stanza_get_attr(query, "xmlns");
    if (!xmlns) {
        return NULL;
    }

    return xmlns;
}

void
stanza_free(XMPPStanza *stanza)
{
    if (!stanza) {
        return;
    }

    free(stanza->name);
    if (stanza->content) {
        g_string_free(stanza->content, TRUE);
    }
    g_list_free_full(stanza->attrs, (GDestroyNotify)_attrs_free);
    g_list_free_full(stanza->children, (GDestroyNotify)stanza_free);
    free(stanza);
}

XMPPStanza*
stanza_parse(char *stanza_text)
{
    ParseState *state = malloc(sizeof(ParseState));
    state->depth = 0;
    state->curr_stanza = NULL;

    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetElementHandler(parser, _start_element, _end_element);
    XML_SetCharacterDataHandler(parser, _handle_data);
    XML_SetUserData(parser, state);

    XML_Parse(parser, stanza_text, strlen(stanza_text), 0);
    XML_ParserFree(parser);

    return state->curr_stanza;
}

static void
_start_element(void *data, const char *element, const char **attributes)
{
    ParseState *state = data;

    XMPPStanza *stanza = stanza_new(element, attributes);
    if (state->depth == 0) {
        state->curr_stanza = stanza;
        state->curr_stanza->parent = NULL;
    } else {
        stanza->parent = state->curr_stanza;
        state->curr_stanza = stanza;
    }

    state->depth++;
}

static void
_end_element(void *data, const char *element)
{
    ParseState *state = data;

    state->depth--;

    if (state->depth > 0) {
        stanza_add_child(state->curr_stanza->parent, state->curr_stanza);
        state->curr_stanza = state->curr_stanza->parent;
    }
}

static void
_handle_data(void *data, const char *content, int length)
{
    ParseState *state = data;

    if (!state->curr_stanza->content) {
        state->curr_stanza->content = g_string_new("");
    }

    g_string_append_len(state->curr_stanza->content, content, length);
}

static void
_attrs_free(XMPPAttr *attr)
{
    if (!attr) {
        return;
    }

    free(attr->name);
    free(attr->value);
    free(attr);
}
