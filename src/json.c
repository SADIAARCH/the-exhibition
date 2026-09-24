#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "json.h"

char *json_escape(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s);
    /* worst case every char needs escaping (2x) plus quotes/null */
    char *out = malloc(len * 2 + 3);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') {
            out[j++] = '\\';
            out[j++] = c;
        } else if (c == '\n') {
            out[j++] = '\\'; out[j++] = 'n';
        } else if (c < 0x20) {
            /* skip other control chars */
        } else {
            out[j++] = c;
        }
    }
    out[j] = '\0';
    return out;
}

char *json_from_artworks(Artwork *list, int count) {
    /* Rough upper bound per item, grown if needed. */
    size_t cap = 256 * (count + 1) + 16;
    char *buf = malloc(cap);
    size_t len = 0;
    len += snprintf(buf + len, cap - len, "[");

    for (int i = 0; i < count; i++) {
        char *t = json_escape(list[i].title);
        char *ar = json_escape(list[i].artist);
        char *m = json_escape(list[i].medium);

        int needed = snprintf(NULL, 0,
            "%s{\"id\":%d,\"title\":\"%s\",\"artist\":\"%s\",\"medium\":\"%s\",\"price\":%.2f}",
            i > 0 ? "," : "", list[i].id, t, ar, m, list[i].price);

        if (len + (size_t)needed + 8 >= cap) {
            cap = (len + needed + 8) * 2;
            buf = realloc(buf, cap);
        }
        len += snprintf(buf + len, cap - len,
            "%s{\"id\":%d,\"title\":\"%s\",\"artist\":\"%s\",\"medium\":\"%s\",\"price\":%.2f}",
            i > 0 ? "," : "", list[i].id, t, ar, m, list[i].price);

        free(t); free(ar); free(m);
    }
    len += snprintf(buf + len, cap - len, "]");
    return buf;
}

/* Aggregates: count and total value per medium, plus overall totals.
   Small fixed medium set keeps this simple and fast. */
#define MAX_MEDIUMS 32

char *json_stats_from_artworks(Artwork *list, int count) {
    char mediums[MAX_MEDIUMS][50];
    int  m_count[MAX_MEDIUMS] = {0};
    double m_total[MAX_MEDIUMS] = {0};
    int  m_types = 0;

    double grand_total = 0;

    for (int i = 0; i < count; i++) {
        grand_total += list[i].price;
        int found = -1;
        for (int j = 0; j < m_types; j++) {
            if (strcmp(mediums[j], list[i].medium) == 0) { found = j; break; }
        }
        if (found == -1 && m_types < MAX_MEDIUMS) {
            found = m_types++;
            snprintf(mediums[found], sizeof(mediums[found]), "%s", list[i].medium);
        }
        if (found != -1) {
            m_count[found]++;
            m_total[found] += list[i].price;
        }
    }

    size_t cap = 128 * (m_types + 1) + 64;
    char *buf = malloc(cap);
    size_t len = 0;
    len += snprintf(buf + len, cap - len,
        "{\"total_artworks\":%d,\"total_value\":%.2f,\"by_medium\":[", count, grand_total);

    for (int j = 0; j < m_types; j++) {
        char *me = json_escape(mediums[j]);
        int needed = snprintf(NULL, 0,
            "%s{\"medium\":\"%s\",\"count\":%d,\"total_price\":%.2f}",
            j > 0 ? "," : "", me, m_count[j], m_total[j]);
        if (len + (size_t)needed + 8 >= cap) {
            cap = (len + needed + 8) * 2;
            buf = realloc(buf, cap);
        }
        len += snprintf(buf + len, cap - len,
            "%s{\"medium\":\"%s\",\"count\":%d,\"total_price\":%.2f}",
            j > 0 ? "," : "", me, m_count[j], m_total[j]);
        free(me);
    }
    len += snprintf(buf + len, cap - len, "]}");
    return buf;
}

/* Extracts the string value for "key" from a flat JSON object body. Returns 0 on success. */
static int extract_string(const char *body, const char *key, char *out, size_t out_size) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(body, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return -1;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\' && *(p + 1)) p++; /* skip escape char, copy next literally */
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 0;
}

static int extract_number(const char *body, const char *key, double *out) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(body, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p == ' ' || *p == ':') p++;
    char *end;
    double v = strtod(p, &end);
    if (end == p) return -1;
    *out = v;
    return 0;
}

int json_parse_artwork(const char *body, Artwork *out) {
    memset(out, 0, sizeof(Artwork));
    extract_string(body, "title", out->title, sizeof(out->title));
    extract_string(body, "artist", out->artist, sizeof(out->artist));
    extract_string(body, "medium", out->medium, sizeof(out->medium));
    double price = 0;
    extract_number(body, "price", &price);
    out->price = price;
    return 0;
}
