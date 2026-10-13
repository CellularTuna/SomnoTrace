/*
 * SomnoTrace - Host shim for httpd_req_get_url_query_str
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
#include "esp_http_server.h"
#include <string.h>

esp_err_t httpd_req_get_url_query_str(httpd_req_t *req, char *out, size_t cap)
{
    if (!req || !out || cap == 0) return ESP_ERR_INVALID_ARG;
    if (req->query_error != ESP_OK) return req->query_error;
    if (!req->query || strlen(req->query) >= cap) return ESP_ERR_INVALID_SIZE;
    strcpy(out, req->query);
    return ESP_OK;
}
