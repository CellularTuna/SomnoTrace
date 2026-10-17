/*
 * SomnoTrace - pure helpers for the GitHub update-availability check
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
#include "update_check_core.h"

#include <string.h>

bool upd_slug_is_repo(const char *slug)
{
    if (!slug || !*slug) return false;

    int slashes = 0;
    int seg_len = 0;
    for (const char *p = slug; *p; p++) {
        char c = *p;
        if (c == '/') {
            if (++slashes > 1 || seg_len == 0) return false;
            seg_len = 0;
            continue;
        }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '.' || c == '-')) {
            return false;
        }
        seg_len++;
    }
    return slashes == 1 && seg_len > 0;
}

int upd_extract_tag(const char *location, char *out, size_t out_len)
{
    static const char marker[] = "/releases/tag/";
    if (!location || !out || out_len < 2) return -1;

    const char *p = strstr(location, marker);
    if (!p) return -1;
    p += sizeof(marker) - 1;

    size_t n = 0;
    while (p[n] && p[n] != '?' && p[n] != '#') {
        if (n + 1 >= out_len) return -1;
        out[n] = p[n];
        n++;
    }
    out[n] = '\0';
    return n > 0 ? 0 : -1;
}

bool upd_is_update(const char *tag, const char *fw_ver)
{
    if (!tag || !tag[0] || !fw_ver) return false;
    return strcmp(tag, fw_ver) != 0;
}
