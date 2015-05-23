#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "server/stanza.h"
#include "server/log.h"

static GList *stanzas;

XMPPStanza*
stanza_new(const char *name, const char **attributes)
{
    XMPPStanza *stanza = malloc(sizeof(XMPPStanza));
    stanza->name = strdup(name);
    stanza->content = NULL;
    stanza->children = NULL;
    stanza->attrs = NULL;
    if (attributes[0]) {
        int i;
        for (i = 0; attributes[i]; i += 2) {
            XMPPAttr *attr = malloc(sizeof(XMPPAttr));
            attr->name = strdup(attributes[i]);
            attr->value = strdup(attributes[i+1]);
            stanza->attrs = g_list_append(stanza->attrs, attr);
        }
    }

    return stanza;
}

void
stanza_show(XMPPStanza *stanza)
{
    log_println("NAME   : %s", stanza->name);

    if (stanza->content && stanza->content->len > 0) {
        log_println("CONTENT: %s", stanza->content->str);
    }

    if (stanza->attrs) {
        GList *curr_attr = stanza->attrs;
        while (curr_attr) {
            XMPPAttr *attr = curr_attr->data;
            log_println("ATTR   : %s='%s'", attr->name, attr->value);

            curr_attr = g_list_next(curr_attr);
        }
    }

    if (stanza->children) {
        log_println("CHILDREN:");
        GList *curr_child = stanza->children;
        while (curr_child) {
            XMPPStanza *child = curr_child->data;
            stanza_show(child);
            curr_child = g_list_next(curr_child);
        }
    }
}

void
stanza_show_all(void)
{
    GList *curr = stanzas;
    while (curr) {
        XMPPStanza *stanza = curr->data;
        stanza_show(stanza);
        log_println("");
        curr = g_list_next(curr);
    }
}

void
stanza_add(XMPPStanza *stanza)
{
    stanzas = g_list_append(stanzas, stanza);
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

const char *
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

static void
_attrs_free(XMPPAttr *attr)
{
    if (attr) {
        free(attr->name);
        free(attr->value);
        free(attr);
    }
}

static void
_stanza_free(XMPPStanza *stanza)
{
    if (stanza) {
        free(stanza->name);
        if (stanza->content) {
            g_string_free(stanza->content, TRUE);
        }
        g_list_free_full(stanza->attrs, (GDestroyNotify)_attrs_free);
        g_list_free_full(stanza->children, (GDestroyNotify)_stanza_free);
        free(stanza);
    }
}

void
stanza_free_all(void)
{
    g_list_free_full(stanzas, (GDestroyNotify)_stanza_free);
    stanzas = NULL;
}
