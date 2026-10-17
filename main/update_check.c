/*
 * SomnoTrace - periodic GitHub release check (LCD update badge)
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
#include "update_check.h"
#include "update_check_core.h"
#include "bsp_display.h"
#include "net_provision.h"
#include "psram_task.h"

#include <string.h>
#include <strings.h>

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "updchk";

#ifdef SNT_SOURCE_REPO
#define UPD_SOURCE_REPO SNT_SOURCE_REPO
#else
#define UPD_SOURCE_REPO ""
#endif

#define UPD_TAG_MAX   32
#define UPD_URL_MAX   96
#define UPD_LOC_MAX   256
#define UPD_FIRST_MS  (3 * 60 * 1000)        /* let NTP/boot TLS settle */
#define UPD_PERIOD_MS (60 * 60 * 1000)
#define UPD_JITTER_MS (5 * 60 * 1000)

static char  s_latest_tag[UPD_TAG_MAX];
static bool  s_update_available = false;
static const char *s_fw_ver = "";

/* Location of the 302 is the whole payload of this check — the body is 0
 * bytes. Captured via ON_HEADER into this small struct on the task stack. */
typedef struct {
    char location[UPD_LOC_MAX];
} upd_http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_HEADER && evt->user_data) {
        upd_http_ctx_t *ctx = evt->user_data;
        if (strcasecmp(evt->header_key, "Location") == 0) {
            strlcpy(ctx->location, evt->header_value, sizeof(ctx->location));
        }
    }
    return ESP_OK;
}

static void apply_state(bool avail, const char *tag)
{
    s_update_available = avail;
    bsp_display_set_update_available(avail);
    if (tag) {
        strlcpy(s_latest_tag, tag, sizeof(s_latest_tag));
    }
}

static void do_check(void)
{
    char url[UPD_URL_MAX];
    snprintf(url, sizeof(url),
             "https://github.com/%s/releases/latest", UPD_SOURCE_REPO);

    /* The check runs on github.com's own redirect, not the REST API:
     * the 302's Location header carries the tag, the body is empty, and no
     * API rate-limit budget is consumed. disable_auto_redirect is essential
     * or esp_http_client would chase the redirect into the release HTML. */
    upd_http_ctx_t ctx = {0};
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = true,
        .timeout_ms = 15000,
        .event_handler = http_event_handler,
        .user_data = &ctx,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGW(TAG, "http init failed");
        return;
    }
    esp_http_client_set_header(client, "User-Agent", "somnotrace");
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    /* Failures preserve last-known state: filtered internet, captive
     * portals and GitHub hiccups must not flicker the badge off. */
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "check failed: %s", esp_err_to_name(err));
        return;
    }
    if (status != 302) {
        ESP_LOGD(TAG, "check: unexpected status %d", status);
        return;
    }

    char tag[UPD_TAG_MAX];
    if (upd_extract_tag(ctx.location, tag, sizeof(tag)) != 0) {
        /* 302 to bare /releases: the repo has no stable release at all. */
        ESP_LOGI(TAG, "no stable release on %s", UPD_SOURCE_REPO);
        apply_state(false, "");
        return;
    }

    bool avail = upd_is_update(tag, s_fw_ver);
    ESP_LOGI(TAG, "latest release: %s (running %s) — %s",
             tag, s_fw_ver, avail ? "UPDATE AVAILABLE" : "up to date");
    apply_state(avail, tag);
}

static void update_check_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(UPD_FIRST_MS));
    for (;;) {
        /* In portal (SoftAP) mode there is no upstream link; skip. */
        if (netprov_is_link_up()) {
            do_check();
        }
        uint32_t jitter = esp_random() % UPD_JITTER_MS;
        vTaskDelay(pdMS_TO_TICKS(UPD_PERIOD_MS) + pdMS_TO_TICKS(jitter));
    }
}

void update_check_init(void)
{
    if (!upd_slug_is_repo(UPD_SOURCE_REPO)) {
        ESP_LOGI(TAG, "build origin '%s' is not a repo slug — check disabled",
                 UPD_SOURCE_REPO);
        return;
    }

    const esp_app_desc_t *d = esp_app_get_description();
    s_fw_ver = (d && d->version[0]) ? d->version : "unknown";

    /* Deliberately no persistence: a cached tag could stale-paint the badge
     * after a firmware change (OTA flash leaves NVS untouched but changes
     * esp_app_desc_t.version). The badge stays dark until the first live
     * check ~3 min after boot. */
    if (!psram_task_create(update_check_task, "updchk", 6144, NULL,
                           4, tskNO_AFFINITY, NULL, NULL)) {
        ESP_LOGE(TAG, "failed to start checker task");
    }
    ESP_LOGI(TAG, "release check active for %s", UPD_SOURCE_REPO);
}

bool update_check_is_available(void)
{
    return s_update_available;
}

const char *update_check_latest_tag(void)
{
    return s_latest_tag;
}
