/*
 * SomnoTrace - Minimal FreeRTOS test shim for host unit tests
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

#pragma once

#include <stdint.h>

/* Single-threaded test host: the portMUX critical sections are no-ops and
 * the semphr.h mutex calls always succeed immediately. */
typedef int      BaseType_t;
typedef uint32_t TickType_t;
typedef int      portMUX_TYPE;
typedef uint32_t UBaseType_t;
typedef uint8_t StackType_t;
typedef struct { unsigned dummy; } StaticTask_t;

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define tskIDLE_PRIORITY 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))

#define portMAX_DELAY               ((TickType_t)0xffffffffu)
#define portMUX_INITIALIZER_UNLOCKED 0

#define taskENTER_CRITICAL(mux)     (void)(mux)
#define taskEXIT_CRITICAL(mux)      (void)(mux)
