/**
 * vimb - a webkit based vim like browser.
 *
 * Copyright (C) 2012-2013 Daniel Carl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.
 */

#include "main.h"
#include "bookmark.h"
#include "util.h"

extern VbCore vb;

typedef struct {
    char *uri;
    char *title;
    char **tags;
} Bookmark;

static GList *load(const char *file);
static Bookmark *line_to_bookmark(const char *line);
static int bookmark_comp(Bookmark *a, Bookmark *b);
static void free_bookmark(Bookmark *bm);

/**
 * Write a new bookmark entry to the end of bookmark file.
 */
gboolean bookmark_add(const char *uri, const char *title, const char *tags)
{
    FILE *f;

    if ((f = fopen(vb.files[FILES_BOOKMARK], "a+"))) {
        file_lock_set(fileno(f), F_WRLCK);

        if (tags) {
            fprintf(f, "%s\t%s\t%s\n", uri, title ? title : "", tags);
        } else if (title) {
            fprintf(f, "%s\t%s\n", uri, title);
        } else {
            fprintf(f, "%s\n", uri);
        }

        file_lock_set(fileno(f), F_UNLCK);
        fclose(f);

        return true;
    }
    return false;
}

gboolean bookmark_remove(const char *uri)
{
    char **lines, *line, *p;
    int len, i;
    GString *new;
    gboolean removed = false;

    if (!uri) {
        return false;
    }

    lines = util_get_lines(vb.files[FILES_BOOKMARK]);
    if (lines) {
        new = g_string_new(NULL);
        len = g_strv_length(lines) - 1;
        for (i = 0; i < len; i++) {
            line = lines[i];
            g_strstrip(line);
            /* ignore the title or bookmark tags and test only the uri */
            if ((p = strchr(line, '\t'))) {
                *p = '\0';
                if (!strcmp(uri, line)) {
                    removed = true;
                    continue;
                } else {
                    /* reappend the tags */
                    *p = '\t';
                }
            }
            if (!strcmp(uri, line)) {
                removed = true;
                continue;
            }
            g_string_append_printf(new, "%s\n", line);
        }
        g_strfreev(lines);
        g_file_set_contents(vb.files[FILES_BOOKMARK], new->str, -1, NULL);
        g_string_free(new, true);
    }

    return removed;
}

/**
 * Retrieves all bookmark uri matching the given space separated tags string.
 * Don't forget to free the returned list.
 */
GList *bookmark_get_by_tags(const char *tags)
{
    GList *res = NULL, *src = NULL;
    char **parts;
    unsigned int len;
    Bookmark *bm;

    src = load(vb.files[FILES_BOOKMARK]);
    if (!tags || *tags == '\0') {
        /* without any tags return all bookmarked items */
        for (GList *l = src; l; l = l->next) {
            bm = (Bookmark*)l->data;
            res = g_list_prepend(res, g_strdup(bm->uri));
        }
    } else {
        parts = g_strsplit(tags, " ", 0);
        len   = g_strv_length(parts);

        for (GList *l = src; l; l = l->next) {
            bm = (Bookmark*)l->data;
            if (bm->tags
                && util_array_contains_all_tags(bm->tags, g_strv_length(bm->tags), parts, len)
            ) {
                res = g_list_prepend(res, g_strdup(bm->uri));
            }
        }
        g_strfreev(parts);
    }

    g_list_free_full(src, (GDestroyNotify)free_bookmark);

    return res;
}

static GList *load(const char *file)
{
    return util_file_to_unique_list(
        file, (Util_Content_Func)line_to_bookmark, (GCompareFunc)bookmark_comp,
        (GDestroyNotify)free_bookmark
    );
}

static Bookmark *line_to_bookmark(const char *line)
{
    char **parts;
    int len;
    while (g_ascii_isspace(*line)) {
        line++;
    }
    if (*line == '\0') {
        return NULL;
    }

    Bookmark *item = g_new0(Bookmark, 1);

    parts = g_strsplit(line, "\t", 3);
    len   = g_strv_length(parts);
    if (len == 3) {
        item->tags = g_strsplit(parts[2], " ", 0);
        item->title = g_strdup(parts[1]);
        item->uri = g_strdup(parts[0]);
    } else if (len == 2) {
        item->title = g_strdup(parts[1]);
        item->uri = g_strdup(parts[0]);
    } else {
        item->uri = g_strdup(parts[0]);
    }
    g_strfreev(parts);

    return item;
}

static int bookmark_comp(Bookmark *a, Bookmark *b)
{
    return g_strcmp0(a->uri, b->uri);
}

static void free_bookmark(Bookmark *bm)
{
    g_free(bm->uri);
    g_free(bm->title);
    g_strfreev(bm->tags);
    g_free(bm);
}
