/*
 * SomnoTrace - host test for the rolling-window leak average (leak_roll.c)
 * Copyright (C) 2026 Ilya Kruchinin <https://github.com/ilyakruchinin>
 *
 * This file is part of SomnoTrace.
 *
 * SomnoTrace is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * SomnoTrace is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ADDITIONAL TERM (GPLv3 Section 7(b)): Redistributions must preserve the
 * attribution "Based on SomnoTrace, originally created by Ilya Kruchinin
 * (https://github.com/ilyakruchinin)." See the NOTICE file for details.
 */

#include <stdio.h>
#include <math.h>
#include "leak_roll.h"

static int checks = 0, fails = 0;

#define CHECK(cond, msg) do { \
        checks++; \
        if (!(cond)) { fails++; printf("FAILED: %s (line %d)\n", msg, __LINE__); } \
    } while (0)

#define US(s) ((int64_t)(s) * 1000000)

/* Push one sample per second for `secs` seconds at value v.
 * NOTE: a push at second N flushes the accumulator for second N-1, so N
 * pushes produce N-1 completed buckets plus one still-accumulating. */
static void push_secs(leak_roll_t *r, float v, int start_s, int secs)
{
    for (int i = 0; i < secs; i++)
        leak_roll_push(r, v, US(start_s + i));
}

static bool feq(float a, float b) { return fabsf(a - b) < 0.01f; }

int main(void)
{
    leak_roll_t r;

    /* ── Empty state ─────────────────────────────────────────────── */
    leak_roll_reset(&r);
    CHECK(!leak_roll_ready(&r), "empty window not ready");
    CHECK(feq(leak_roll_avg(&r), 0.0f), "empty avg is 0");

    /* ── Warmup: ready only at exactly LEAK_ROLL_SECS buckets ────── */
    push_secs(&r, 50.0f, 0, LEAK_ROLL_SECS - 1);        /* 118 buckets  */
    CHECK(!leak_roll_ready(&r), "118 buckets: not ready");
    leak_roll_push(&r, 50.0f, US(LEAK_ROLL_SECS - 1));  /* 119 buckets  */
    CHECK(!leak_roll_ready(&r), "119 buckets: not ready");
    leak_roll_push(&r, 50.0f, US(LEAK_ROLL_SECS));      /* 120 buckets  */
    CHECK(leak_roll_ready(&r), "120 buckets: ready");
    CHECK(feq(leak_roll_avg(&r), 50.0f), "constant input -> same avg");

    /* ── Window actually rolls ───────────────────────────────────── */
    leak_roll_reset(&r);
    push_secs(&r, 10.0f, 0, LEAK_ROLL_SECS);            /* 119 buckets  */
    leak_roll_push(&r, 10.0f, US(LEAK_ROLL_SECS));      /* 120 buckets  */
    CHECK(leak_roll_ready(&r) && feq(leak_roll_avg(&r), 10.0f),
          "window full of 10s");
    /* 121 pushes of 40 flush buckets 120..240: bucket 120 holds the
     * leftover 10 (evicts a 10), buckets 121..240 are 120 buckets of 40
     * which evict every remaining 10 — final ring is all 40. */
    push_secs(&r, 40.0f, LEAK_ROLL_SECS + 1, LEAK_ROLL_SECS + 1);
    CHECK(feq(leak_roll_avg(&r), 40.0f), "oldest buckets evicted");
    CHECK(leak_roll_ready(&r), "still ready after rollover");

    /* ── Multiple samples within one second collapse to one bucket ── */
    leak_roll_reset(&r);
    /* 5 samples of 60 in second 0, then 1 Hz at 0 — the burst must
     * count as a single bucket of 60, not five entries. */
    for (int i = 0; i < 5; i++)
        leak_roll_push(&r, 60.0f, US(0) + i * 100000);
    for (int s = 1; s <= LEAK_ROLL_SECS; s++)
        leak_roll_push(&r, 0.0f, US(s));
    /* ring: 1 bucket of 60 + (LEAK_ROLL_SECS-1) buckets of 0 */
    CHECK(leak_roll_ready(&r), "burst fills window with Hz data");
    CHECK(feq(leak_roll_avg(&r), 60.0f / LEAK_ROLL_SECS),
          "5-sample burst counts as one bucket, not five");

    /* ── Silent seconds produce no buckets (gap freezes, not dilutes) ── */
    leak_roll_reset(&r);
    push_secs(&r, 30.0f, 0, 60);                        /* 59 buckets   */
    /* 120 s BLE stall — no pushes at all. */
    push_secs(&r, 30.0f, 200, 61);                      /* 61 pushes    */
    /* push at 200 flushes the sec-59 bucket (#60); pushes 201..260
     * flush buckets 200..259 (60 more) → 120 buckets, all 30. */
    CHECK(leak_roll_ready(&r), "gap did not inject zero buckets");
    CHECK(feq(leak_roll_avg(&r), 30.0f),
          "avg unaffected by silent gap");

    /* ── Reset clears everything (ack path) ──────────────────────── */
    leak_roll_reset(&r);
    CHECK(!leak_roll_ready(&r) && feq(leak_roll_avg(&r), 0.0f),
          "reset restores warmup state");

    /* ── Threshold scenario: startup spike must not alert ────────── */
    leak_roll_reset(&r);
    push_secs(&r, 45.0f, 0, 30);                       /* 29 buckets   */
    CHECK(!leak_roll_ready(&r),
          "30 s spike cannot fire — window not full");
    push_secs(&r, 8.0f, 30, 91);                       /* settles to 8 */
    /* push at 30 flushes the sec-29 bucket (30th bucket of 45);
     * pushes 31..120 flush buckets 30..119 (90 buckets of 8).
     * ring = 30×45 + 90×8, avg = 17.25 — below a 24 threshold even
     * though the first half-minute ran at 45. */
    CHECK(leak_roll_ready(&r), "window full after settle");
    float expect = (30.0f * 45.0f + 90.0f * 8.0f) / LEAK_ROLL_SECS;
    CHECK(feq(leak_roll_avg(&r), expect),
          "ramp spike absorbed by window");

    /* ── NULL robustness ─────────────────────────────────────────── */
    leak_roll_push(NULL, 5.0f, US(0));
    CHECK(!leak_roll_ready(NULL), "NULL handled");
    CHECK(feq(leak_roll_avg(NULL), 0.0f), "NULL avg is 0");

    printf("leak_roll_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
