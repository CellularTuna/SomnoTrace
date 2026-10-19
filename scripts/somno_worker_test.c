/*
 * SomnoTrace - SomnoStage worker allocation and lifecycle regression tests
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
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "somno_ml_features.h"
int64_t host_now_us = 1000000;
static int created, destroyed;
somno_feat_t *__real_somno_feat_create(double dt, int motion);
somno_feat_t *__wrap_somno_feat_create(double dt, int motion)
{
    created++;
    return __real_somno_feat_create(dt, motion);
}
void __real_somno_feat_destroy(somno_feat_t *fe);
void __wrap_somno_feat_destroy(somno_feat_t *fe)
{
    if (fe) destroyed++;
    __real_somno_feat_destroy(fe);
}
#include "somno_ml_worker.c"

bool somno_ml_available(void) { return true; }
const char *somno_ml_model_semver(void) { return "host-test"; }
const somno_model_t *somno_ml_get_model(void) { return NULL; }
void somno_ml_predict(const somno_model_t *m, const float *features, double out[4])
{
    (void)m; (void)features;
    out[0] = 0.1; out[1] = 0.7; out[2] = 0.1; out[3] = 0.1;
}
int somno_ml_decode_night(const somno_model_t *m, const double *probs, int n,
                          uint8_t *stages, double *post)
{
    (void)m;
    for (int i = 0; i < n; i++) {
        stages[i] = 1;
        if (post) memcpy(post + i * 4, probs + i * 4, 4 * sizeof(double));
    }
    return 0;
}
QueueHandle_t xQueueCreate(UBaseType_t n, UBaseType_t size)
{
    (void)n; (void)size; return (void *)1;
}
BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t ticks)
{
    (void)q; (void)item; (void)ticks; return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t ticks)
{
    (void)q; (void)item; (void)ticks; return pdFALSE;
}
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) { (void)q; return 0; }
TaskHandle_t xTaskCreateStaticPinnedToCore(TaskFunction_t fn, const char *name,
    uint32_t size, void *arg, UBaseType_t priority, StackType_t *stack,
    StaticTask_t *tcb, BaseType_t core)
{
    (void)fn; (void)name; (void)arg; (void)priority; (void)core;
    assert(size >= 24 * 1024);
    free(stack); free(tcb);
    return (void *)1;
}
static void fixture(const char *dir, int index, ring_rec_t *rec)
{
    char path[400];
    snprintf(rec->dir, sizeof(rec->dir), "%s/%d", dir, index);
    snprintf(rec->id, sizeof(rec->id), "fixture%d", index);
    assert(mkdir(rec->dir, 0700) == 0);
    snprintf(path, sizeof(path), "%s/recording.json", rec->dir);
    FILE *f = fopen(path, "w"); assert(f);
    fputs("{\"state\":\"ready\",\"active_generation\":1}", f); fclose(f);
    snprintf(path, sizeof(path), "%s/generations", rec->dir); assert(mkdir(path,0700)==0);
    snprintf(path, sizeof(path), "%s/generations/1", rec->dir); assert(mkdir(path,0700)==0);
    snprintf(path, sizeof(path), "%s/generations/1/data", rec->dir); assert(mkdir(path,0700)==0);
    snprintf(path, sizeof(path), "%s/generations/1/data/vitals.snt", rec->dir);
    f = fopen(path, "wb"); assert(f);
    uint8_t hdr[64] = {0};
    uint32_t magic = 0x33544e53u, period = 1000000, denominator = 1, count = 60;
    uint16_t header_size = 64;
    int64_t start = 1893456000000LL + index * 60000;
    memcpy(hdr, &magic, 4); hdr[4] = 3; hdr[7] = 5; hdr[8] = 2;
    memcpy(hdr + 10, &header_size, 2); memcpy(hdr + 12, &period, 4);
    memcpy(hdr + 16, &denominator, 4); memcpy(hdr + 20, &start, 8);
    memcpy(hdr + 28, &count, 4);
    assert(fwrite(hdr, 1, 64, f) == 64);
    int16_t sample[5] = {9700, 6500, 0, 0, 1};
    for (int i = 0; i < 60; i++) assert(fwrite(sample, 2, 5, f) == 5);
    fclose(f);
}
static void cleanup(const ring_rec_t *rec)
{
    char path[400];
    const char *files[] = {"vitals.snt", "stages.sst"};
    for (unsigned i=0;i<2;i++) {
        snprintf(path,sizeof(path),"%s/generations/1/data/%s",rec->dir,files[i]);
        assert(unlink(path)==0);
    }
    snprintf(path,sizeof(path),"%s/generations/1/data",rec->dir); assert(rmdir(path)==0);
    snprintf(path,sizeof(path),"%s/generations/1",rec->dir); assert(rmdir(path)==0);
    snprintf(path,sizeof(path),"%s/generations",rec->dir); assert(rmdir(path)==0);
    snprintf(path,sizeof(path),"%s/recording.json",rec->dir); assert(unlink(path)==0);
    assert(rmdir(rec->dir)==0);
}
int main(void)
{
    char dir[] = "/tmp/somno-worker-XXXXXX";
    assert(mkdtemp(dir));
    ring_rec_t group[2] = {0};
    fixture(dir,0,&group[0]); fixture(dir,1,&group[1]);
    assert(score_ring_group(group,2,"synthetic") == 0);
    assert(created == 1 && destroyed == 1);
    created = destroyed = 0;
    for (int i=0;i<2;i++) {
        char path[400];
        snprintf(path,sizeof(path),"%s/generations/1/data/stages.sst",group[i].dir);
        assert(somno_ml_sst_validate(path,NULL,0) & SOMNO_SST_FLAG_INSUFFICIENT);
        cleanup(&group[i]);
    }
    /* 0-byte / truncated _sa2.snt from an interrupted session must be marked
     * INSUFFICIENT so reconcile stops re-enqueueing it on every boot. */
    {
        char sa2[400], sst[400];
        snprintf(sa2, sizeof(sa2), "%s/20260708_165632_sa2.snt", dir);
        snprintf(sst, sizeof(sst), "%s/20260708_165632_stages.sst", dir);
        FILE *f = fopen(sa2, "wb"); assert(f); fclose(f);
        somno_ml_job_t empty_job = { .type = SOMNO_ML_SRC_AS11_SA2 };
        strlcpy(empty_job.dir, dir, sizeof(empty_job.dir));
        strlcpy(empty_job.id, "20260708_165632", sizeof(empty_job.id));
        assert(somno_ml_score_file(&empty_job) == 0);
        assert(somno_ml_sst_validate(sst, NULL, 0) & SOMNO_SST_FLAG_INSUFFICIENT);
        assert(unlink(sa2) == 0 && unlink(sst) == 0);
    }
    assert(rmdir(dir)==0);
    somno_ml_job_t active = { .type = SOMNO_ML_SRC_AS11_SA2 };
    strlcpy(active.dir,"/synthetic/day",sizeof(active.dir));
    strlcpy(active.id,"recording",sizeof(active.id));
    somno_ml_set_as11_active(active.dir,active.id,true);
    assert(as11_is_recording(active.dir,active.id));
    assert(somno_ml_score_file(&active) == 0);
    somno_ml_set_as11_active(active.dir,"previous",false);
    assert(as11_is_recording(active.dir,active.id));
    somno_ml_set_as11_active(active.dir,active.id,false);
    assert(!as11_is_recording(active.dir,active.id));
    assert(somno_ml_score_file(&active) == -1);
    assert(score_ring_group(NULL,0,"synthetic") == -1);
    somno_ml_worker_start();
    puts("somno_worker_test: allocations, active-source deferral and stack budget passed");
    return 0;
}
