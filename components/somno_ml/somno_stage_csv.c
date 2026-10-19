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

#include "somno_stage_csv.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* Sanity ceiling for a sidecar's epoch count: a 24 h recording at the
 * shortest possible 30 s grid is 2880 epochs; anything beyond this is a
 * corrupt header, not a hypnogram. */
#define SST_MAX_EPOCHS   100000u

/* OSCAR's Zeo loader steps the Detailed Sleep Graph in fixed 30 s slots. */
#define ZEO_EPOCH_SEC    30u
/* Zeo "Sleep Graph" column granularity: one code per 5 minutes. */
#define ZEO_SG_GROUP     10u   /* 10 x 30 s */

static bool sst_usable(const somno_sst_header_t *hdr)
{
    return hdr && hdr->magic == SOMNO_SST_MAGIC &&
           hdr->version == SOMNO_SST_VERSION &&
           !(hdr->flags & (SOMNO_SST_FLAG_INSUFFICIENT | SOMNO_SST_FLAG_PARTIAL)) &&
           hdr->n_epochs > 0 && hdr->n_epochs <= SST_MAX_EPOCHS &&
           hdr->epoch_sec > 0 && hdr->epoch_sec <= 3600 &&
           hdr->start_ms > 0;
}

/* Which source epoch owns the middle of 30-second output slot k.
 * For epoch_sec == 30 this is the identity map k -> k. */
static uint32_t zeo_src_idx(uint32_t k, uint32_t epoch_sec, uint32_t n_epochs)
{
    uint32_t idx = (uint32_t)(((uint64_t)k * ZEO_EPOCH_SEC +
                               ZEO_EPOCH_SEC / 2) / epoch_sec);
    return idx < n_epochs ? idx : n_epochs - 1;
}

static int fmt_ts(int64_t epoch_ms, char out[20])
{
    time_t sec = (time_t)(epoch_ms / 1000);
    struct tm tmv;
    if (!localtime_r(&sec, &tmv)) return -1;
    return strftime(out, 20, "%Y-%m-%d %H:%M:%S", &tmv) == 19 ? 0 : -1;
}

int somno_stage_write_zeo_csv_stream(FILE *out,
                                     const somno_sst_header_t *hdr,
                                     const uint8_t *stages)
{
    if (!out || !stages || !sst_usable(hdr)) return -1;

    const uint32_t n_epochs = hdr->n_epochs;
    const uint32_t epoch_sec = hdr->epoch_sec;
    const uint64_t total_sec = (uint64_t)n_epochs * epoch_sec;
    const uint32_t n_zeo = (uint32_t)(total_sec / ZEO_EPOCH_SEC);
    if (n_zeo == 0) return -1;

    /* Build the 30 s grid + summary counters in one pass. */
    int *zeo = malloc(n_zeo * sizeof(*zeo));
    if (!zeo) return -1;

    uint32_t c_wake = 0, c_rem = 0, c_light = 0, c_deep = 0;
    uint32_t awakenings = 0;
    int32_t first_sleep = -1;
    bool prev_sleep = false;
    for (uint32_t k = 0; k < n_zeo; k++) {
        int z = somno_stage_to_zeo(stages[zeo_src_idx(k, epoch_sec, n_epochs)]);
        zeo[k] = z;
        switch (z) {
        case 1: c_wake++; break;
        case 2: c_rem++; break;
        case 3: c_light++; break;
        case 4: c_deep++; break;
        default: break;
        }
        if (first_sleep < 0 && z >= 2 && z <= 4) first_sleep = (int32_t)k;
        if (first_sleep >= 0) {
            if (z == 1 && prev_sleep) awakenings++;
            prev_sleep = (z >= 2 && z <= 4);
        }
    }

    const int min_wake  = (int)((c_wake + 1) / 2);
    const int min_rem   = (int)((c_rem + 1) / 2);
    const int min_light = (int)((c_light + 1) / 2);
    const int min_deep  = (int)((c_deep + 1) / 2);
    const int time_to_z = first_sleep >= 0 ? (int)((first_sleep + 1) / 2) : 0;

    /* Zeo Sleep Quality approximation: rewards total/REM/deep sleep,
     * penalises wake and awakenings.  Bounded like the real 0..150 score. */
    double total_sleep_hr = (min_light + min_deep + min_rem) / 60.0;
    int zq = (int)lrint((total_sleep_hr + min_rem / 120.0 +
                         min_deep * 1.5 / 60.0 - min_wake / 120.0 -
                         awakenings / 15.0) * 8.5);
    if (zq < 0) zq = 0;
    if (zq > 150) zq = 150;

    char t_start[20], t_end[20];
    if (fmt_ts(hdr->start_ms, t_start) != 0 ||
        fmt_ts(hdr->start_ms + (int64_t)total_sec * 1000, t_end) != 0) {
        free(zeo);
        return -1;
    }

    int rc = -1;
    if (fprintf(out,
            "Start of Night,End of Night,Rise Time,ZQ,Time to Z,"
            "Time in Wake,Time in REM,Time in Light,Time in Deep,"
            "Awakenings,Morning Feel,Detailed Sleep Graph,Sleep Graph\n") < 0)
        goto done;
    if (fprintf(out, "%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,0,\"",
                t_start, t_end, t_end, zq, time_to_z, min_wake, min_rem,
                min_light, min_deep, (int)awakenings) < 0)
        goto done;
    for (uint32_t k = 0; k < n_zeo; k++)
        if (fprintf(out, "%s%d", k ? " " : "", zeo[k]) < 0) goto done;

    /* 5-minute mode column: majority non-empty code per group of 10 slots. */
    if (fprintf(out, "\",\"") < 0) goto done;
    for (uint32_t g = 0; g < n_zeo; g += ZEO_SG_GROUP) {
        int votes[5] = {0};
        uint32_t end = g + ZEO_SG_GROUP;
        if (end > n_zeo) end = n_zeo;
        for (uint32_t k = g; k < end; k++) votes[zeo[k]]++;
        int best = 0;
        for (int c = 1; c <= 4; c++) if (votes[c] > votes[best]) best = c;
        if (fprintf(out, "%s%d", g ? " " : "", best) < 0) goto done;
    }
    if (fprintf(out, "\"\n") < 0 || ferror(out)) goto done;
    rc = 0;

done:
    free(zeo);
    return rc;
}

/* mkdir -p for every parent directory of path (the basename itself is the
 * output file, not a directory).  EEXIST everywhere is fine. */
static void mk_parents(const char *path)
{
    char dir[512];
    for (const char *s = path; (s = strchr(s, '/')); s++) {
        size_t len = (size_t)(s - path);
        if (len == 0 || len >= sizeof(dir)) continue;
        memcpy(dir, path, len);
        dir[len] = '\0';
        mkdir(dir, 0775);
    }
}

int somno_stage_write_zeo_csv_file(const char *csv_path,
                                   const somno_sst_header_t *hdr,
                                   const uint8_t *stages)
{
    if (!csv_path || !sst_usable(hdr) || !stages) return -1;
    mk_parents(csv_path);

    char tmp[600];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", csv_path) >= (int)sizeof(tmp))
        return -1;
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    int ok = somno_stage_write_zeo_csv_stream(f, hdr, stages) == 0;
    if (ok && fflush(f) != 0) ok = false;
    if (fclose(f) != 0) ok = false;
    if (!ok) { unlink(tmp); return -1; }
    /* FATFS cannot rename over an existing file — remove first. */
    if (rename(tmp, csv_path) != 0) {
        unlink(csv_path);
        if (rename(tmp, csv_path) != 0) { unlink(tmp); return -1; }
    }
    return 0;
}

int somno_stage_export_csv_from_sst(const char *sst_path, const char *csv_path)
{
    if (!sst_path || !csv_path) return -1;
    FILE *f = fopen(sst_path, "rb");
    if (!f) return 1;   /* nothing to export — treated as "skip" */

    somno_sst_header_t hdr;
    int rc = -1;
    uint8_t *stages = NULL;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) { fclose(f); return 1; }
    if (hdr.magic != SOMNO_SST_MAGIC || hdr.version != SOMNO_SST_VERSION ||
        hdr.n_epochs > SST_MAX_EPOCHS) { fclose(f); return 1; }
    if (hdr.flags & (SOMNO_SST_FLAG_INSUFFICIENT | SOMNO_SST_FLAG_PARTIAL) ||
        hdr.n_epochs == 0) { fclose(f); return 1; }

    /* SST1 layout is exactly 64-byte header + stages[n] + conf[n]. */
    if (fseek(f, 0, SEEK_END) != 0 ||
        ftell(f) != (long)(sizeof(hdr) + 2u * hdr.n_epochs)) {
        fclose(f);
        return 1;   /* truncated or foreign layout — skip, do not guess */
    }
    if (fseek(f, (long)sizeof(hdr), SEEK_SET) != 0) { fclose(f); return -1; }
    stages = malloc(hdr.n_epochs);
    if (!stages) { fclose(f); return -1; }
    if (fread(stages, 1, hdr.n_epochs, f) != hdr.n_epochs) goto out;
    fclose(f);
    f = NULL;

    rc = sst_usable(&hdr)
         ? (somno_stage_write_zeo_csv_file(csv_path, &hdr, stages) == 0 ? 0 : -1)
         : 1;

out:
    free(stages);
    if (f) fclose(f);
    return rc;
}
