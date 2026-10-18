/*
 * SomnoTrace - SomnoStage CSV upload discovery and state
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

/* Tracks the consumer-facing Zeo CSV files that somno_ml exports into
 * .somnotrace/stages/<YYYYMMDD>/<unit_id>.csv — one unit per scored
 * recording (ring) or session (AS11).  A stage unit exists only after its
 * source was scored, so stages arrive later than CPAP/oximetry data and are
 * tracked independently: rescoring changes the CSV fingerprint, which
 * resets every backend's unit to pending without touching CPAP or ox state.
 *
 * Modelled on upload_ox: grow-on-demand state list, one JSON file per unit
 * under upload_state/stage/, backends keyed by name.  Backends that cannot
 * consume stages (supports_stage == false) simply leave their units
 * pending — enabling the capability later backfills automatically. */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "upload_index.h"

/* Bounds the ref array returned per pass — same trade-off as oximetry:
 * exceeding this only hides the oldest units from one pass, the persisted
 * state itself is unbounded. */
#define UPLOAD_STAGE_MAX_UNITS 128
#define UPLOAD_STAGE_ID_LEN    64
#define UPLOAD_STAGE_PATH_LEN  512

/* One stage unit as it exists on the card, ready to hand to a backend.
 * csv_path is the Zeo-compatible export; sst_path locates the internal
 * SST1 sidecar for future backends that want the native format (empty for
 * ring units whose generation could not be resolved, and is not part of the
 * tracking identity — fingerprint changes alone re-upload). */
typedef struct {
    char     day[12];                         /* YYYYMMDD folder          */
    char     unit_id[UPLOAD_STAGE_ID_LEN];    /* <rec_id> | as11_<prefix> */
    char     csv_path[UPLOAD_STAGE_PATH_LEN];
    char     sst_path[UPLOAD_STAGE_PATH_LEN];
    uint64_t fingerprint;
} upload_stage_ref_t;

esp_err_t upload_stage_init(void);
int       upload_stage_scan(upload_stage_ref_t *out, int max_out);
int       upload_stage_reconcile(upload_stage_ref_t *out, int max_out,
                                 int max_days);
int       upload_stage_status(const upload_stage_ref_t *ref, int backend_slot);
void      upload_stage_mark(const upload_stage_ref_t *ref, int backend_slot,
                            upload_unit_status_t status, const char *remote_id);
int       upload_stage_pending(const upload_stage_ref_t *refs, int n_refs,
                               int backend_slot);
int       upload_stage_cached_pending(int backend_slot);
void      upload_stage_backend_progress(int backend_slot, int max_days,
                                        int *out_done, int *out_total);
/* "Reset Upload State" hook: clears on-disk unit files AND the in-memory
 * list so every stage CSV inside the window is re-offered (#317 parity). */
esp_err_t upload_stage_clear(void);
