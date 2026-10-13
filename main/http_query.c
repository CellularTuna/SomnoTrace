/*
 * SomnoTrace - Bounded HTTP query parsing
 * Copyright (C) 2026 Ilya Kruchinin <https://github.com/ilyakruchinin>
 *
 * This file is part of SomnoTrace.
 *
 * SomnoTrace is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * SomnoTrace is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ADDITIONAL TERM (GPLv3 Section 7(b)): Redistributions must preserve the
 * attribution "Based on SomnoTrace, originally created by Ilya Kruchinin
 * (https://github.com/ilyakruchinin)." See the NOTICE file for details.
 */
#include "http_query.h"
#include <string.h>

bool http_query_read_encoded(httpd_req_t *req, const char *key, char *out, size_t cap)
{
    if (!req || !key || !*key || !out || cap == 0) return false;
    out[0] = '\0';
    char query[512];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) return false;
    size_t klen = strlen(key);
    for (const char *p = query; *p;) {
        const char *amp = strchr(p, '&');
        size_t len = amp ? (size_t)(amp - p) : strlen(p);
        if (len > klen && strncmp(p, key, klen) == 0 && p[klen] == '=') {
            size_t n = len - klen - 1;
            if (n == 0 || n >= cap) return false;
            memcpy(out, p + klen + 1, n);
            out[n] = '\0';
            return true;
        }
        if (!amp) break;
        p = amp + 1;
    }
    return false;
}

static int hex_digit(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int http_query_decode(char *dst, const char *src, int cap)
{
    if (!dst || !src || cap < 1) return -1;
    int n = 0;
    while (*src) {
        unsigned char c = (unsigned char)*src++;
        if (c == '%') {
            if (!src[0] || !src[1]) goto invalid;
            int hi = hex_digit((unsigned char)src[0]);
            int lo = hex_digit((unsigned char)src[1]);
            if (hi < 0 || lo < 0) goto invalid;
            c = (unsigned char)((hi << 4) | lo);
            src += 2;
        } else if (c == '+') c = ' ';
        if (c == 0 || n >= cap - 1) goto invalid;
        dst[n++] = (char)c;
    }
    dst[n] = '\0';
    return n;
invalid:
    dst[0] = '\0';
    return -1;
}
