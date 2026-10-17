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
#pragma once

#include <stdbool.h>

/* Start the hourly release checker. No-op when the build origin
 * (SNT_SOURCE_REPO from CMakeLists) is not a real "owner/repo" slug —
 * i.e. "local build" and "unknown CI build" never phone GitHub. */
void update_check_init(void);

/* Latest check result: a different stable release exists upstream. */
bool update_check_is_available(void);

/* Latest release tag observed ("" when none seen yet). */
const char *update_check_latest_tag(void);
