/*
 * SomnoTrace - HTTP query parsing regression tests
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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "http_query.h"

static int n_pass, n_fail;

static void check(bool cond, const char *what)
{
    if (cond) n_pass++;
    else { n_fail++; printf("  FAIL %s\n", what); }
}

static httpd_req_t request(const char *query, esp_err_t error)
{
    httpd_req_t req = { .uri = "/download", .query = query, .query_error = error };
    return req;
}

int main(void)
{
    char out[256];

    /* Regression: httpd_req_get_url_query_str returns ESP_OK on success. */
    httpd_req_t req = request("path=/somnotrace/SDCARD/STR.edf", ESP_OK);
    check(http_query_read_encoded(&req, "path", out, sizeof(out)), "ESP_OK query is read");
    check(strcmp(out, "/somnotrace/SDCARD/STR.edf") == 0, "raw value matches");

    req = request("path=/a%20b%2Fc+d", ESP_OK);
    check(http_query_read_encoded(&req, "path", out, sizeof(out)), "encoded query is read");
    check(http_query_decode(out, out, sizeof(out)) == 8 && strcmp(out, "/a b/c d") == 0,
          "percent and plus decode");

    req = request("session=20261013_055111&date=20261012&type=events", ESP_OK);
    check(http_query_read_encoded(&req, "type", out, sizeof(out)), "later key found");
    check(strcmp(out, "events") == 0, "value of later key");
    check(!http_query_read_encoded(&req, "type", out, 4), "short buffer rejected");
    check(!http_query_read_encoded(&req, "missing", out, sizeof(out)), "absent key rejected");

    req = request("path=", ESP_OK);
    check(!http_query_read_encoded(&req, "path", out, sizeof(out)), "empty value rejected");

    req = request("path=%zz%2", ESP_OK);
    check(http_query_read_encoded(&req, "path", out, sizeof(out)), "bad escapes kept raw");
    check(http_query_decode(out, out, sizeof(out)) == -1, "bad escapes rejected");

    req = request("path=/x", ESP_FAIL);
    check(!http_query_read_encoded(&req, "path", out, sizeof(out)), "API failure rejected");

    req = request("dir=/somnotrace", ESP_OK);
    check(http_query_read_encoded(&req, "dir", out, sizeof(out)), "dir key found");

    check(http_query_decode(out, "a%2Fb+c", sizeof(out)) == 5 &&
          strcmp(out, "a/b c") == 0, "decode helper");
    check(http_query_decode(out, "%00", sizeof(out)) == -1, "NUL rejected");
    check(http_query_decode(out, "abc", 3) == -1, "overflow rejected");
    check(http_query_decode(out, "", sizeof(out)) == 0, "empty decodes");

    printf("http_query_test: %d passed, %d failed\n", n_pass, n_fail);
    return n_fail ? 1 : 0;
}
