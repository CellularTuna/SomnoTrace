/*
 * SomnoTrace - rolling-window leak rate average (per-second buckets)
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

#include "leak_roll.h"

void leak_roll_reset(leak_roll_t *r)
{
    if (!r) return;
    r->roll_sum = 0.0f;
    r->sec_sum  = 0.0f;
    r->sec_mark = 0;
    r->head     = 0;
    r->n        = 0;
    r->sec_n    = 0;
}

static void ring_push(leak_roll_t *r, float bucket_mean)
{
    if (r->n < LEAK_ROLL_SECS) {
        r->n++;
    } else {
        r->roll_sum -= r->ring[r->head];
    }
    r->ring[r->head] = bucket_mean;
    r->roll_sum += bucket_mean;
    r->head = (uint16_t)((r->head + 1) % LEAK_ROLL_SECS);
}

void leak_roll_push(leak_roll_t *r, float v_lpm, int64_t now_us)
{
    if (!r) return;
    int64_t sec = now_us / 1000000;
    if (r->sec_n == 0) {
        r->sec_mark = sec;
    } else if (sec != r->sec_mark) {
        /* The accumulator belongs to a completed second; flush it.
         * Silent seconds in between produce no bucket (see header). */
        ring_push(r, r->sec_sum / (float)r->sec_n);
        r->sec_sum  = 0.0f;
        r->sec_n    = 0;
        r->sec_mark = sec;
    }
    r->sec_sum += v_lpm;
    r->sec_n++;
}

bool leak_roll_ready(const leak_roll_t *r)
{
    return r && r->n >= LEAK_ROLL_SECS;
}

float leak_roll_avg(const leak_roll_t *r)
{
    if (!r || r->n == 0) return 0.0f;
    return r->roll_sum / (float)r->n;
}
