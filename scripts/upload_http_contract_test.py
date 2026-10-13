#!/usr/bin/env python3
# SomnoTrace - Host regression checks for the shared multipart sender
# Copyright (C) 2026 Ilya Kruchinin <https://github.com/ilyakruchinin>
#
# This file is part of SomnoTrace.
#
# SomnoTrace is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# SomnoTrace is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <https://www.gnu.org/licenses/>.
#
# ADDITIONAL TERM (GPLv3 Section 7(b)): Redistributions must preserve the
# attribution "Based on SomnoTrace, originally created by Ilya Kruchinin
# (https://github.com/ilyakruchinin)." See the NOTICE file for details.

import os
import pathlib
import shlex
import subprocess
import tempfile

repo = pathlib.Path(__file__).resolve().parent.parent
src = (repo / "components/uploader/upload_http.c").read_text()
functions = src[src.index("static const char *uph_multipart_name("):]
preamble = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
typedef void esp_tls_t;
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOG_LEVEL(...) ((void)0)
#define MALLOC_CAP_SPIRAM 1
#define UPH_READ_CHUNK 1024
#define taskYIELD() ((void)0)
static void *heap_caps_malloc(size_t n, int caps) { (void)caps; return malloc(n); }
static unsigned esp_random(void) { return 123; }
static char wire[30000];
static size_t wire_len;
static int write_calls, fail_write_call;
static int uph_write_all(esp_tls_t *tls, const void *buf, size_t len) {
    (void)tls;
    if (++write_calls == fail_write_call) return -1;
    assert(wire_len + len < sizeof(wire));
    memcpy(wire + wire_len, buf, len);
    wire_len += len;
    wire[wire_len] = 0;
    return (int)len;
}
static int uph_read_response(esp_tls_t *tls, size_t cap, size_t bodycap,
                             char **body, size_t *len, bool *pc) {
    (void)tls; (void)cap; (void)bodycap; (void)len;
    if (body) *body = strdup("{}");
    if (pc) *pc = false;
    return 201;
}
'''
tests = r'''
static void reset(void) {
    wire_len = 0; write_calls = 0; fail_write_call = 0; wire[0] = 0;
}
static void check_length(void) {
    char *p = strstr(wire, "Content-Length: "); assert(p);
    size_t n = strtoul(p + 16, NULL, 10);
    char *body = strstr(wire, "\r\n\r\n"); assert(body); body += 4;
    assert(n == wire_len - (size_t)(body - wire));
}
static int occurrences(const char *p, const char *needle) {
    int n = 0;
    while ((p = strstr(p, needle))) { n++; p += strlen(needle); }
    return n;
}
int main(void) {
    bool pc = false; char *body = NULL;
    reset();
    assert(uph_put_files((void *)1, "example.test", "/upload", "synthetic-token",
                         NULL, NULL, 0, NULL, NULL, &body, 4096, &pc) == 201);
    check_length(); free(body);
    FILE *f = fopen("source.bin", "wb"); assert(f);
    fwrite("abc", 1, 3, f); fclose(f);
    f = fopen("b.edf", "wb"); assert(f);
    for (int i = 0; i < 2050; i++) fputc('x', f);
    fclose(f);
    const char *paths[] = {"source.bin", "b.edf"};
    reset();
    assert(uph_put_files((void *)1, "example.test", "/upload", "synthetic-token",
                         NULL, paths, 2, "vendor", "resmed", &body, 4096, &pc) == 201);
    check_length(); free(body);
    assert(occurrences(wire, "name=\"files\"") == 2);
    assert(strstr(wire, "name=\"vendor\"\r\n\r\nresmed\r\n"));
    assert(strstr(wire, "filename=\"source.bin\""));
    const char *names[] = {"20260102015549.bin", "b.edf"};
    reset();
    assert(uph_put_files_named((void *)1, "example.test", "/upload", "synthetic-token",
                               NULL, paths, 2, names, NULL, NULL, &body, 4096, &pc) == 201);
    check_length(); free(body);
    assert(strstr(wire, "filename=\"20260102015549.bin\""));
    assert(!strstr(wire, "filename=\"source.bin\""));
    assert(strstr(wire, "\r\n\r\nabc\r\n"));
    const char *bad[] = {"../a.bin", "a\\b.bin", "a\".bin", "a\r\nx.bin", ""};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        const char *unsafe[] = {bad[i]}; reset();
        assert(uph_put_files_named((void *)1, "example.test", "/upload", "synthetic-token",
                                   NULL, paths, 1, unsafe, NULL, NULL, &body, 4096, &pc) == -1);
        assert(wire_len == 0);
    }
    reset(); fail_write_call = 3;
    assert(uph_put_files((void *)1, "example.test", "/upload", "synthetic-token",
                         NULL, paths, 2, NULL, NULL, &body, 4096, &pc) == -1 && pc);
    const char *absent[] = {"missing.edf"}; reset();
    assert(uph_put_files((void *)1, "example.test", "/upload", "synthetic-token",
                         NULL, absent, 1, NULL, NULL, &body, 4096, &pc) == -1 && wire_len == 0);
    puts("upload_http_contract_test: lengths, streaming, original names, injection and socket failures passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="snt-multipart-test-") as tmp:
    source = pathlib.Path(tmp) / "multipart.c"
    source.write_text(preamble + functions + tests)
    binary = pathlib.Path(tmp) / "multipart"
    flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-g"]
    flags += shlex.split(os.environ.get("SNT_TEST_SANITIZE", ""))
    subprocess.run([os.environ.get("CC", "gcc"), *flags, str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], cwd=tmp, check=True)
