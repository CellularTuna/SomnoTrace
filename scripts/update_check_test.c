/*
 * SomnoTrace - host tests for the update-check pure helpers
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

#include <stdio.h>
#include <string.h>

static int n_pass, n_fail;

static void ok(const char *what, int cond)
{
    if (cond) { n_pass++; } else { n_fail++; printf("  FAIL %s\n", what); }
}

static void eq(const char *what, long got, long want)
{
    if (got == want) { n_pass++; }
    else { n_fail++; printf("  FAIL %s: got %ld, want %ld\n", what, got, want); }
}

int main(void)
{
    char tag[40];

    /* ---- repo-slug validity --------------------------------------------- */
    ok("real slug", upd_slug_is_repo("ilyakruchinin/SomnoTrace"));
    ok("dotted/dashed slug", upd_slug_is_repo("some-user.name/repo_2.x"));
    ok("local build rejected", !upd_slug_is_repo("local build"));
    ok("unknown CI rejected", !upd_slug_is_repo("unknown CI build"));
    ok("empty rejected", !upd_slug_is_repo(""));
    ok("NULL rejected", !upd_slug_is_repo(NULL));
    ok("bare name rejected", !upd_slug_is_repo("SomnoTrace"));
    ok("double slash rejected", !upd_slug_is_repo("a/b/c"));
    ok("empty owner rejected", !upd_slug_is_repo("/repo"));
    ok("empty repo rejected", !upd_slug_is_repo("owner/"));
    ok("leading slash rejected", !upd_slug_is_repo("/owner/repo"));
    ok("space rejected", !upd_slug_is_repo("owner /repo"));
    ok("url rejected", !upd_slug_is_repo("https://github.com/o/r"));

    /* ---- Location → tag extraction --------------------------------------- */
    eq("full URL", upd_extract_tag(
        "https://github.com/ilyakruchinin/SomnoTrace/releases/tag/v3.2.1",
        tag, sizeof(tag)), 0);
    ok("full URL value", strcmp(tag, "v3.2.1") == 0);

    eq("relative path", upd_extract_tag(
        "/ilyakruchinin/SomnoTrace/releases/tag/v9.9.9-rc.1", tag, sizeof(tag)), 0);
    ok("prerelease tag preserved", strcmp(tag, "v9.9.9-rc.1") == 0);

    eq("query stripped", upd_extract_tag(
        "https://github.com/o/r/releases/tag/v1.2.3?x=1#y", tag, sizeof(tag)), 0);
    ok("query value", strcmp(tag, "v1.2.3") == 0);

    eq("bare /releases is no-release", upd_extract_tag(
        "https://github.com/o/r/releases", tag, sizeof(tag)), -1);
    eq("missing marker", upd_extract_tag(
        "https://github.com/o/r/issues/1", tag, sizeof(tag)), -1);
    eq("NULL location", upd_extract_tag(NULL, tag, sizeof(tag)), -1);
    eq("empty tag tail", upd_extract_tag(
        "https://github.com/o/r/releases/tag/", tag, sizeof(tag)), -1);
    eq("tiny buffer", upd_extract_tag(
        "https://github.com/o/r/releases/tag/v3.2.1", tag, 4), -1);

    /* ---- version compare -------------------------------------------------- */
    ok("same tag: no update", !upd_is_update("v3.2.1", "v3.2.1"));
    ok("newer tag: update", upd_is_update("v3.2.1", "v3.2.0"));
    ok("older tag still flags (downgrade)", upd_is_update("v3.2.0", "v3.2.1"));
    ok("dirty local differs", upd_is_update("v3.2.1", "v3.2.1+dirty"));
    ok("dev suffix differs", upd_is_update("v3.2.1", "v3.2.1-dev+5"));
    ok("untagged fork release differs", upd_is_update("3.2.1", "v3.2.1"));
    ok("empty tag never updates", !upd_is_update("", "v3.2.0"));
    ok("NULL tag never updates", !upd_is_update(NULL, "v3.2.0"));

    printf("update_check_test: %d passed, %d failed\n", n_pass, n_fail);
    return n_fail ? 1 : 0;
}
