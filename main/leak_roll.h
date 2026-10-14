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

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Rolling window length in seconds.  120 s is deliberately long: transient
 * leaks from repositioning usually self-resolve within ~60 s, so only a
 * sustained high leak (mask off/dislodged) can fill the window above the
 * alert threshold. */
#define LEAK_ROLL_SECS 120

/* Per-second-bucketed rolling mean.
 *
 * Raw samples arrive at whatever rate the AS11 reports leak at, and the
 * Leak array may carry several samples per notification.  Sampling every
 * sample into a count-based ring would silently change the window duration
 * if that rate ever varied, so samples are first collapsed into 1-second
 * means ("buckets") keyed on the monotonic second, and the ring holds
 * LEAK_ROLL_SECS of those buckets.  Seconds with no samples (BLE stall)
 * produce no bucket rather than a zero — a data gap freezes the window
 * instead of diluting it toward a false "leak fixed" reading.
 *
 * leak_roll_push() is O(1) amortised: one bucket flush per second boundary.
 * Memory: LEAK_ROLL_SECS * sizeof(float) for the ring plus scalars.
 *
 * "ready" means the ring is completely full — i.e. LEAK_ROLL_SECS seconds
 * of data have accumulated since the last reset.  Consumers use this as the
 * warmup gate: no alert can fire until the window is full, which suppresses
 * the mask-settling/ramp leak spike at therapy start.
 */
typedef struct {
    float    ring[LEAK_ROLL_SECS]; /* one mean per completed second   */
    float    roll_sum;             /* running sum of ring contents    */
    float    sec_sum;              /* accumulator for current second  */
    int64_t  sec_mark;             /* monotonic second of accumulator */
    uint16_t head;                 /* next ring write slot            */
    uint16_t n;                    /* filled ring slots (≤ SECS)      */
    uint16_t sec_n;                /* samples in current second       */
} leak_roll_t;

/* Clear all state (therapy start, or alert-acknowledge grace reset). */
void  leak_roll_reset(leak_roll_t *r);

/* Push one leak sample.  now_us is a monotonic timestamp in microseconds
 * (esp_timer_get_time() on target, synthetic in tests). */
void  leak_roll_push(leak_roll_t *r, float v_lpm, int64_t now_us);

/* True once the ring holds a full LEAK_ROLL_SECS window of data. */
bool  leak_roll_ready(const leak_roll_t *r);

/* Rolling average (L/min).  Returns 0 when no buckets exist yet. */
float leak_roll_avg(const leak_roll_t *r);
