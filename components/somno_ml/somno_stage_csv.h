/*
 * SomnoTrace - SomnoStage sleep stage export to Zeo-compatible CSV
 * Copyright (C) 2026 Ilya Kruchinin <https://github.com/ilyakruchinin>
 *
 * This file is part of SomnoTrace.
 *
 * SomnoTrace is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3, or (at your option) any later version.
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

/* Consumer-facing sleep stage export.  The internal source of truth stays
 * the .sst sidecar (stages + confidence + model provenance); this module
 * renders a valid SST1 hypnogram as a Zeo Personal Sleep Coach CSV row —
 * the format OSCAR's "Import ZEO Data" loader reads.  Pure C + libc: no
 * ESP-IDF or FreeRTOS dependencies, so host tests link this file directly.
 *
 * Zeo has NO per-epoch duration field: OSCAR steps the Detailed Sleep Graph
 * in fixed 30 s slots.  SomnoStage epochs are also nominally 30 s, but a
 * Gen1 O2 Ring samples at 0.25 Hz, so its epoch grid is 8 x 4 s = 32 s.
 * Feeding raw 32 s epochs to a 30 s reader compresses an 8 h night to
 * 7 h 30 m and skews every stage transition against the CPAP waveform.
 * The writer therefore maps the discrete stage function onto a strict 30 s
 * grid (midpoint lookup) — transitions stay within half a slot of their
 * true timestamp and never drift with night length.  30 s sources map 1:1. */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "somno_ml.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OSCAR/Zeo Detailed Sleep Graph codes.  SomnoTrace: 0 Wake, 1 Light,
 * 2 Deep, 3 REM, >=4 missing.  Zeo: 1 Awake, 2 REM, 3 Light, 4 Deep,
 * 0 no data. */
static inline int somno_stage_to_zeo(uint8_t st)
{
    switch (st) {
    case 0:  return 1;   /* Wake  -> Awake */
    case 1:  return 3;   /* Light -> Light */
    case 2:  return 4;   /* Deep  -> Deep  */
    case 3:  return 2;   /* REM   -> REM   */
    default: return 0;   /* 0xFF etc. -> no data */
    }
}

/* Emit one complete Zeo CSV (header line + one session row) to `out`.
 * `hdr`/`stages` describe one valid SST1 hypnogram; insufficient or partial
 * sidecars are refused.  Returns 0 on success, -1 on invalid input or a
 * stream write error. */
int somno_stage_write_zeo_csv_stream(FILE *out,
                                     const somno_sst_header_t *hdr,
                                     const uint8_t *stages);

/* Atomically write the Zeo CSV for one hypnogram to `csv_path`
 * (tmp + rename; parent directories are created).  Returns 0 on success,
 * -1 on error. */
int somno_stage_write_zeo_csv_file(const char *csv_path,
                                   const somno_sst_header_t *hdr,
                                   const uint8_t *stages);

/* Read an existing .sst sidecar and write its Zeo CSV to `csv_path`
 * (atomic, parents created).  Used to backfill CSVs for .sst files that
 * predate the exporter without re-running the model.
 * Returns 0 written, 1 skipped (INSUFFICIENT/PARTIAL stub or unreadable
 * sidecar — nothing meaningful to export), -1 on I/O error. */
int somno_stage_export_csv_from_sst(const char *sst_path, const char *csv_path);

#ifdef __cplusplus
}
#endif
